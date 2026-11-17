"""Independent hand-counted I51 evaluation fixtures; no private data."""
import importlib.util
import json
import math
import unittest
from fractions import Fraction
from pathlib import Path

spec = importlib.util.spec_from_file_location('g6', Path(__file__).resolve().parents[1] / 'docs/procedural/smoke/g6_evaluate_holdout.py')
g6 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g6)


class HoldoutTests(unittest.TestCase):
    def test_distances(self):
        self.assertAlmostEqual(g6.contribution(-1, 1, True), 1.762747174039086)
        self.assertAlmostEqual(g6.contribution(1, 0), .6931471805599453)
        for v, r in [(0, 100), (1000000, 1), (1, 1000000), (None, 1), (1, None), (None, None)]:
            self.assertEqual(g6.contribution(v, r), math.log(64))
        with self.assertRaises(ValueError):
            g6.contribution(-1, 1)

    def test_fixed_denominator(self):
        candidate = dict.fromkeys(g6.METRICS, 1)
        reference = candidate.copy()
        reference[g6.METRICS[0]] = None
        candidate[g6.METRICS[1]] = None
        self.assertAlmostEqual(g6.map_distance(candidate, reference), 2 * math.log(64) / 26)
        with self.assertRaises(KeyError):
            g6.map_distance({}, reference)
        with self.assertRaises(ValueError):
            g6.value('nan')
        with self.assertRaises(ValueError):
            g6.value('')

    def test_families(self):
        # M01=0 M05=6 winter=(4+8)/2=6; main=(0+6+6)/3=4.
        maps = dict(M01=0, M05=6, M11=4, M18=8)
        maps.update(zip(g6.RELATED, [1, 2, 3, 4, 5, 6], strict=True))
        result = g6.family_distances(maps)
        self.assertEqual(result['winter'], 6)
        self.assertEqual(result['main'], 4)
        self.assertEqual(result['M19-related'], 3.5)
        maps['M12'] = 6000
        self.assertEqual(g6.family_distances(maps)['main'], 4)

    def test_even_median_and_baseline(self):
        # Sorted 0..15 => (7+8)/2=7.5. Eight below median; one fails hard => 7/16.
        distances = list(range(16))
        hard = [True] + [False] * 15
        result = g6.pair_result(distances, hard, 7)
        self.assertEqual(result['median'], 7.5)
        self.assertTrue(result['success'])
        self.assertEqual(result['baseline'], Fraction(7, 16))
        self.assertFalse(g6.pair_result(distances, hard, 0)['success'])
        self.assertFalse(g6.pair_result(distances, hard, 8)['success'])
        ties = g6.pair_result([2] * 16, [False] * 16, 0)
        self.assertFalse(ties['success'])
        self.assertEqual(ties['baseline'], 0)
        # 0,0,1x14: median=1, strict ties lose, baseline=2/16.
        self.assertEqual(g6.pair_result([0, 0] + [1] * 14, [False] * 16, 0)['baseline'], Fraction(1, 8))
        self.assertEqual((result['baseline'] + ties['baseline']) / 2, Fraction(7, 32))

    def test_frozen_bands(self):
        for style in ('m06', 'm19'):
            bands = g6.load_bands(g6.BANDS_DIR / f'{style}.bands')
            self.assertEqual(len(bands), 26)
            self.assertEqual(bands['trans.R0'], (33, 0, 8300) if style == 'm06' else (99, 6633, 14850))
        self.assertEqual(g6.load_bands(g6.BANDS_DIR / 'm19.bands')['patch.RS'], (15, 0, 3500))

    def test_full_aggregation(self):
        # 25 constant N/A penalties plus one log ratio: k=0..5 below cap, k>=6 tied.
        # Candidate 0 fails hard, so exactly 5/16 eligible; second half seeds all tie.
        ref = dict.fromkeys(g6.METRICS)
        ref['channels.L'] = 1
        references = {m: ref for m in g6.MAIN_MAPS + g6.RELATED}
        groups, winners = {}, {}
        for seed in range(1001, 1033):
            rows = []
            for i in range(16):
                metrics = ref.copy()
                metrics['channels.L'] = 2 ** i if seed < 1017 else 1
                rows.append({'metrics': metrics, 'hard': i == 0, 'index': i})
            groups[seed], winners[seed] = rows, 1
        result = g6.evaluate(groups, winners, references)
        self.assertEqual(result['mapCoverage']['M01']['referenceKnownPositions'], 1)
        self.assertEqual(result['mapCoverage']['M01']['varyingMetrics'], ['channels.L'])
        for summary in result['summary'].values():
            self.assertEqual(summary['successes'], 16)
            self.assertEqual(summary['pairs'], 32)
            self.assertEqual(summary['randomBaselineExact'], '5/32')
        self.assertFalse(result['thresholdPassed'])

    def test_report_na_versus_technical_error(self):
        bands = g6.load_bands(g6.BANDS_DIR / 'm06.bands')
        lines = ['g6-candidate-report/1',
                 'settings\tside=512\tcount=16\tpathLength=40\tbuildingDensity=30\trockBlocks=false\tg5=true\tmode=ranking']
        lines += ['band\t' + m + '\t' + '\t'.join(map(str, bands[m])) for m in g6.METRICS]
        lines += ['userSeed\tindex\tseed\tsurfaceFingerprint\thardFail\tinBand\tdistance\t' + '\t'.join(g6.METRICS)]
        for seed in range(1001, 1033):
            for i in range(16):
                # One N/A => 25 bands and round(1000 ln64)=4159, all other terms zero.
                values = ['NA' if i == 1 and j == 0 else str(bands[m][0]) for j, m in enumerate(g6.METRICS)]
                lines.append(f'{seed}\t{i}\t{seed}\t0\tfalse\t{25 if i == 1 else 26}\t{4159 if i == 1 else 0}\t' + '\t'.join(values))
            lines.append(f'winner\t{seed}\t0\tfalse\t26\t0')
        text = '\n'.join(lines) + '\n'
        path = Path(__file__).resolve().parents[1] / 'scratchpad/g6-holdout-tests/m06-run1.tsv'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        groups, winners = g6.read_candidates(path, bands)
        self.assertEqual(len(groups), 32)
        self.assertIsNone(groups[1001][1]['metrics']['channels.L'])
        self.assertEqual(winners[1001], 0)
        for bad in (text.replace('\tchannels.L\t', '\tmissing.L\t', 1),
                    text.replace('\tNA\t', '\tgarbage\t', 1),
                    text.replace('\t25\t4159\t', '\t25\t4158\t', 1),
                    text.replace('winner\t1001\t0', 'winner\t1001\t1', 1),
                    text.replace('1001\t0\t1001', '1002\t0\t1002', 1)):
            path.write_text(bad, encoding='utf-8')
            with self.assertRaises((ValueError, KeyError)):
                g6.read_candidates(path, bands)
        path.write_text(text, encoding='utf-8')

    def test_reference_completeness_versions_and_qualification(self):
        root = Path(__file__).resolve().parents[1] / 'scratchpad/g6-reference-tests'
        profiles = root / 'profiles'
        profiles.mkdir(parents=True, exist_ok=True)
        for style in ('m06', 'm19'):
            (profiles / f'{style}.profile').write_text('g4-profile/6\nclient\t10.98\nlevel\tsurface\nfloors\t0\t7\nassets\tdat_sha256\tdat\n', encoding='utf-8')
        for n in range(1, 20):
            alias = f'M{n:02}'
            bands = g6.load_bands(g6.BANDS_DIR / ('m19.bands' if n == 19 else 'm06.bands'))
            refs = {m: str(bands[m][0]) for m in g6.METRICS}
            for bundle in g6.BUNDLES:
                d = root / bundle / alias
                run = d / 'run'
                analysis = run / 'analysis'
                analysis.mkdir(parents=True, exist_ok=True)
                (d / 'verification.json').write_text(json.dumps({'result': 'PASS', 'exit_code': 0, 'client_id': '10.98',
                    'source_hashes_unchanged': {'Tibia.dat': 'dat', 'Tibia.spr': 'spr', 'map': alias}}))
                (run / 'reference-report.json').write_text(json.dumps({'schema': 'g3-import-report/1', 'client': '10.98',
                    'compatibility': 'compatible', 'sha256': alias, 'header': {}, 'tiles': 1000}))
                (run / 'semantic-report.json').write_text(json.dumps({'schema': 'g3-semantic-report/1', 'sha256': alias}))
                (run / 'client-id.txt').write_text('10.98\n')
                (run / 'keymap.tsv').write_text('g4-keymap/1\n')
                def write(name, rows):
                    (analysis / f'{name}-surface.txt').write_text(''.join(f'{k}\t{v}\n' for k, v in rows.items()))
                write('channels', dict(count=5, length_per_64k=refs['channels.L'], width_milli=refs['channels.W'],
                                      branches_per_100_milli=refs['channels.B'], connected_permille=refs['channels.C']))
                write('scale', dict(excess_16=refs['scale.E16'], excess_64=refs['scale.E64'], pairs_64=1000))
                write('paths', dict(count=5, length_per_64k=refs['paths.L'], width_milli=refs['paths.W']))
                write('buildings', dict(count=5, per_64k=refs['build.per64k'], long_median=refs['build.long'],
                                       short_median=refs['build.short'], wall_median=refs['outline.median']))
                write('patches', dict(zip(('boundary', 'compact', 'beach', 'rock_boundary', 'rock_compact', 'rock_dust'),
                                         [refs[m] for m in g6.METRICS[6:12]], strict=True)))
                write('transitions', dict(rock_comp=' '.join(refs[f'trans.R{i}'] for i in range(4)),
                                         dirt_comp=' '.join(refs[f'trans.D{i}'] for i in range(4)), rock_pairs=1000, dirt_pairs=1000))
        refs, audit = g6.load_references(root, profiles)
        self.assertEqual(len(refs), 19)
        self.assertEqual(audit['maps']['M01']['differentiatingPositions'], 26)
        path = root / g6.BUNDLES[1] / 'M02/run/analysis/scale-surface.txt'
        original = path.read_text()
        path.unlink()
        with self.assertRaises(OSError):
            g6.load_references(root, profiles)
        path.write_text(original)
        version = root / g6.BUNDLES[2] / 'M01/run/reference-report.json'
        original = version.read_text()
        changed = json.loads(original)
        changed['header'] = {'items_minor': 999}
        version.write_text(json.dumps(changed))
        with self.assertRaises(ValueError):
            g6.load_references(root, profiles)
        version.write_text(original)
        # Keep all repeated summaries consistent while lowering one map's counts.
        for bundle in g6.BUNDLES:
            a = root / bundle / 'M01/run/analysis'
            p = a / 'channels-surface.txt'
            p.write_text(p.read_text().replace('count\t5', 'count\t0'))
            p = a / 'buildings-surface.txt'
            p.write_text(p.read_text().replace('count\t5', 'count\t1'))
        refs, audit = g6.load_references(root, profiles)
        self.assertEqual(refs['M01']['channels.L'], 0)
        self.assertIsNone(refs['M01']['channels.W'])
        self.assertIsNone(refs['M01']['build.per64k'])
        self.assertEqual(audit['maps']['M01']['differentiatingPositions'], 19)


if __name__ == '__main__':
    unittest.main()
