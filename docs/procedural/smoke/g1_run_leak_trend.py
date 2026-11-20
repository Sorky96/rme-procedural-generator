"""Run the SELF-DRIVEN dialog leak-trend probe in a NEW private RME instance and evaluate the trend.

Unlike the other g1_* probes no operator input is needed: the probe queues the dialog's menu and button
events itself, so production handlers run but native input is NOT exercised. Each cycle: open the dialog
from the menu, Preview, in-flight Cancel, Preview, Validate, Cancel (closes), settle 1 s, sample.
The default uses Classic (one candidate). Use --candidates 1 and --candidates N to compare the same
Natural surface fixture at different candidate counts.

Criterion fixed BEFORE the first run: the first 2 cycles are warm-up; over cycles 3..N the least-squares
slope of working set and private bytes must be <= 1 MiB per cycle, and GDI objects, USER objects and
handles must not exceed their value after cycle 2.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

MIB = 1024 * 1024
SLOPE_LIMIT = 1.0  # MiB per cycle
WARMUP = 2

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cycles', type=int, default=10)
parser.add_argument('--side', type=int, choices=(100, 500), default=500)
parser.add_argument('--heap', action='store_true',
                    help='Also write heap-trend.csv and heap-buckets.csv (D4) plus a final HeapCompact sample')
parser.add_argument('--candidates', type=int, choices=range(1, 17), metavar='N',
                    help='Use the single-floor Natural surface fixture with N candidates (1–16)')
for name in ('output', 'profile_data', 'config'):
    parser.add_argument(name, type=lambda value: Path(value).resolve())
args = parser.parse_args()
if args.cycles < WARMUP + 3:
    parser.error('--cycles must leave at least 3 cycles after warm-up')
fixture = 'natural-single-floor' if args.candidates is not None else 'classic'
candidate_count = args.candidates or 1
repo = Path(__file__).resolve().parents[3]
if not (args.output.is_relative_to(repo / 'local-data/procedural-smoke') or args.output.is_relative_to(repo / 'scratchpad')):
    raise ValueError('Output must be a NEW private procedural-smoke directory')
args.output.mkdir(parents=True, exist_ok=False)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

probe = repo / 'tests/test_procedural_dialog_leak.cpp'
sources = [repo / name for name in (
    'source/ui/dialogs/terrain_generator_dialog.h', 'source/ui/dialogs/terrain_generator_dialog.cpp',
    'source/ui/main_menubar.cpp', 'source/ui/menubar/menubar_action_manager.cpp', 'data/menubar.xml',
    'docs/procedural/smoke/g1_run_leak_trend.py', 'docs/procedural/smoke/g1_build_input_probe.py')] + [probe]
source_hashes = {str(path.relative_to(repo)): sha(path) for path in sources}
subprocess.run([sys.executable, str(Path(__file__).with_name('g1_build_input_probe.py')), str(args.output),
                '--source', str(probe), '--application-entry'], cwd=repo, check=True)

run = args.output / 'run'
data = run / 'data'
data.mkdir(parents=True)
source_files = [args.profile_data / 'clients.toml', args.profile_data / 'menubar.xml', args.config, repo / 'data/menubar.xml']
source_files += [p for p in (args.profile_data / '1098').rglob('*') if p.is_file()]
original = {str(path): sha(path) for path in source_files}
shutil.copytree(args.profile_data / '1098', data / '1098')
shutil.copy2(args.profile_data / 'clients.toml', data / 'clients.toml')
shutil.copy2(repo / 'data/menubar.xml', data / 'menubar.xml')
config = args.config.read_text(encoding='utf-8')
config = re.sub(r'^recent_edited_map_path = .*?$', "recent_edited_map_path = ''", config, flags=re.M)
config = re.sub(r'^recent_files = \[[\s\S]*?^\]', 'recent_files = []', config, flags=re.M)
settings = {'data_directory': "'" + data.as_posix() + "/'", 'use_custom_data_directory': '1',
            'indirectory_installation': '1', 'only_one_instance': 'false', 'welcome_dialog': 'false',
            'use_updater': 'false', 'goto_website_on_boot': 'false'}
for key, value in settings.items():
    config, count = re.subn(r'^' + key + r' = .*?$', key + ' = ' + value, config, flags=re.M)
    assert count == 1, key
(run / 'config.toml').write_text(config, encoding='utf-8')
(run / '.g1-dialog-probe').write_text('Private synthetic editor only', encoding='utf-8')
marker = f'{args.cycles} {args.side}'
if args.heap:
    marker += ' heap'
if args.candidates is not None:
    marker += f' candidates {candidate_count}'
(run / '.g1-dialog-leak').write_text(marker, encoding='utf-8')
shutil.copytree(repo / 'build-ninja/assets', args.output / 'assets')
env = os.environ.copy()
env['PATH'] = str(repo / 'build-ninja') + os.pathsep + env.get('PATH', '')
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 1
with (args.output / 'stdout.log').open('w', encoding='utf-8') as out, (args.output / 'stderr.log').open('w', encoding='utf-8') as err:
    process = subprocess.Popen([str(args.output / 'snapshot_probe.exe')], cwd=run, env=env, stdout=out, stderr=err, startupinfo=startup)
    code = process.wait(timeout=max(1500, 12 * args.cycles * candidate_count + 300))

report = (run / 'dialog-probe.log').read_text(encoding='utf-8')
rows = list(csv.DictReader((run / 'leak-trend.csv').open(newline='')))
lines = report.splitlines()
heap_report = run / 'heap-trend.csv'
if args.heap and heap_report.is_file():
    with heap_report.open(newline='') as stream:
        heap_samples = sum(1 for _ in csv.DictReader(stream))
else:
    heap_samples = 0
series = [line for line in lines if line.startswith('LEAK_SERIES_PASS\t')]
expected_series = [f'LEAK_SERIES_PASS\t{i}\t{candidate_count}' for i in range(1, args.cycles + 1)]
complete = (code == 0 and 'LEAK_PASS' in lines and 'FAILED\t' not in report and len(rows) == args.cycles + 1 and
            f'LEAK_FIXTURE\t{fixture}\tcandidates={candidate_count}' in lines and series == expected_series and
            [f'LEAK_CYCLE_PASS\t{i}' for i in range(1, args.cycles + 1)] == [l for l in lines if l.startswith('LEAK_CYCLE_PASS')] and
            (not args.heap or heap_samples == args.cycles + 2))
unchanged = all(sha(Path(path)) == value for path, value in original.items())
current = all(sha(repo / path) == value for path, value in source_hashes.items())
stderr_lines = [l for l in (args.output / 'stderr.log').read_text(encoding='utf-8', errors='replace').splitlines() if l.strip()]
focus_only = bool(stderr_lines) and all('SetFocus' in l and '0x00000057' in l for l in stderr_lines)


def slope(values):
    n = len(values)
    mean_x, mean_y = (n - 1) / 2, sum(values) / n
    return sum((x - mean_x) * (y - mean_y) for x, y in enumerate(values)) / sum((x - mean_x) ** 2 for x in range(n))


trend = {}
if complete:
    tail = rows[WARMUP + 1:]  # row 0 is the baseline, rows 1..WARMUP are warm-up
    for key in ('working_set', 'private_bytes'):
        trend[key + '_slope_mib_per_cycle'] = slope([int(r[key]) for r in tail]) / MIB
    reference = rows[WARMUP]
    for key in ('gdi_objects', 'user_objects', 'handles'):
        trend[key + '_growth_after_warmup'] = int(rows[-1][key]) - int(reference[key])
trend_ok = bool(trend) and all(trend[k] <= SLOPE_LIMIT for k in trend if k.endswith('_mib_per_cycle')) and \
    all(trend[k] <= 0 for k in trend if k.endswith('_after_warmup'))
passed = complete and unchanged and current and (not stderr_lines or focus_only) and trend_ok
result = {'passed': passed, 'complete': complete, 'trend_ok': trend_ok, 'exit_code': code, 'cycles': args.cycles, 'heap': args.heap,
          'fixture': fixture, 'requested_candidates': candidate_count,
          'verified_series': len(series), 'effective_candidates': candidate_count if series == expected_series else None,
          'side': args.side, 'input_assets_unchanged': unchanged, 'current_sources': current,
          'stderr_focus_warning_only': focus_only, 'stderr_empty': not stderr_lines, 'trend': trend,
          'criterion': {'warmup_cycles': WARMUP, 'slope_limit_mib_per_cycle': SLOPE_LIMIT},
          'csv_sha256': sha(run / 'leak-trend.csv'), 'sources': source_hashes, 'report': report,
          'scope': 'Self-driven dialog cycles (queued menu/button events, not native input); process memory and handle counts only'}
(args.output / 'verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sources'}, indent=2))
sys.exit(0 if passed else 1)
