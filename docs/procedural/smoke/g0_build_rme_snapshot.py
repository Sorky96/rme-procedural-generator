import hashlib
import argparse
import json
import pathlib
import re
import subprocess

root = pathlib.Path(__file__).resolve().parents[3]
build = root / 'build-ninja'
parser = argparse.ArgumentParser(description='Link a separate snapshot test executable from existing RME Ninja objects; never overwrites rme.exe')
parser.add_argument('output', type=lambda value: pathlib.Path(value).resolve())
parser.add_argument('--source', type=lambda value: pathlib.Path(value).resolve(),
                    default=pathlib.Path(__file__).with_name('g0_rme_snapshot_probe.cpp').resolve())
args = parser.parse_args()
probe = args.output
probe.mkdir(parents=True, exist_ok=True)
source = args.source
database = json.loads((build / 'compile_commands.json').read_text())
record = next(x for x in database if x['file'].replace('\\', '/').endswith('/lua_api_app.cpp'))
command = record['command']
command = re.sub(r'/Fo\S+', '/Fo' + str(probe / 'snapshot_probe.obj').replace('\\', '/'), command)
command = re.sub(r'/Fd\S+', '/Fd' + str(probe / 'compiler.pdb').replace('\\', '/'), command)
command = command[:command.rfind(' -c ')] + ' -c "' + str(source).replace('\\', '/') + '"'
text = (build / 'build.ninja').read_text()
block = text[text.index('build rme.exe:'):]
block = block[:block.index('\n\n')]
inputs = block.splitlines()[0].split(': ', 1)[1].split(' | ', 1)[0].split()[1:]
for entry in database:
    relative = str(pathlib.Path(entry['output']).relative_to(build)).replace('/', '\\')
    if relative in inputs:
        if pathlib.Path(entry['file']).stat().st_mtime_ns > pathlib.Path(entry['output']).stat().st_mtime_ns:
            raise RuntimeError('Stale RME object: ' + relative)
libs = re.search(r'^  LINK_LIBRARIES = (.+)$', block, re.M).group(1).replace('$:', ':')
rsp = probe / 'snapshot_probe.rsp'
rsp.write_text('\n'.join('"' + str(build / name) + '"' for name in inputs) + '\n"' +
               str(probe / 'snapshot_probe.obj') + '"\n' + libs, encoding='utf-8')
cmd = probe / 'build_probe.cmd'
cmd.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvarsall.bat" x64 >nul\n'
    f'cd /d "{build}"\n{command}\nif errorlevel 1 exit /b 1\n'
    f'link /nologo @"{rsp}" /out:"{probe / "snapshot_probe.exe"}" /machine:x64 /INCREMENTAL:NO /subsystem:console /LTCG\nexit /b %errorlevel%\n', encoding='utf-8')
with (probe / 'build.log').open('w', encoding='utf-8') as log:
    subprocess.run(['cmd', '/c', str(cmd)], stdout=log, stderr=subprocess.STDOUT, check=True)
manifest = {'objects': len(inputs), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'editor_persistence_source_sha256': hashlib.sha256((root / 'source/editor/persistence/editor_persistence.cpp').read_bytes()).hexdigest(),
            'editor_persistence_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/editor/persistence/editor_persistence.cpp.obj').read_bytes()).hexdigest(),
            'map_xml_io_source_sha256': hashlib.sha256((root / 'source/io/map_xml_io.cpp').read_bytes()).hexdigest(),
            'map_xml_io_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/io/map_xml_io.cpp.obj').read_bytes()).hexdigest(),
            'iomap_otbm_source_sha256': hashlib.sha256((root / 'source/io/iomap_otbm.cpp').read_bytes()).hexdigest(),
            'iomap_otbm_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/io/iomap_otbm.cpp.obj').read_bytes()).hexdigest(),
            'action_queue_source_sha256': hashlib.sha256((root / 'source/editor/action_queue.cpp').read_bytes()).hexdigest(),
            'action_queue_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/editor/action_queue.cpp.obj').read_bytes()).hexdigest(),
            'editor_source_sha256': hashlib.sha256((root / 'source/editor/editor.cpp').read_bytes()).hexdigest(),
            'editor_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/editor/editor.cpp.obj').read_bytes()).hexdigest(),
            'lua_api_app_source_sha256': hashlib.sha256((root / 'source/lua/lua_api_app.cpp').read_bytes()).hexdigest(),
            'lua_api_app_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/lua/lua_api_app.cpp.obj').read_bytes()).hexdigest(),
            'house_source_sha256': hashlib.sha256((root / 'source/game/house.cpp').read_bytes()).hexdigest(),
            'house_header_sha256': hashlib.sha256((root / 'source/game/house.h').read_bytes()).hexdigest(),
            'house_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/game/house.cpp.obj').read_bytes()).hexdigest(),
            'snapshot_service_source_sha256': hashlib.sha256((root / 'source/editor/persistence/map_backup_service.cpp').read_bytes()).hexdigest(),
            'snapshot_service_object_sha256': hashlib.sha256((build / 'CMakeFiles/rme.dir/source/editor/persistence/map_backup_service.cpp.obj').read_bytes()).hexdigest(),
            'base_rme_exe_sha256': hashlib.sha256((build / 'rme.exe').read_bytes()).hexdigest(),
            'exe_sha256': hashlib.sha256((probe / 'snapshot_probe.exe').read_bytes()).hexdigest()}
(probe / 'build-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(manifest)
