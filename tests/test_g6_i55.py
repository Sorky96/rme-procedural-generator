"""I55 synthetic contracts. All generated material stays in scratchpad."""
import importlib.util
import json
import math
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('i55', ROOT / 'docs/procedural/smoke/g6_i55.py')
i55 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(i55)


class I55Tests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'scratchpad').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'scratchpad', prefix='i55-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def row(self, index=0, hard=False, in_band=0, distance=10, value=1):
        return {'index': index, 'hard': hard, 'inBand': in_band, 'distance': distance,
                'metrics': dict.fromkeys(i55.g6.METRICS, value)}

    def test_l2_l3_deterministic_admissible_and_ties(self):
        rows = [self.row(3, in_band=2), self.row(1, in_band=1), self.row(0, True, 26), self.row(2, in_band=2)]
        l2, l3 = i55.selections(rows)
        self.assertEqual((l2['index'], l3['index']), (1, 2))
        self.assertEqual(i55.selections(list(reversed(rows))), (l2, l3))
        with self.assertRaises(ValueError):
            i55.selections([self.row(hard=True)])
        same = self.row()
        self.assertEqual(i55.selections([same]), (same, same))

    def test_balance_complete_seeds_unique_anonymous_ids(self):
        pairs = i55.assignments()
        i55.validate_key(pairs)
        self.assertEqual(len({p['id'] for p in pairs}), 20)
        with self.assertRaises(ValueError):
            i55.validate_key(pairs[:-1])
        pairs[0]['L3side'] = 'bad'
        with self.assertRaises(ValueError):
            i55.validate_key(pairs)

    def test_ratings_complete_choices_notes(self):
        pairs = i55.assignments()
        ids = [p['id'] for p in pairs]
        ratings = {'schema': 'g6-i55-ratings/1', 'ratings': [{'id': p, 'choice': 'tie'} for p in ids]}
        self.assertEqual(len(i55.validate_ratings(ratings, ids)), 20)
        for mutate in (lambda r: r['ratings'].pop(),
                       lambda r: r['ratings'][0].update(choice=''),
                       lambda r: r['ratings'][0].update(id=ids[1]),
                       lambda r: r['ratings'][0].update(note='ą' * 513)):
            bad = json.loads(json.dumps(ratings))
            mutate(bad)
            with self.assertRaises(ValueError):
                i55.validate_ratings(bad, ids)

    def test_12_13_ties_and_identical_pairs(self):
        pairs = i55.assignments()
        for p in pairs:
            p['identical'] = True
        for wins, passed in ((12, False), (13, True)):
            answers = {p['id']: {'choice': p['L3side'] if n < wins else 'tie'} for n, p in enumerate(pairs)}
            report = i55.blind_result(pairs, answers)
            self.assertEqual(report['blindThresholdPassed'], passed)
            self.assertEqual(report['profiles']['all']['pairs'], 20)
            self.assertEqual(report['profiles']['all']['identicalPairs'], 20)
            self.assertEqual(report['profiles']['all']['ties'], 20 - wins)

    def test_22_23_strict_wins_medians_and_denominator(self):
        for wins in (22, 23):
            rows = [{'L2': 1, 'L3': 0 if n < wins else 1} for n in range(32)]
            report = i55.summarize(rows)
            self.assertEqual(report['atLeast23'], wins == 23)
            self.assertEqual(report['ties'], 32 - wins)
            self.assertEqual(report['medianDifferenceL3MinusL2'], -1)
        with self.assertRaises(ValueError):
            i55.summarize(rows[:-1])

    def test_equal_family_weights_and_missing_coverage(self):
        # L2=1 L3=2. ref M01=1, M05=2, winter maps=4 and 8.
        # Family weighting: L2=(0+ln2+(ln4+ln8)/2)/3 = 7ln2/6.
        rows = [self.row(), self.row(1, in_band=26, value=2)]
        groups = {s: rows for s in i55.SEEDS}
        refs = {m: dict.fromkeys(i55.g6.METRICS, v) for m, v in [('M01', 1), ('M05', 2), ('M11', 4), ('M18', 8)]}
        # Signed metrics use asinh, so make those definitional N/A in every map.
        for ref in refs.values():
            ref['scale.E16'] = ref['scale.E64'] = None
        report = i55.compare(groups, refs, {'M01': ['M01'], 'M05': ['M05'], 'winter': ['M11', 'M18']})
        expected = (24 * 7 * math.log(2) / 6 + 2 * math.log(64)) / 26
        self.assertAlmostEqual(report['perSeed'][0]['L2'], expected)
        self.assertEqual(report['coverage']['M01']['known'], 24)
        for bad_refs, families in (({}, {}), ({'M01': dict.fromkeys(i55.g6.METRICS)}, {'one': ['M01']}),
                                     (refs, {'one': ['M01']})):
            with self.assertRaises(ValueError):
                i55.compare(groups, bad_refs, families)

    def fixture_matrix(self):
        directory = self.root / 'matrix'
        directory.mkdir()
        index = self.root / 'synthetic.i53'
        index.write_text('M06 source semantic keymap 100\nM19 source semantic keymap 100\n')
        i55.write_json(directory / 'manifest.json', {'schema': 'g6-i55-inputs/1', 'policyCommit': i55.POLICY,
                       'inputs': {'index': {'path': str(index), 'sha256': i55.sha(index)}}})
        for style in i55.STYLES:
            bands = i55.g6.load_bands(i55.g6.BANDS_DIR / f'{style}.bands')
            lines = ['g6-candidate-report/1',
                     f'settings\tside=512\tcount=4\tpathLength={40 if style == "m06" else 80}\tbuildingDensity={30 if style == "m06" else 40}\trockBlocks={"false" if style == "m06" else "true"}\tg5=true\tmode=ranking',
                     'similarity\ti53-index/1\tg4-natural-v1\twindow=16\tmodulo=4\tminpresent=50\tshared>=16\tcontainment>=100']
            lines += [f'reference\t{alias}\tsource\tsemantic\tkeymap\t100\t90' for alias in ('M06', 'M19')]
            lines += [f'band\t{m}\t{r}\t{lo}\t{hi}' for m, (r, lo, hi) in bands.items()]
            lines += ['userSeed\tindex\tseed\tsurfaceFingerprint\thardFail\tinBand\tdistance\t' + '\t'.join(i55.g6.METRICS) +
                      '\tcopyState\tcopyBefore\tcopyAfter\tM06shared\tM06permille\tM19shared\tM19permille']
            for seed in i55.SEEDS:
                for n in range(4):
                    lines.append(f'{seed}\t{n}\t{i55.candidate_seed(seed, n)}\t123\tfalse\t26\t0\t' + '\t'.join(str(b[0]) for b in bands.values()) +
                                 '\tevaluated (below copy thresholds)\t100\t90\t0\t0\t0\t0')
                lines.append(f'winner\t{seed}\t0\tfalse\t26\t0')
            for run in (1, 2):
                (directory / f'{style}-run{run}.tsv').write_text('\n'.join(lines) + '\n')
        return directory

    def test_full_matrix_and_i53_refusals(self):
        directory = self.fixture_matrix()
        groups, audit = i55.read_matrix(directory)
        self.assertEqual(len(groups['m06']), 32)
        self.assertEqual(audit['m19']['candidates'], 128)
        p = directory / 'm06-run1.tsv'
        original = p.read_text()
        for bad in (original.replace('count=4', 'count=16'),
                    original.replace('similarity\ti53-index/1', 'similarity\tnot-evaluated'),
                    original.replace('\t90\t0\t0\t0\t0', '\t90\t20\t0\t0\t0'),
                    original.replace('winner\t3001\t0', 'winner\t3001\t1'),
                    original.replace(f'3001\t3\t{i55.candidate_seed(3001, 3)}', f'3001\t2\t{i55.candidate_seed(3001, 3)}')):
            for run in (1, 2):
                (directory / f'm06-run{run}.tsv').write_text(bad)
            with self.assertRaises(ValueError):
                i55.read_matrix(directory)
        for run in (1, 2):
            (directory / f'm06-run{run}.tsv').write_text(original)
        (self.root / 'synthetic.i53').write_text('changed')
        with self.assertRaises(ValueError):
            i55.read_matrix(directory)

    def test_package_no_identity_leak_and_incomplete_before_key(self):
        directory = self.root / 'matrix'
        directory.mkdir()
        (directory / 'manifest.json').write_text('{}')
        groups = {style: {s: [self.row(), self.row(1, in_band=2)] for s in i55.SEEDS} for style in i55.STYLES}
        # Opaque synthetic bytes suffice here; real PNG encoding is covered by CLI tests.
        for style in i55.STYLES:
            image_dir = directory / f'{style}-images1'
            image_dir.mkdir()
            for seed in i55.BLIND_SEEDS:
                for n in (0, 1):
                    (image_dir / f'{seed}-{n}.png').write_bytes(b'synthetic image')
        dest, key = self.root / 'evaluator', self.root / 'private/key.json'
        with patch.object(i55, 'read_matrix', return_value=(groups, {})):
            i55.package(directory, dest, key)
        manifest = json.loads((dest / 'manifest.json').read_text())
        self.assertEqual(len(list(dest.glob('*.png'))), 40)
        self.assertEqual(len(manifest['pairs']), 20)
        text = (dest / 'index.html').read_text()
        for secret in ('m06', 'm19', 'L2index', 'L3side', '3001', 'surfaceFingerprint'):
            self.assertNotIn(secret, text)
            self.assertNotIn(secret, (dest / 'manifest.json').read_text())
        ratings = self.root / 'ratings.json'
        i55.write_json(ratings, {'schema': 'g6-i55-ratings/1', 'ratings': []})
        with self.assertRaisesRegex(ValueError, 'all 20'):
            i55.score(dest, ratings, self.root / 'NONEXISTENT-KEY', self.root / 'result.json')
        answers = {'schema': 'g6-i55-ratings/1', 'ratings': [{'id': p['id'], 'choice': 'tie'} for p in manifest['pairs']]}
        ratings.unlink()
        i55.write_json(ratings, answers)
        i55.score(dest, ratings, key, self.root / 'result.json')
        result = json.loads((self.root / 'result.json').read_text())
        self.assertFalse(result['blindThresholdPassed'])
        (dest / manifest['pairs'][0]['views']['A']['file']).write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'image changed'):
            i55.score(dest, ratings, key, self.root / 'result2.json')

    def test_outputs_scratchpad_only_and_immutable(self):
        with self.assertRaises(ValueError):
            i55.private(ROOT / 'docs/private.json')
        i55.write_json(self.root / 'frozen.json', {})
        with self.assertRaises(FileExistsError):
            i55.write_json(self.root / 'frozen.json', {})


if __name__ == '__main__':
    unittest.main()
