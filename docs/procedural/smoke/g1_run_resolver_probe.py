"""Run the real loaded-client G1 resolver in a new private RME profile. No map files are opened."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--preview', action='store_true', help='Run the owned sandbox/diff probe instead of resolver-only checks')
parser.add_argument('--commit', action='store_true', help='Include atomic terrain history assertions; no GUI preview or G1 E2E claim')
parser.add_argument('--visible', action='store_true', help='Show the private GUI normally for editor/selection tests requiring focus')
for name in ('output', 'profile_data', 'config'):
    parser.add_argument(name, type=lambda value: Path(value).resolve())
args = parser.parse_args()
if args.commit:
    args.preview = True
repo = Path(__file__).resolve().parents[3]
if not args.output.is_relative_to(repo / 'local-data/procedural-smoke'):
    parser.error('A private output inside local-data/procedural-smoke is required')
args.output.mkdir(parents=True, exist_ok=False)
subprocess.run([sys.executable, str(Path(__file__).with_name('g1_build_input_probe.py')),
                str(args.output), '--source', str(repo / ('tests/test_procedural_preview.cpp' if args.preview else 'tests/test_procedural_resolver.cpp')),
                '--application-entry'], cwd=repo, check=True)
run = args.output / 'run'
data = run / 'data'
data.mkdir(parents=True)
source_files = [args.profile_data / 'clients.toml', args.profile_data / 'menubar.xml']
source_files += [path for path in (args.profile_data / '1098').rglob('*') if path.is_file()]
original = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in source_files}
shutil.copytree(args.profile_data / '1098', data / '1098')
for path in source_files[:2]:
    shutil.copy2(path, data / path.name)
config = args.config.read_text(encoding='utf-8')
config = re.sub(r'^recent_edited_map_path = .*?$', "recent_edited_map_path = ''", config, flags=re.M)
config = re.sub(r'^recent_files = \[[\s\S]*?^\]', 'recent_files = []', config, flags=re.M)
settings = {
    'data_directory': "'" + data.as_posix() + "/'", 'use_custom_data_directory': '1',
    'indirectory_installation': '1', 'only_one_instance': 'false', 'welcome_dialog': 'false',
    'use_updater': 'false', 'goto_website_on_boot': 'false',
}
for key, value in settings.items():
    config, count = re.subn(r'^' + key + r' = .*?$', key + ' = ' + value, config, flags=re.M)
    if count != 1:
        raise RuntimeError('Expected one private setting: ' + key)
(run / 'config.toml').write_text(config, encoding='utf-8')
(run / '.g1-resolver-probe').write_text('Private synthetic map, no map file input', encoding='utf-8')
shutil.copytree(repo / 'build-ninja/assets', args.output / 'assets')
env = os.environ.copy()
env['PATH'] = str(repo / 'build-ninja') + os.pathsep + env.get('PATH', '')
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 1 if args.visible else 0
with (args.output / 'stdout.log').open('w', encoding='utf-8') as stdout, \
        (args.output / 'stderr.log').open('w', encoding='utf-8') as stderr:
    process = subprocess.Popen([str(args.output / 'snapshot_probe.exe')], cwd=run, env=env,
                               stdout=stdout, stderr=stderr, startupinfo=startup)
    (args.output / 'probe-pid.txt').write_text(str(process.pid), encoding='utf-8')
    code = process.wait(timeout=180)  # Timeout leaves the same child alive for inspection.
report = (run / 'resolver-probe.log').read_text(encoding='utf-8')
unchanged = all(hashlib.sha256(Path(path).read_bytes()).hexdigest() == value for path, value in original.items())
stderr_empty = (args.output / 'stderr.log').stat().st_size == 0
passed = code == 0 and unchanged and stderr_empty and ('PREVIEW_PASS\n' if args.preview else 'RESOLVER_PASS\n') in report and 'FAILED\t' not in report
if args.commit:
    passed = passed and all(marker in report for marker in (
        'TERRAIN_RESERVATION_FAILURES_PASS\t', 'TERRAIN_COMMIT_FAILURES_PASS\t',
        'TERRAIN_COMMIT_SIZE_PASS\t100\t', 'TERRAIN_COMMIT_SIZE_PASS\t500\t', 'TERRAIN_SELECTED_COMMIT_PASS\t'))
verification = {'passed': passed, 'exit_code': code, 'input_assets_unchanged': unchanged,
                'scope': ('real loaded-client owned preview and atomic Commit/Undo/Redo core; no GUI preview/Commit wiring or G1 E2E' if args.commit else
                          'real loaded-client owned DrawOperations sandbox/diff; no commit/GUI-preview or G1 E2E' if args.preview else
                          'real loaded-client placements at 100x100/500x500; no drawing/commit or G1 E2E'),
                'report': report, 'source_assets': original,
                'stderr_empty': stderr_empty}
(args.output / 'verification.json').write_text(json.dumps(verification, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in verification.items() if key != 'source_assets'}, indent=2))
sys.exit(0 if passed else 1)
