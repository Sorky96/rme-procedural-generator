"""G3-A runner: import a private COPY of a reference OTBM with the private probe EXE.

Arguments: OUTPUT_DIR MAP_FILE PROFILE_DATA CONFIG_TOML
The source map and its companion XML are hashed before and after and never opened for writing.
OUTPUT_DIR must be new and live under local-data/ (git-ignored); it receives the copy and the report.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
for name in ('output', 'map', 'profile_data', 'config'):
    parser.add_argument(name, type=lambda value: Path(value).resolve())
parser.add_argument('--exe', type=lambda value: Path(value).resolve(), required=True)
args = parser.parse_args()

if not args.output.is_relative_to(repo / 'local-data'):
    raise RuntimeError('Output must be under local-data/ (git-ignored)')
if args.output.exists():
    raise RuntimeError('Refusing to reuse an output directory')

stem = args.map.stem
companions = [p for p in args.map.parent.iterdir() if p.is_file() and p.name.startswith(stem + '-') and p.suffix == '.xml']


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def inputs():
    return {p.name: digest(p) for p in [args.map, *sorted(companions)]}


original = inputs()
run = args.output / 'run'
run.mkdir(parents=True)
data = run / 'data'
shutil.copytree(args.profile_data / '1098', data / '1098')
for path in args.profile_data.iterdir():
    if path.is_file():
        shutil.copy2(path, data / path.name)
shutil.copy2(args.map, run / 'reference.otbm')
config = args.config.read_text(encoding='utf-8')
config = re.sub(r'^recent_edited_map_path = .*?$', "recent_edited_map_path = ''", config, flags=re.M)
config = re.sub(r'^recent_files = \[[\s\S]*?^\]', 'recent_files = []', config, flags=re.M)
config = re.sub(r"^data_directory = .*?$", "data_directory = '" + data.as_posix() + "/'", config, flags=re.M)
config = re.sub(r'^use_custom_data_directory = .*?$', 'use_custom_data_directory = 1', config, flags=re.M)
config = re.sub(r'^indirectory_installation = .*?$', 'indirectory_installation = 1', config, flags=re.M)
for key in ('only_one_instance', 'welcome_dialog', 'use_updater', 'goto_website_on_boot'):
    config = re.sub(r'^' + key + r' = .*?$', key + ' = false', config, flags=re.M)
(run / 'config.toml').write_text(config, encoding='utf-8')
(run / '.g3-reference-run').write_text('private copy; read-only import', encoding='utf-8')
if not (args.output / 'assets').exists():
    shutil.copytree(repo / 'build-ninja/assets', args.output / 'assets')
shutil.copy2(args.exe, args.output / args.exe.name)
# The builder links the probe without rme.exe's manifest; without Common Controls v6 wxWidgets
# shows a modal warning at startup that blocks an unattended run. Copy the manifest from rme.exe.
manifest = args.output / 'rme.manifest'
mt = sorted(Path('C:/Program Files (x86)/Windows Kits/10/bin').glob('*/x64/mt.exe'))
if not mt:
    raise RuntimeError('mt.exe (Windows SDK) not found; the probe would show a blocking wx dialog')
probe_exe = args.output / args.exe.name
for command in ([str(mt[-1]), '-nologo', f'-inputresource:{repo / "build-ninja/rme.exe"};#1', f'-out:{manifest}'],
                [str(mt[-1]), '-nologo', '-manifest', str(manifest), f'-outputresource:{probe_exe};#1']):
    embed = subprocess.run(command, capture_output=True, text=True)
    if embed.returncode != 0:
        raise RuntimeError('Manifest embedding failed: ' + embed.stdout + embed.stderr)
environment = dict(os.environ)
environment['PATH'] = str(repo / 'build-ninja') + os.pathsep + environment['PATH']
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
with (args.output / 'stdout.log').open('w') as out, (args.output / 'stderr.log').open('w') as err:
    process = subprocess.Popen([str(args.output / args.exe.name)], cwd=run, env=environment, stdout=out, stderr=err, startupinfo=startup)
    try:
        code = process.wait(timeout=300)
    except subprocess.TimeoutExpired:
        raise RuntimeError(f'Probe still running as PID {process.pid}; inspect it before doing anything else')
if inputs() != original:
    raise AssertionError('Source map or companions changed')
log = (run / 'g3-probe.log').read_text(encoding='utf-8')
if code != 0 or 'IMPORT_PASS' not in log:
    raise RuntimeError(f'Probe exit {code}: {log}')
(args.output / 'verification.json').write_text(json.dumps({'result': 'PASS', 'exit_code': code, 'source_hashes_unchanged': original}, indent=2), encoding='utf-8')
print('PASS', json.dumps(original))
