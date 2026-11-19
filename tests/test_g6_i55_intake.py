"""Synthetic-only I55 intake fixtures; never access private maps or a blind key."""
import importlib.util
import json
import math
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('intake', REPO / 'docs/procedural/smoke/g6_i55_intake.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class IntakeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base = REPO / 'scratchpad/i55-intake-tests'
        base.mkdir(parents=True, exist_ok=True)
        cls.temp = tempfile.TemporaryDirectory(dir=base)
        cls.root = Path(cls.temp.name)
        cls.data = cls.fixture(cls.root)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @staticmethod
    def fixture(root):
        def write(path, content):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8')
            return str(path)
        assets = {k: write(root / k, 'synthetic ' + k) for k in ('dat', 'spr', 'otb')}
        hashes = {k: m.i55.sha(p) for k, p in assets.items()}
        evidence = write(root / 'evidence.txt', 'SYNTHETIC evidence, not legal consent or real evaluation')
        profiles = root / 'profiles'
        for style, alias in (('m06', 'M06'), ('m19', 'M19')):
            write(profiles / f'{style}.profile', 'g4-profile/6\nclient\t10.98\nlevel\tsurface\nfloors\t0\t7\n'
                  f"assets\tdat_sha256\t{hashes['dat']}\nassets\totb_sha256\t{hashes['otb']}\nsource\t{alias}\t1000\n")
        entries = []
        for n, alias in enumerate([f'M{i:02}' for i in range(1, 20)] + ['N01', 'N02']):
            source = write(root / 'sources' / (alias + '.fixture'), 'synthetic source ' + alias)
            source_sha = m.i55.sha(source)
            refs = {k: str(v[0]) for k, v in m.g4.load_bands(m.g4.BANDS_DIR / (
                'm19.bands' if alias == 'M19' else 'm06.bands')).items()}
            e = {'alias': alias, 'prior': alias in m.KNOWN, 'source': source, 'bundles': str(root),
                 'family': alias, 'identifiers': ['world:' + alias]}
            if not e['prior']:
                e['declaration'] = dict.fromkeys(('provenance', 'rightsBasis', 'rightsHolder', 'attestedBy', 'date'), 'fixture')
                e['declaration'].update(permissionToUse=True, neverUsedInAnalysisOrTuning=True,
                    provenanceEvidence=evidence, permissionEvidence=evidence, exposureEvidence=evidence)
            entries.append(e)
            for bundle in m.g4.BUNDLES:
                d = root / bundle / alias
                run = d / 'run'
                write(d / 'verification.json', json.dumps({'result': 'PASS', 'exit_code': 0, 'client_id': '10.98',
                    'source_hashes_unchanged': {'Tibia.dat': hashes['dat'], 'Tibia.spr': hashes['spr'], 'map': source_sha}}))
                write(run / 'reference-report.json', json.dumps({'schema': 'g3-import-report/1', 'client': '10.98',
                    'compatibility': 'compatible', 'sha256': source_sha, 'header': {}, 'tiles': 1000}))
                write(run / 'semantic-report.json', json.dumps({'schema': 'g3-semantic-report/1', 'sha256': source_sha}))
                write(run / 'keymap.tsv', 'g4-keymap/1\n')
                write(run / 'client-id.txt', '10.98\n')
                write(run / 'data/1098/items.otb', 'synthetic otb')
                write(run / 'analysis/fingerprint.fp', m.copy.fingerprint(
                    dict(windows=100, sparse=0, candidates=100, uniform=0), set(range(n * 100, n * 100 + 64))).decode())
                tables = {
                    'channels': dict(count=5, length_per_64k=refs['channels.L'], width_milli=refs['channels.W'],
                                     branches_per_100_milli=refs['channels.B'], connected_permille=refs['channels.C']),
                    'paths': dict(count=5, length_per_64k=refs['paths.L'], width_milli=refs['paths.W']),
                    'scale': dict(excess_16=refs['scale.E16'], excess_64=refs['scale.E64'], pairs_64=1000),
                    'buildings': dict(count=5, per_64k=refs['build.per64k'], long_median=refs['build.long'],
                                      short_median=refs['build.short'], wall_median=refs['outline.median']),
                    'patches': dict(zip(('boundary', 'compact', 'beach', 'rock_boundary', 'rock_compact', 'rock_dust'),
                                       [refs[k] for k in m.g4.METRICS[6:12]])),
                    'transitions': dict(rock_pairs=1000, dirt_pairs=1000,
                        rock_comp=' '.join(refs[f'trans.R{i}'] for i in range(4)),
                        dirt_comp=' '.join(refs[f'trans.D{i}'] for i in range(4)))}
                for name, rows in tables.items():
                    write(run / f'analysis/{name}-surface.txt', ''.join(f'{k}\t{v}\n' for k, v in rows.items()))
        return {'schema': 'g6-i55-intake/1', 'policyCommit': m.POLICY, 'evidenceKind': 'synthetic-test', 'assets': assets,
                'profiles': str(profiles), 'maps': entries,
                'declaration': {'curator': 'synthetic', 'date': 'fixture', 'historyScope': 'fixture only',
                                'completePriorExposureRegistry': True, 'evidence': evidence}}

    def spec(self, data=None):
        directory = Path(tempfile.mkdtemp(dir=self.root))
        path = directory / 'input.json'
        path.write_text(json.dumps(data or self.data), encoding='utf-8')
        return path

    def frozen(self):
        spec = self.spec()
        review = spec.with_name('review.json')
        m.prepare(spec, review)
        approval = m.read(review)
        approval.update(accepted=True, reviewer='synthetic test', date='fixture')
        review.write_text(json.dumps(approval))
        output = spec.parent / 'frozen'
        m.freeze(spec, review, output)
        return output / 'manifest.json'

    def test_freeze_audit_and_immutable_inputs(self):
        manifest = self.frozen()
        frozen = m.verify(manifest)
        self.assertEqual(frozen['units'], ['N01', 'N02'])
        self.assertEqual(frozen['families'], {'N01': ['N01'], 'N02': ['N02']})
        self.assertEqual(frozen['strictWinsRequiredPerProfile'], 23)
        self.assertEqual(len(frozen['references']['N01']), 26)
        with self.assertRaises(ValueError):
            m.freeze(self.spec(), manifest, manifest.parent)
        for path in (Path(self.data['maps'][-1]['source']),
                     self.root / m.g4.BUNDLES[0] / 'N01/run/analysis/channels-surface.txt',
                     Path(self.data['declaration']['evidence']),
                     self.root / m.g4.BUNDLES[0] / 'N01/run/keymap.tsv',
                     self.root / m.g4.BUNDLES[0] / 'N01/run/analysis/fingerprint.fp', manifest):
            original = path.read_bytes()
            try:
                path.write_bytes(original + b'\n')
                with self.assertRaises(ValueError):
                    m.verify(manifest)
            finally:
                path.write_bytes(original)

    def test_attestations_history_identity_and_review_blockers(self):
        for change in ('permission', 'exposure', 'history', 'known', 'related', 'empty', 'aliases'):
            data = json.loads(json.dumps(self.data))
            e = data['maps'][-1]
            if change == 'permission':
                e['declaration']['permissionToUse'] = False
            elif change == 'exposure':
                e['declaration']['neverUsedInAnalysisOrTuning'] = False
            elif change == 'history':
                data['maps'] = data['maps'][1:]
            elif change == 'known':
                e['source'] = data['maps'][0]['source']
            elif change == 'related':
                e['identifiers'] = data['maps'][0]['identifiers']
            elif change == 'empty':
                data['maps'] = data['maps'][:19]
            else:
                e['alias'] = '../escape'
            with self.subTest(change=change), self.assertRaises(ValueError):
                m.intake(self.spec(data))
        spec = self.spec()
        review = spec.with_name('review.json')
        m.prepare(spec, review)
        with self.assertRaises(ValueError):
            m.freeze(spec, review, spec.parent / 'frozen')
        with self.assertRaises(FileExistsError):
            m.prepare(spec, review)

    def test_template_is_ineligible_and_json_is_strict(self):
        output = self.root / 'template.json'
        m.template(output)
        self.assertEqual(len(m.read(output)['maps']), 20)
        with self.assertRaises((ValueError, OSError)):
            m.intake(output)
        for content in ('{"x": 1, "x": 2}', '{"x": NaN}'):
            output.write_text(content)
            with self.assertRaises(ValueError):
                m.read(output)

    def test_bundle_refusals_and_stale_human_acceptance(self):
        spec = self.spec()
        paths = {
            self.root / m.g4.BUNDLES[0] / 'N01/verification.json': lambda s: s.replace('PASS', 'FAIL'),
            self.root / m.g4.BUNDLES[0] / 'N01/run/keymap.tsv': lambda s: s + 'key\tchanged\n',
            self.root / m.g4.BUNDLES[0] / 'N01/run/data/1098/items.otb': lambda s: s + ' changed',
            self.root / m.g4.BUNDLES[0] / 'N01/run/reference-report.json': lambda s: s.replace('compatible', 'wrong'),
        }
        for path, mutate in paths.items():
            original = path.read_text()
            try:
                path.write_text(mutate(original))
                with self.subTest(path=path), self.assertRaises(ValueError):
                    m.intake(spec)
            finally:
                path.write_text(original)
        review = spec.with_name('review.json')
        m.prepare(spec, review)
        acceptance = m.read(review)
        acceptance.update(accepted=True, reviewer='synthetic', date='fixture')
        review.write_text(json.dumps(acceptance))
        data = m.read(spec)
        data['declaration']['date'] = 'changed after review'
        spec.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            m.freeze(spec, review, spec.parent / 'frozen')

    def test_g4_content_threshold_generic_and_single_linkage(self):
        entries = [{'alias': str(i), 'family': str(i), 'identifiers': [str(i)], 'prior': i == 0} for i in range(8)]
        fps = [set(range(i * 1000, i * 1000 + 160)) for i in range(8)]
        # ceil(8/4)=2: use a larger pool so pair overlap is not generic.
        entries += [{'alias': str(i), 'family': str(i), 'identifiers': [str(i)], 'prior': False} for i in range(8, 12)]
        fps += [set(range(i * 1000, i * 1000 + 160)) for i in range(8, 12)]
        fps[1] = set(range(16)) | set(range(1000, 1144))
        with self.assertRaises(ValueError):
            m.related(entries, fps)  # exactly 16 shared, exactly 100 permille
        fps[1] = set(range(15)) | set(range(1000, 1145))
        self.assertEqual(m.related(entries, fps)['genericThreshold'], 3)
        entries[2]['identifiers'] = entries[1]['identifiers']
        result = m.related(entries, fps)
        self.assertIn(['1', '2'], result['families'].values())
        entries[1]['identifiers'] = entries[0]['identifiers']
        with self.assertRaises(ValueError):
            m.related(entries, fps)
        # Generic windows in 3 of 12 maps are removed before overlap comparison.
        for i in range(3):
            fps[i] |= set(range(90000, 90020))
        entries[1]['identifiers'] = ['1']
        self.assertEqual(m.related(entries, fps)['after'][0], 160)
        fps[-1] = set()
        with self.assertRaises(ValueError):
            m.related(entries, fps)

    def test_evaluation_pairing_boundary_and_matrix_binding(self):
        manifest = self.frozen()
        frozen = m.verify(manifest)
        directory = manifest.parent.parent / 'matrix'
        directory.mkdir()
        index = directory / 'index.i53'
        index.write_text('i53-index/1 g4-natural-v1\n10.98 ' + frozen['assets']['otb'] + ' ' + frozen['assets']['dat'] + '\n' +
                         '\n'.join(a + ' ' + frozen['referenceAudit'][a]['maps'][a]['mapSha256'] + ' semantic ' +
                                   m.i55.sha(self.root / m.g4.BUNDLES[0] / a / 'run/keymap.tsv') + ' 100'
                                   for a in ('M06', 'M19')))
        inputs = {'familyManifest': {'path': str(manifest), 'sha256': m.i55.sha(manifest)},
                  'index': {'path': str(index), 'sha256': m.i55.sha(index)}}
        for style in m.i55.STYLES:
            inputs[style] = {'sha256': m.i55.sha(Path(self.data['profiles']) / (style + '.profile'))}
        (directory / 'manifest.json').write_text(json.dumps({'inputs': inputs}))
        refs = frozen['references']['N01']
        for wins in (22, 23):
            groups = {}
            for seed in m.i55.SEEDS:
                good = dict(refs)
                bad = dict(refs, **{'channels.L': refs['channels.L'] * 10})
                metrics = (bad, good) if seed < 3001 + wins else (good, good)
                groups[seed] = [{'index': i, 'metrics': value, 'hard': False,
                                 'inBand': i, 'distance': 0} for i, value in enumerate(metrics)]
            pool = {style: groups for style in m.i55.STYLES}
            output = directory / f'result-{wins}.json'
            with patch.object(m.i55, 'read_matrix', return_value=(pool, {})):
                self.assertEqual(m.evaluate(manifest, directory, output), int(wins < 23))
            result = m.read(output)
            self.assertEqual(result['profiles']['m06']['summary']['strictL3Wins'], wins)
            self.assertEqual(result['G6'], 'OPEN')
            self.assertEqual(result['blind'], 'NOT READ')
            self.assertFalse(result['countsTowardI55'])
            self.assertFalse(result['eligibleQuantitativeEvidence'])
        inputs.pop('familyManifest')
        (directory / 'manifest.json').write_text(json.dumps({'inputs': inputs}))
        with self.assertRaises(KeyError):
            m.evaluate(manifest, directory, directory / 'rejected.json')

    def test_equal_family_weight_and_fixed_na(self):
        ref = dict.fromkeys(m.g4.METRICS, 1)
        other = dict.fromkeys(m.g4.METRICS, 2)
        na = dict(ref)
        na['channels.L'] = None
        self.assertAlmostEqual(m.g4.map_distance(ref, na), math.log(64) / 26)
        groups = {s: [{'index': 0, 'metrics': ref, 'hard': False, 'inBand': 0, 'distance': 0}] for s in m.i55.SEEDS}
        result = m.i55.compare(groups, {'A': ref, 'B': other, 'C': other}, {'one': ['A'], 'two': ['B', 'C']})
        row = result['perSeed'][0]
        self.assertAlmostEqual(row['L2'], (row['maps']['A']['L2'] + row['maps']['B']['L2']) / 2)
        self.assertEqual(result['summary']['ties'], 32)

    def test_full_synthetic_matrix_without_mocked_reader(self):
        test_spec = importlib.util.spec_from_file_location('existing_i55_tests', REPO / 'tests/test_g6_i55.py')
        existing = importlib.util.module_from_spec(test_spec)
        test_spec.loader.exec_module(existing)
        helper = existing.I55Tests()
        helper.root = Path(tempfile.mkdtemp(dir=self.root))
        directory = helper.fixture_matrix()
        manifest = self.frozen()
        frozen = m.verify(manifest)
        matrix = m.read(directory / 'manifest.json')
        matrix['inputs']['familyManifest'] = {'path': str(manifest), 'sha256': m.i55.sha(manifest)}
        for style in m.i55.STYLES:
            p = Path(self.data['profiles']) / f'{style}.profile'
            matrix['inputs'][style] = {'path': str(p), 'sha256': m.i55.sha(p)}
        index = Path(matrix['inputs']['index']['path'])
        lines = ['i53-index/1 g4-natural-v1', f"10.98 {frozen['assets']['otb']} {frozen['assets']['dat']}"]
        for alias in ('M06', 'M19'):
            sha = frozen['referenceAudit'][alias]['maps'][alias]['mapSha256']
            keymap = m.i55.sha(self.root / m.g4.BUNDLES[0] / alias / 'run/keymap.tsv')
            lines.append(f'{alias} {sha} semantic {keymap} 100')
            for style in m.i55.STYLES:
                for run in (1, 2):
                    p = directory / f'{style}-run{run}.tsv'
                    p.write_text(p.read_text().replace(f'reference\t{alias}\tsource\tsemantic\tkeymap\t',
                                                      f'reference\t{alias}\t{sha}\tsemantic\t{keymap}\t'))
        index.write_text('\n'.join(lines) + '\n')
        matrix['inputs']['index']['sha256'] = m.i55.sha(index)
        (directory / 'manifest.json').write_text(json.dumps(matrix))
        output = directory / 'result.json'
        self.assertEqual(m.evaluate(manifest, directory, output), 1)
        self.assertFalse(m.read(output)['quantitativeThresholdPassed'])
        self.assertEqual(m.read(output)['profiles']['m19']['summary']['ties'], 32)
        index.write_text(index.read_text().replace('M06 ', 'OTHER ', 1))
        with self.assertRaises(ValueError):
            m.check_generator_inputs(frozen, self.data['profiles'], index)


if __name__ == '__main__':
    unittest.main()
