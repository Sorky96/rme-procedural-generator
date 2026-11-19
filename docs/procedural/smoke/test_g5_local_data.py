"""D25: G5 measurement finds the G4 profiles from a worktree, honours --local-data with spaces, and stops before generating
when a profile is missing."""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

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

    def test_explicit_local_data_with_spaces_reaches_generator(self):
        local_data = self.folder / 'local data with spaces'
        profiles = local_data / 'procedural-smoke/g4-profiles/10.98-v10'
        profiles.mkdir(parents=True)
        for name in ('m06', 'm19'):
            (profiles / f'{name}.profile').write_text('fixture')

        repo = self.folder / 'checkout'
        executable = repo / 'build-ninja/source/procedural/style_render.exe'
        executable.parent.mkdir(parents=True)
        executable.write_text('fixture')
        recorder = self.folder / 'record generator args.py'
        recorder.write_text(
            'import json, os, sys\n'
            'with open(os.environ["G5_ARGS"], "a", encoding="utf-8") as f:\n'
            ' json.dump(sys.argv[1:], f); f.write("\\n")\n'
            'print("level seed 1 used 1 attempts 1 all true edges 1/1")\n'
            'for name in ("doors", "sites", "caveEntrances", "caveExits", "caps", "caveCells", "storeys", "dungeons"):\n'
            ' for seed in range(32): print(f"level category seed {seed} {name} 1/1")\n'
            'for seed in range(32):\n'
            ' print(f"level repair seed {seed} repairs 0 bridges 0 removedCaps 0 removedPois 0 removedDecor 0 unrepaired false")\n'
            ' print(f"level cave pruning seed {seed} removedComponents 0")\n'
            ' print("level islands total 1 nonShoreIslands 0 nonShoreCells 0")\n'
            ' print("level cave i47 entrances 0 apart 0 floor 1 cavern 1 longestStraight 1 tunnels 0 loops 0 branches 0 caverns 0 parallelPairs 0")\n'
            ' for floor in range(1, 4): print(f"level flora level {floor} grass 1 flora 1 surfaceGrass 1 surfaceFlora 1")\n'
        )
        (repo / '.git').mkdir()
        git('-C', repo, 'init', '-q')
        launcher = self.folder / 'python launcher with spaces.py'
        launcher.write_text(
            'import os, subprocess, sys\n'
            'raise SystemExit(subprocess.run([sys.executable, os.environ["G5_FAKE_RENDER"], *sys.argv[1:]]).returncode)\n'
        )
        executable.write_text(f'#!"{sys.executable}" "{launcher}"\n')
        env = dict(os.environ, G5_ARGS=str(self.folder / 'args.json'), G5_FAKE_RENDER=str(recorder))
        output = self.folder / 'output with spaces'
        result = subprocess.run([sys.executable, '-I', str(MEASURE), '--i47', '--rock-blocks', 'm06=0', 'm19=1',
                                 '--local-data', str(local_data), '--output', str(output)],
                                capture_output=True, text=True, env=env)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('G5_GATE_PASS', result.stdout)
        commands = [json.loads(line) for line in (self.folder / 'args.json').read_text().splitlines()]
        self.assertEqual(len(commands), 256)
        self.assertTrue(all(str(profiles.resolve()) in command[0] for command in commands))


if __name__ == '__main__':
    unittest.main()
