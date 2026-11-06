"""Link a console test with current RME objects and a private renamed entry point."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=lambda value: Path(value).resolve())
parser.add_argument('--source', type=lambda value: Path(value).resolve())
parser.add_argument('--application-entry', action='store_true', help='Use normal RME entry with a wxModule probe')
parser.add_argument('--symbols', action='store_true', help='Generate a private PDB for runtime diagnostics')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
build = repo / 'build-ninja'
out = args.output
database = json.loads((build / 'compile_commands.json').read_text())
block = (build / 'build.ninja').read_text().split('build rme.exe:', 1)[1].split('\n\n', 1)[0]
inputs = block.splitlines()[0].split(' | ', 1)[0].split()[1:]
app_record = next(entry for entry in database if entry['file'].replace('\\', '/').endswith('/app/application.cpp'))
template = next(entry for entry in database if entry['file'].replace('\\', '/').endswith('/lua_api_app.cpp'))
app_output = str(Path(app_record['output']).relative_to(build)).replace('/', '\\')
assert app_output in inputs
records = [entry for entry in database if str(Path(entry['output']).relative_to(build)).replace('/', '\\') in inputs]
for entry in records:
    if Path(entry['file']).stat().st_mtime_ns > Path(entry['output']).stat().st_mtime_ns:
        raise RuntimeError('Stale object: ' + entry['output'])

def compile_command(record, source, obj, extra=''):
    command = re.sub(r'/Fo\S+', '/Fo' + str(obj).replace('\\', '/'), record['command'])
    command = re.sub(r'/Fd\S+', '/Fd' + str(out / 'compiler.pdb').replace('\\', '/'), command)
    return command[:command.rfind(' -c ')] + extra + ' -c "' + str(source).replace('\\', '/') + '"'

probe_source = args.source or repo / 'tests/test_procedural_ground_brush.cpp'
probe_hash = hashlib.sha256(probe_source.read_bytes()).hexdigest()
source_hashes = {str(Path(entry['file']).relative_to(repo)): hashlib.sha256(Path(entry['file']).read_bytes()).hexdigest()
                 for entry in records}
probe_obj = out / 'input_probe.obj'
app_obj = out / 'application_test_entry.obj'
libs = re.search(r'^  LINK_LIBRARIES = (.+)$', block, re.M).group(1).replace('$:', ':')
objects = ([build / name for name in inputs] + [probe_obj]) if args.application_entry else \
          ([build / name for name in inputs if name != app_output] + [app_obj, probe_obj])
rsp = out / 'input_probe.rsp'
rsp.write_text('\n'.join('"' + str(path) + '"' for path in objects) + '\n' + libs, encoding='utf-8')
cmd = out / 'build_probe.cmd'
symbol_options = f' /DEBUG:FULL /OPT:REF /OPT:ICF /PDB:"{out / "snapshot_probe.pdb"}"' if args.symbols else ''
entry_compile = '' if args.application_entry else compile_command(app_record, app_record['file'], app_obj,
                ' /Dmain=rme_unused_test_entry') + '\nif errorlevel 1 exit /b 1\n'
cmd.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 >nul\n'
    f'cd /d "{build}"\n' + entry_compile + compile_command(template, probe_source, probe_obj) +
    '\nif errorlevel 1 exit /b 1\n' +
    f'link /nologo @"{rsp}" /out:"{out / "snapshot_probe.exe"}" /machine:x64 /INCREMENTAL:NO /subsystem:console /LTCG{symbol_options}\nexit /b %errorlevel%\n', encoding='utf-8')
with (out / 'build.log').open('w', encoding='utf-8') as log:
    subprocess.run(['cmd', '/c', str(cmd)], stdout=log, stderr=subprocess.STDOUT, check=True)
if hashlib.sha256(probe_source.read_bytes()).hexdigest() != probe_hash or any(
        hashlib.sha256((repo / name).read_bytes()).hexdigest() != value for name, value in source_hashes.items()):
    raise RuntimeError('A probe or linked source changed during build; use a new output and rebuild')
manifest = {
    'entry_override': 'normal application entry' if args.application_entry else
                      'private application.cpp object compiled with /Dmain=rme_unused_test_entry; no production file edits',
    'probe_source': str(probe_source), 'probe_source_sha256': probe_hash,
    'sources': source_hashes,
    'objects': {str(path.relative_to(build)) if path.is_relative_to(build) else path.name:
                hashlib.sha256(path.read_bytes()).hexdigest() for path in objects},
    'exe_sha256': hashlib.sha256((out / 'snapshot_probe.exe').read_bytes()).hexdigest(),
    'normal_exe_sha256': hashlib.sha256((build / 'rme.exe').read_bytes()).hexdigest(),
}
(out / 'build-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
