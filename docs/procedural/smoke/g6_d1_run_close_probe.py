"""D1: run the private close probe (g6_d1_close_probe.cpp) on client 10.98 for each case.

Startup loads 10.98 and lists asset warnings; the probe records them and presses OK (EndModal).
Cases, each in a NEW private process and profile copy (the startup empty map is the first tab):
  exit       two clean tabs, File > Exit
  cancel     dirty first tab + clean current tab, File > Exit, Cancel on the save prompt, then Exit
  tab-close  three clean tabs, File > Close, the notebook PAGE_CLOSE event, then File > Exit
Commands are queued wxEVT_MENU / PAGE_CLOSE events handled by production code, not mouse clicks.
A case passes with exit code 0, all markers in order, unchanged source assets and empty stderr.
A stderr consisting only of wx 'SetFocus' 0x00000057 lines is reported as FOCUS_WARNING.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

CASES = {
    'exit': ['D1_READY', 'D1_EXIT_POSTED', 'D1_MODULE_EXIT'],
    'cancel': ['D1_READY', 'D1_EXIT_POSTED', 'D1_SAVE_PROMPT', 'D1_CANCEL_PASS', 'D1_EXIT_POSTED', 'D1_MODULE_EXIT'],
    'tab-close': ['D1_READY', 'D1_FILE_CLOSE_POSTED', 'D1_FILE_CLOSE_PASS', 'D1_TAB_BUTTON_CLOSE_POSTED',
                  'D1_TAB_BUTTON_CLOSE_PASS', 'D1_EXIT_POSTED', 'D1_MODULE_EXIT'],
}

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('output', type=lambda value: Path(value).resolve(), help='NEW private output directory')
parser.add_argument('profile_data', type=lambda value: Path(value).resolve(), help='Profile data with 1098/ and clients.toml')
parser.add_argument('config', type=lambda value: Path(value).resolve())
parser.add_argument('--case', choices=sorted(CASES), action='append', help='Default: all cases')
parser.add_argument('--timeout', type=int, default=300)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
args.output.mkdir(parents=True, exist_ok=False)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
probe = Path(__file__).with_name('g6_d1_close_probe.cpp')
sources = [probe, Path(__file__), repo / 'tests/procedural_focus_trace.h', repo / 'source/editor/editor_tabs.cpp',
           repo / 'source/editor/managers/editor_manager.cpp', repo / 'source/ui/main_frame.cpp']
source_hashes = {str(path.relative_to(repo)): sha(path) for path in sources}

build = args.output / 'build'
build.mkdir()
subprocess.run([sys.executable, str(Path(__file__).with_name('g1_build_input_probe.py')), str(build),
                '--source', str(probe), '--application-entry', '--symbols'], cwd=repo, check=True)
source_files = [args.profile_data / 'clients.toml', args.config, repo / 'data/menubar.xml']
source_files += [p for p in (args.profile_data / '1098').rglob('*') if p.is_file()]
original = {str(path): sha(path) for path in source_files}

config_text = args.config.read_text(encoding='utf-8')
config_text = re.sub(r'^recent_edited_map_path = .*?$', "recent_edited_map_path = ''", config_text, flags=re.M)
config_text = re.sub(r'^recent_files = \[[\s\S]*?^\]', 'recent_files = []', config_text, flags=re.M)

results = {}
for case in args.case or list(CASES):
    root = args.output / case
    run = root / 'run'
    data = run / 'data'
    data.mkdir(parents=True)
    for item in build.glob('snapshot_probe.*'):
        shutil.copy2(item, root / item.name)
    shutil.copytree(repo / 'build-ninja/assets', root / 'assets')
    shutil.copytree(args.profile_data / '1098', data / '1098')
    shutil.copy2(args.profile_data / 'clients.toml', data / 'clients.toml')
    shutil.copy2(repo / 'data/menubar.xml', data / 'menubar.xml')
    text = config_text
    settings = {'data_directory': "'" + data.as_posix() + "/'", 'use_custom_data_directory': '1',
                'indirectory_installation': '1', 'only_one_instance': 'false', 'welcome_dialog': 'false',
                'use_updater': 'false', 'goto_website_on_boot': 'false'}
    for key, value in settings.items():
        text, count = re.subn(r'^' + key + r' = .*?$', key + ' = ' + value, text, flags=re.M)
        assert count == 1, key
    (run / 'config.toml').write_text(text, encoding='utf-8')
    (run / 'd1-case.txt').write_text(case + '\n', encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = str(repo / 'build-ninja') + os.pathsep + env.get('PATH', '')
    env['_NT_SYMBOL_PATH'] = ';'.join(map(str, (root, repo / 'build-ninja/vcpkg_installed/x64-windows/bin')))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 1
    timed_out = False
    with (root / 'stdout.log').open('w', encoding='utf-8') as out, (root / 'stderr.log').open('w', encoding='utf-8') as err:
        process = subprocess.Popen([str(root / 'snapshot_probe.exe')], cwd=run, env=env, stdout=out, stderr=err,
                                   startupinfo=startup)
        try:
            code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()  # Only this private child.
            code = process.wait()
    log_path = run / 'd1-probe.log'
    lines = log_path.read_text(encoding='utf-8').splitlines() if log_path.is_file() else []
    startup_warnings = [line for line in lines if line.startswith('D1_STARTUP_WARNING\t')]
    lines = [line for line in lines if line not in startup_warnings]
    names = [line.split('\t', 1)[0] for line in lines]
    stderr_lines = [line for line in (root / 'stderr.log').read_text(encoding='utf-8', errors='replace').splitlines()
                    if line.strip()]
    focus_lines = [line for line in stderr_lines if 'SetFocus' in line and '0x00000057' in line]
    trace_path = run / 'focus-trace.log'
    trace = trace_path.read_text(encoding='utf-8', errors='replace') if trace_path.is_file() else ''
    markers_ok = names == CASES[case]
    functional = code == 0 and not timed_out and markers_ok and 'FAILED' not in names
    status = ('PASS' if functional and not stderr_lines else
              'FOCUS_WARNING' if functional and len(focus_lines) == len(stderr_lines) else 'FAIL')
    results[case] = {
        'status': status, 'exit_code': code, 'startup_warnings_acknowledged': len(startup_warnings), 'timed_out': timed_out, 'markers_ok': markers_ok, 'report': lines,
        'stderr_lines': len(stderr_lines), 'setfocus_87_lines': len(focus_lines),
        'other_stderr': [line for line in stderr_lines if line not in focus_lines],
        'traced_setfocus_failures': trace.count("'SetFocus' failed"),
        'trace_via_delete_tab': trace.count('MapTabbook::DeleteTab'),
    }
unchanged = all(sha(Path(path)) == value for path, value in original.items())
current = all(sha(repo / path) == value for path, value in source_hashes.items())
summary = {
    'passed': unchanged and current and all(result['status'] == 'PASS' for result in results.values()),
    'input_assets_unchanged': unchanged, 'current_sources': current, 'sources': source_hashes,
    'normal_exe_sha256': sha(repo / 'build-ninja/rme.exe'), 'probe_exe_sha256': sha(build / 'snapshot_probe.exe'),
    'cases': results,
    'scope': 'Private application-entry probe on client 10.98, synthetic empty maps; queued production menu and '
             'notebook close events, not mouse input. Not a visual GUI check.',
}
(args.output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in summary.items() if key != 'sources'}, indent=2))
sys.exit(0 if summary['passed'] else 1)
