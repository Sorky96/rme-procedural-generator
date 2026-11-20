"""Hand-counted I51b order, paired aggregation and complete-matrix refusals."""
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('i51b', ROOT / 'docs/procedural/smoke/g6_evaluate_i51b.py')
g6 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g6)


class I51bTests(unittest.TestCase):
    def test_orders_and_ties(self):
        rows = [dict(index=0, hard=False, inBand=2, distance=10000),
                dict(index=1, hard=False, inBand=1, distance=0),
                dict(index=2, hard=True, inBand=26, distance=0)]
        self.assertEqual(min(rows, key=lambda r: g6.ranking_key(r, 'A'))['index'], 0)
        self.assertEqual(min(rows, key=lambda r: g6.ranking_key(r, 'B'))['index'], 1)
        ties = [dict(index=i, hard=False, inBand=b, distance=10) for i, b in [(9, 2), (0, 1), (3, 2)]]
        for v in ('A', 'B'):
            self.assertEqual(min(ties, key=lambda r: g6.ranking_key(r, v))['index'], 3)

    def test_full_matrix_and_refusals(self):
        bands = g6.i51.load_bands(g6.i51.BANDS_DIR / 'm06.bands')
        lines = ['g6-candidate-report/1',
                 'settings\tside=512\tcount=16\tpathLength=40\tbuildingDensity=30\trockBlocks=false\tg5=true\tmode=ranking',
                 'experiment\tI51b']
        lines += ['band\t' + m + '\t' + '\t'.join(map(str, bands[m])) for m in g6.i51.METRICS]
        lines += ['userSeed\tindex\tseed\tsurfaceFingerprint\thardFail\tinBand\tdistance\t' + '\t'.join(g6.i51.METRICS)]
        for seed in range(2001, 2033):
            for index in range(16):
                metrics = [str(bands[m][0]) for m in g6.i51.METRICS]
                if index == 0:
                    # Two inclusive upper bounds: 2*ln(1.5)*1000 -> 811; 26 votes.
                    metrics[:2] = ['576', '4347']
                elif index == 1:
                    # ln(577/384)*1000 -> 407; one value outside => 25 votes.
                    metrics[0] = '577'
                elif index > 2:
                    metrics = ['NA'] * 26
                score = (26, 811) if index == 0 else (25, 407) if index == 1 else (26, 0) if index == 2 else (0, 108131)
                lines.append(f'{seed}\t{index}\t{seed}\t0\t{str(index == 2).lower()}\t{score[0]}\t{score[1]}\t' + '\t'.join(metrics))
            lines += [f'winner\t{seed}\t0\tfalse\t26\t811', f'winnerB\t{seed}\t1\tfalse\t25\t407']
        text = '\n'.join(lines) + '\n'
        path = ROOT / 'scratchpad/i51b-tests/m06-run1.tsv'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        groups, winners = g6.read_candidates(path, bands)
        self.assertEqual(len(groups), 32)
        self.assertEqual(set(winners['A'].values()), {0})
        self.assertEqual(set(winners['B'].values()), {1})
        for bad in (text.replace('winnerB\t2001\t1', 'winnerB\t2001\t0'),
                    text.replace('winnerB\t2001\t1\tfalse\t25\t407\n', ''),
                    text.replace('2001\t0\t2001', '2002\t0\t2002', 1),
                    '\n'.join(s for s in text.splitlines() if not s.startswith(('2032\t', 'winner\t2032\t', 'winnerB\t2032\t'))),
                    text.replace('\t25\t407\t', '\t25\t408\t', 1),
                    text.replace('experiment\tI51b', 'experiment\tother')):
            path.write_text(bad, encoding='utf-8')
            with self.assertRaises(ValueError):
                g6.read_candidates(path, bands)
        path.write_text(text, encoding='utf-8')

    def test_paired_aggregation(self):
        ref = dict.fromkeys(g6.i51.METRICS)
        ref['channels.L'] = 1
        refs = {m: ref for m in g6.i51.MAIN_MAPS + g6.i51.RELATED}
        groups, winners = {}, {'A': {}, 'B': {}}
        for seed in range(2001, 2033):
            rows = []
            for i in range(16):
                metric = ref.copy()
                # Median ln(16): first 16 improve, next 8 worsen, last 8 tie.
                metric['channels.L'] = (64 if seed < 2017 else 1) if i == 0 else (64 if 2017 <= seed < 2025 else 1) if i == 1 else 16
                rows.append(dict(index=i, hard=False, inBand=2 if i == 0 else 1,
                                 distance=0 if i == 1 else 1000 + i, metrics=metric))
            groups[seed] = rows
            winners['A'][seed], winners['B'][seed] = 0, 1
        result = g6.evaluate_ab(groups, winners, refs)
        self.assertTrue(result['thresholdPassed'])  # A=16, B=24; 24>=23 and 24>16.
        self.assertEqual(result['paired']['winnerChanges'], 32)
        for counts in result['paired']['summary'].values():
            self.assertEqual(counts, dict(improved=16, worsened=8, tied=8,
                failureToSuccess=16, successToFailure=8, successToSuccess=8, failureToFailure=0))
        for v, successes in (('A', 16), ('B', 24)):
            self.assertEqual(result['variants'][v]['summary']['family.main']['successes'], successes)
            self.assertEqual(result['variants'][v]['summary']['family.main']['randomBaselineExact'], '5/64')
        # Exact acceptance boundary and strict improvement are separate conditions.
        for a_count, b_count, passed in ((0, 22, False), (23, 23, False), (22, 23, True)):
            for seed, rows in groups.items():
                rows[0]['metrics']['channels.L'] = 1 if seed - 2001 < a_count else 64
                rows[1]['metrics']['channels.L'] = 1 if seed - 2001 < b_count else 64
            self.assertEqual(g6.evaluate_ab(groups, winners, refs)['thresholdPassed'], passed)
        del groups[2032]
        with self.assertRaises(ValueError):
            g6.evaluate_ab(groups, winners, refs)


if __name__ == '__main__':
    unittest.main()
