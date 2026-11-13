"""Full procedural-core regression gate: every core test exe, then (optionally) both G1 probes.

Run after EVERY change to source/procedural/core. Exit code 0 only when everything passed.

  py -I docs/procedural/smoke/g5_run_suite.py                      # core tests only
  py -I docs/procedural/smoke/g5_run_suite.py --probes PROFILE_DATA CONFIG   # plus --commit and --surface probes

The probes need a fresh rme.exe (rebuild with build_ninja.bat first) and a private profile in local-data.
"""
import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--probes', nargs=2, metavar=('PROFILE_DATA', 'CONFIG'), type=Path)
args = parser.parse_args()

failures = []
tests = sorted((repo / 'build-ninja/source/procedural').glob('test_procedural_*.exe'))
if not tests:
    sys.exit('No core test executables found; build first with build_ninja.bat')
for exe in tests:
    started = time.monotonic()
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    print(f'{exe.name:<46} exit={result.returncode} {time.monotonic() - started:.1f}s')
    if result.returncode != 0:
        failures.append(exe.name)
        print((result.stdout + result.stderr)[-1500:])
print(f'core tests: {len(tests) - len(failures)}/{len(tests)} passed')

if args.probes:
    profile_data, config = (path.resolve() for path in args.probes)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    for mode in ('commit', 'surface'):
        out = repo / 'local-data/procedural-smoke' / f'suite-{mode}-{stamp}'
        run = subprocess.run([sys.executable, '-I', str(repo / 'docs/procedural/smoke/g1_run_resolver_probe.py'),
                              str(out), str(profile_data), str(config), f'--{mode}'], capture_output=True, text=True)
        try:
            report = json.loads(run.stdout[run.stdout.index('{'):])
        except ValueError:
            report = {}
        marker = 'PREVIEW_PASS' if mode == 'commit' else 'SURFACE_RESOLVER_PASS'
        # The runner itself reports passed=false whenever stderr is non-empty. A hidden probe window makes wx print
        # 'SetFocus failed' lines; that known message is tolerated here, anything else on stderr is a failure.
        stderr_lines = [line for line in (out / 'stderr.log').read_text(encoding='utf-8', errors='replace').splitlines() if line.strip()]
        foreign = [line for line in stderr_lines if "'SetFocus' failed" not in line]
        ok = (report.get('exit_code') == 0 and report.get('input_assets_unchanged') is True
              and marker in report.get('report', '') and 'FAILED\t' not in report.get('report', '') and not foreign)
        if mode == 'surface':
            ok &= 'MOUNTAIN_PASS_GUI_PASS' in report.get('report', '')
            ok &= 'BUILDING_STAIRS_GUI_PASS' in report.get('report', '')
            ok &= 'STOREY_SEALED_RAMP_REPAIR_PASS' in report.get('report', '')
            ok &= 'M19_ROCK_GUI_CASE_PASS' in report.get('report', '')
            ok &= 'DUNGEON_GUI_PASS' in report.get('report', '')
        print(f'probe --{mode:<8} {"PASS" if ok else "FAIL"} (exit_code={report.get("exit_code")}, '
              f'input_assets_unchanged={report.get("input_assets_unchanged")}, SetFocus warnings={len(stderr_lines) - len(foreign)}, other stderr={len(foreign)})')
        if not ok:
            failures.append('probe-' + mode)
            print((run.stdout + run.stderr)[-1500:])

print('SUITE_PASS' if not failures else 'SUITE_FAIL: ' + ', '.join(failures))
sys.exit(1 if failures else 0)
