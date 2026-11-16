"""Full procedural-core regression gate: rebuild the core test targets, run every core test exe, then (optionally) both G1 probes.

Run after EVERY change to source/procedural/core. Exit code 0 only when everything passed.

  py -I docs/procedural/smoke/g5_run_suite.py                      # rebuild, core tests only
  py -I docs/procedural/smoke/g5_run_suite.py --probes PROFILE_DATA CONFIG   # plus --commit and --surface probes

build_ninja.bat builds only rme.exe, so the suite itself rebuilds style_render and every test_procedural_* target in
build-ninja (Ninja on PATH, MSVC found through vswhere) and fails when that build fails. The probes still need a fresh
rme.exe from build_ninja.bat. Relative PROFILE_DATA and CONFIG paths that do not exist from the current directory are
taken from --local-data, by default the main checkout's local-data (found with `git worktree list`), so a worktree
run needs no absolute path to it.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
build = repo / 'build-ninja'


def main_checkout_local_data():
    listing = subprocess.run(['git', '-C', str(repo), 'worktree', 'list', '--porcelain'], capture_output=True, text=True)
    first = next((line[len('worktree '):] for line in listing.stdout.splitlines() if line.startswith('worktree ')), None)
    return Path(first) / 'local-data' if listing.returncode == 0 and first else repo / 'local-data'


def resolve_input(path, local_data):
    return path.resolve() if path.is_absolute() or path.exists() else (local_data / path).resolve()


def rebuild_core_targets():
    """Rebuild style_render and every test_procedural_* target; returns the target list, or exits on failure."""
    ninja = shutil.which('ninja')
    if not ninja or not (build / 'build.ninja').exists():
        sys.exit('SUITE_FAIL: rebuild needs ninja on PATH and a configured build-ninja (run build_ninja.bat first)')
    listing = subprocess.run([ninja, '-C', str(build), '-t', 'targets', 'all'], capture_output=True, text=True)
    targets = sorted({m.group(1) for m in re.finditer(r'^(test_procedural_\w+|style_render): phony', listing.stdout, re.M)})
    if listing.returncode != 0 or 'style_render' not in targets or len(targets) < 2:
        sys.exit('SUITE_FAIL: could not list the core test targets in build-ninja')
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    found = subprocess.run([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                            '-property', 'installationPath'], capture_output=True, text=True) if vswhere.exists() else None
    vcvars = Path(found.stdout.strip()) / 'VC/Auxiliary/Build/vcvarsall.bat' if found and found.stdout.strip() else None
    if not vcvars or not vcvars.exists():
        sys.exit('SUITE_FAIL: vcvarsall.bat not found through vswhere')
    command = f'call "{vcvars}" x64 >nul && cmake --build "{build}" --target {" ".join(targets)} --parallel 2'
    started = time.monotonic()
    # A string, not a list: list quoting escapes the inner quotes, which cmd does not understand.
    result = subprocess.run(f'cmd /d /s /c "{command}"', capture_output=True, text=True)
    steps = len(re.findall(r'^\[\d+/\d+\]', result.stdout, re.M))
    print(f'rebuilt targets ({len(targets)}): {" ".join(targets)}')
    print(f'rebuild exit={result.returncode} build steps={steps} {time.monotonic() - started:.1f}s')
    if result.returncode != 0:
        print((result.stdout + result.stderr)[-3000:])
        sys.exit('SUITE_FAIL: core target rebuild failed')
    return targets


parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--probes', nargs=2, metavar=('PROFILE_DATA', 'CONFIG'), type=Path)
parser.add_argument('--local-data', type=Path, help='base for relative probe inputs (default: the main checkout local-data)')
args = parser.parse_args()

rebuild_core_targets()
failures = []
tests = sorted((build / 'source/procedural').glob('test_procedural_*.exe'))
if not tests:
    sys.exit('No core test executables found after the rebuild')
for exe in tests:
    started = time.monotonic()
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    print(f'{exe.name:<46} exit={result.returncode} {time.monotonic() - started:.1f}s')
    if result.returncode != 0:
        failures.append(exe.name)
        print((result.stdout + result.stderr)[-1500:])
print(f'core tests: {len(tests) - len(failures)}/{len(tests)} passed')

if args.probes:
    local_data = (args.local_data or main_checkout_local_data()).resolve()
    profile_data, config = (resolve_input(path, local_data) for path in args.probes)
    print(f'probe inputs: {profile_data} {config}')
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
        stderr_log = out / 'stderr.log'
        stderr_lines = [line for line in stderr_log.read_text(encoding='utf-8', errors='replace').splitlines() if line.strip()] if stderr_log.exists() else []
        foreign = [line for line in stderr_lines if "'SetFocus' failed" not in line]
        ok = (report.get('exit_code') == 0 and report.get('input_assets_unchanged') is True
              and marker in report.get('report', '') and 'FAILED\t' not in report.get('report', '') and not foreign)
        if mode == 'surface':
            ok &= 'MOUNTAIN_PASS_GUI_PASS' in report.get('report', '')
            ok &= 'BUILDING_STAIRS_GUI_PASS' in report.get('report', '')
            ok &= 'STOREY_SEALED_RAMP_REPAIR_PASS' in report.get('report', '')
            ok &= 'M19_ROCK_GUI_CASE_PASS' in report.get('report', '')
            ok &= 'DUNGEON_GUI_PASS' in report.get('report', '')
            ok &= 'D26_RAMP_ENTRANCE_PASS' in report.get('report', '')
            ok &= 'M19_CAVES_GUI_PASS' in report.get('report', '')
            ok &= 'FALLBACK_ENTRANCE_LOOK_PASS' in report.get('report', '')
        print(f'probe --{mode:<8} {"PASS" if ok else "FAIL"} (exit_code={report.get("exit_code")}, '
              f'input_assets_unchanged={report.get("input_assets_unchanged")}, SetFocus warnings={len(stderr_lines) - len(foreign)}, other stderr={len(foreign)})')
        if not ok:
            failures.append('probe-' + mode)
            print((run.stdout + run.stderr)[-1500:])

print('SUITE_PASS' if not failures else 'SUITE_FAIL: ' + ', '.join(failures))
sys.exit(1 if failures else 0)
