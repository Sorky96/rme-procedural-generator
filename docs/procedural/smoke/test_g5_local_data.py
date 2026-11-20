"""D25: G5 measurement finds the G4 profiles from a worktree, honours --local-data with spaces, and stops before generating
when a profile is missing."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from g5_local_data import main_checkout_local_data

MEASURE = Path(__file__).with_name('g5_measure_levels.py')


def git(*args):
    subprocess.run(['git', '-c', 'user.name=test', '-c', 'user.email=test@example.invalid', *map(str, args)],
                   check=True, capture_output=True)


class LocalDataTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='g5 local data ')
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)

    def test_worktree_defaults_to_main_checkout(self):
        main = self.folder / 'main checkout'
        git('init', '-q', main)
        git('-C', main, 'commit', '-q', '--allow-empty', '-m', 'root')
        worktree = self.folder / 'linked worktree'
        git('-C', main, 'worktree', 'add', '-q', '--detach', worktree)
        self.assertEqual(main_checkout_local_data(worktree).resolve(), (main / 'local-data').resolve())
        self.assertEqual(main_checkout_local_data(main).resolve(), (main / 'local-data').resolve())

    def test_missing_profile_stops_before_generation(self):
        local_data = self.folder / 'local data with spaces'
        profiles = local_data / 'procedural-smoke/g4-profiles/10.98-v10'
        profiles.mkdir(parents=True)
        (profiles / 'm06.profile').write_text('present')
        output = self.folder / 'measurement output'
        result = subprocess.run([sys.executable, '-I', str(MEASURE), '--i47', '--rock-blocks', 'm06=0', 'm19=1',
                                 '--local-data', str(local_data), '--output', str(output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn(str(profiles.resolve() / 'm19.profile'), result.stderr)
        self.assertNotIn('m06.profile', result.stderr)
        self.assertFalse(output.exists())

if __name__ == '__main__':
    unittest.main()
