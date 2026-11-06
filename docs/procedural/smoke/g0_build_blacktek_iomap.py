"""Build the isolated probe from an existing fresh non-unity BlackTek MSBuild tree.

Usage: python g0_build_blacktek_iomap.py SERVER_ROOT FRESH_BUILD_DIR VCVARSALL_BAT
Only writes probe artifacts in FRESH_BUILD_DIR. Never launches the server.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'build', 'vcvars'):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument('--source', type=lambda value: Path(value).resolve(),
                        help='Separate probe source for a newer server API; never server main')
    parser.add_argument('--vcvars-version', help='Match the existing MSBuild compiler, e.g. 14.44.35207')
    args = parser.parse_args()
    if args.vcvars_version and not re.fullmatch(r'\d+\.\d+(?:\.\d+)?', args.vcvars_version):
        parser.error('Compiler version must contain only numeric dotted components')
    vcvars_option = f' -vcvars_ver={args.vcvars_version}' if args.vcvars_version else ''
    source = args.source or Path(__file__).with_name('g0_blacktek_iomap_probe.cpp').resolve()
    objects = sorted(path for path in (args.build / 'obj').glob('*.obj')
                     if path.name.lower() != 'otserv.obj' and not path.name.lower().startswith('unity_'))
    if not objects or not any(path.name.lower() == 'iomap.obj' for path in objects):
        raise RuntimeError('Fresh per-source IOMap objects required')
    for obj in objects:
        cpp = args.server / 'src' / (obj.stem + '.cpp')
        if not cpp.exists() or cpp.stat().st_mtime_ns > obj.stat().st_mtime_ns:
            raise RuntimeError(f'Missing/newer source for {obj.name}; rebuild BlackTek first')
    headers = list((args.server / 'src').rglob('*.h'))
    if any(header.stat().st_mtime_ns > min(obj.stat().st_mtime_ns for obj in objects)
           for header in headers):
        raise RuntimeError('Headers newer than build; rebuild BlackTek first')
    deps = args.server / 'vcpkg_installed' / 'x64-windows' / 'x64-windows'
    response = args.build / 'probe_objects.rsp'
    response.write_text('\n'.join(f'"{path}"' for path in objects + [
        args.build / 'otserv_probe.obj', args.build / 'iomap_probe.obj']), encoding='utf-8')
    flags = ('/nologo /c /EHsc /std:c++latest /utf-8 /MD /DNDEBUG /D_MBCS '
             f'/openmp /bigobj /O2 /arch:AVX /I"{args.server / "src"}" /I"{deps / "include"}"')
    command = args.build / 'build_iomap_probe.cmd'
    command.write_text(
        '@echo off\nsetlocal\n'
        f'call "{args.vcvars}" x64{vcvars_option} >nul\nif errorlevel 1 exit /b 1\n'
        f'cl {flags} /Dmain=blacktek_unused_server_main "{args.server / "src" / "otserv.cpp"}" '
        f'/Fo"{args.build / "otserv_probe.obj"}"\nif errorlevel 1 exit /b 1\n'
        f'cl {flags} "{source}" /Fo"{args.build / "iomap_probe.obj"}"\nif errorlevel 1 exit /b 1\n'
        'link /nologo /subsystem:console /machine:x64 '
        f'/out:"{args.build / "iomap_probe.exe"}" @"{response}" '
        f'/libpath:"{deps / "lib"}" /libpath:"{deps / "lib" / "manual-link"}" "{deps / "lib" / "*.lib"}" '
        'kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib '
        'ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib\nexit /b %errorlevel%\n',
        encoding='utf-8')
    with (args.build / 'build_iomap_probe.log').open('w', encoding='utf-8') as log:
        subprocess.run(['cmd', '/c', str(command)], check=True, stdout=log, stderr=subprocess.STDOUT)
    manifest = {'probe_source_sha256': digest(source), 'vcvars_version': args.vcvars_version,
                'probe_exe_sha256': digest(args.build / 'iomap_probe.exe'),
                'objects': {path.name: digest(path) for path in objects},
                'sources': {str(path.relative_to(args.server)): digest(path)
                            for path in sorted((args.server / 'src').rglob('*'))
                            if path.suffix in ('.h', '.cpp')},
                'excluded': ['otserv.obj', 'unity_*.obj'],
                'server_entry_point': 'renamed, never called'}
    (args.build / 'probe_build_manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(f'PASS: probe linked against {len(objects)} regular fresh objects; manifest saved')


if __name__ == '__main__':
    main()
