"""D21: the iteration 3-4 runner names missing local data before building anything, and a documented deviation passes
only while the report keeps exactly the documented value."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import g4_reproduce_iter34 as runner

SCRIPT = Path(__file__).with_name('g4_reproduce_iter34.py')


class RunnerTest(unittest.TestCase):
    def test_missing_local_data_is_listed_before_any_build(self):
        with tempfile.TemporaryDirectory(prefix='d21 local data ') as tmp:
            local_data = Path(tmp) / 'local data'
            split = local_data / 'procedural-smoke/g4-split'
            split.mkdir(parents=True)
            (split / 'manifest.tsv').write_text('g4-manifest/1\n# alias\tsha256\tclient\trole\tfamily\nM01\t00\t10.98\tstyle\tm01\n')
            out = Path(tmp) / 'out'
            result = subprocess.run([sys.executable, '-I', str(SCRIPT), str(out), '--local-data', str(local_data)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('g4-split/fp-c/M01.fp', result.stderr)
            self.assertIn('g4-water-20261108a/M01/run/analysis/water-shapes-surface.txt', result.stderr)
            self.assertIn('g4-fit/styles-1.json', result.stderr)
            self.assertNotIn('doodad-brushes.csv', result.stderr)
            self.assertTrue(result.stderr.rstrip().endswith('D21_FAIL'))
            self.assertFalse(out.exists())

    def test_documented_deviation(self):
        rows = runner.evaluate([('same', 1, 1), ('off', 2, 1), ('known', 3, 1, 3), ('drifted', 4, 1, 3), ('fixed', 1, 1, 3)])
        self.assertEqual([r['status'] for r in rows], ['match', 'mismatch', 'documented deviation', 'mismatch', 'match'])

    def test_manifest_aliases_skip_schema_and_comments(self):
        with tempfile.TemporaryDirectory() as tmp:
            manifest = Path(tmp) / 'manifest.tsv'
            manifest.write_text('g4-manifest/1\n# alias\tsha256\nM01\ta\nM19\tb\n')
            self.assertEqual(runner.manifest_aliases(manifest), ['M01', 'M19'])


if __name__ == '__main__':
    unittest.main()
