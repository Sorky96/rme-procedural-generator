"""Build/run the G1 input/ground-variant probe against current RME objects.

No client, map files, GUI startup, Docker or external service is used.
Run the canonical build first. OUTPUT must be a new private directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=lambda value: Path(value).resolve())
mode = parser.add_mutually_exclusive_group()
mode.add_argument('--snapshot', action='store_true', help='Test snapshot memory, owned halo and complete equality')
mode.add_argument('--border-order', action='store_true', help='Test real overlapping autoborder rules in controlled address orders')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
if not args.output.is_relative_to(repo / 'local-data/procedural-smoke'):
    parser.error('Output must be inside local-data/procedural-smoke')
args.output.mkdir(parents=True, exist_ok=False)
source = repo / ('tests/test_procedural_border_order.cpp' if args.border_order else
                 'tests/test_procedural_snapshot_memory.cpp' if args.snapshot else 'tests/test_procedural_ground_brush.cpp')
subprocess.run([sys.executable, str(Path(__file__).with_name('g1_build_input_probe.py')),
                str(args.output), '--source', str(source)], cwd=repo, check=True)
env = os.environ.copy()
env['PATH'] = str(repo / 'build-ninja') + os.pathsep + env.get('PATH', '')
with (args.output / 'stdout.log').open('w', encoding='utf-8') as stdout, \
        (args.output / 'stderr.log').open('w', encoding='utf-8') as stderr:
    result = subprocess.run([str(args.output / 'snapshot_probe.exe')], cwd=args.output,
                            env=env, stdout=stdout, stderr=stderr, timeout=60)
paths = [source, repo / 'source/procedural/adapter/generation_input.cpp',
         repo / 'source/procedural/adapter/generation_input.h',
         repo / 'source/procedural/core/generation.cpp', repo / 'source/procedural/core/generation.h',
         repo / 'source/brushes/ground/ground_brush.cpp', repo / 'source/brushes/ground/ground_brush.h']
if args.snapshot:
    paths += [repo / path for path in (
        'source/procedural/adapter/generation_snapshot.cpp', 'source/procedural/adapter/generation_snapshot.h',
        'source/procedural/adapter/snapshot_memory.cpp', 'source/procedural/adapter/snapshot_memory.h',
        'source/procedural/adapter/snapshot_equal.cpp', 'source/procedural/adapter/snapshot_equal.h',
        'source/game/item_attributes.h', 'source/game/creature.h')]
if args.border_order:
    paths += [repo / path for path in ('source/brushes/ground/ground_border_calculator.cpp',
                                      'source/brushes/ground/ground_border_calculator.h')]
marker = ('test_procedural_border_order passed' if args.border_order else
          'test_procedural_snapshot_memory passed' if args.snapshot else 'PROCEDURAL INPUT AND GROUND VARIANT PASS')
report = {
    'exit_code': result.returncode,
    'scope': ('actual RME overlapping border rules; traversal/dedup/default address order; no assets/commit/GUI' if args.border_order else
              'actual RME snapshot memory/capture/equality; no assets/drawing/commit/GUI' if args.snapshot else
              'actual RME input capture and explicit ground variant selection; no assets/drawing/commit/GUI'),
    'sources': {str(path.relative_to(repo)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths},
    'passed': result.returncode == 0 and marker in
              (args.output / 'stdout.log').read_text(encoding='utf-8'),
}
(args.output / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
sys.exit(0 if report['passed'] else 1)
