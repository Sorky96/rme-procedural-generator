"""Run the real BlackTek IOMap harness on private copies, never server startup.

Usage: python g0_blacktek_iomap_cases.py PROBE_EXE BT_DEPENDENCY_BIN DAT FIXTURE_DIR
FIXTURE_DIR contains the RME-saved house-roundtrip OTBM and two XML files.
The probe requires the specific synthetic fixture documented in TEST_PLAN.md.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from contextlib import nullcontext


FILES = ('house-roundtrip.otbm', 'house-roundtrip-house.xml',
         'house-roundtrip-spawn.xml')


def hashes(directory):
    return {name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
            for name in FILES}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('probe', 'dependency_bin', 'dat', 'fixture'):
        parser.add_argument(name, type=lambda value: Path(value).resolve())
    parser.add_argument('--report', type=Path)
    parser.add_argument('--current-zones', action='store_true',
                        help='Current Zones probe: guarded private conversion outputs, no server startup')
    args = parser.parse_args()
    if args.current_zones and not args.report:
        parser.error('--current-zones requires --report to preserve all private case artifacts')
    if args.current_zones:
        case_root = args.report.resolve().parent / 'cases'
        case_root.mkdir()  # Refuse reuse; retain each current-server conversion and failure.
    original = hashes(args.fixture)
    if args.current_zones and original != {
        FILES[0]: 'b0f4c1465695c841b4488ff35abe4ef2a1b125997467e33659954c5036418564',
        FILES[1]: '050b6e04e63b3d79da20e45bef6ad81d4bac74c42fef6acb65dc65ec0d93a141',
        FILES[2]: 'dc85b6835770ba8665e534339324e655d5c23693616b9ca726d2a49bfa6ad8b6',
    }:
        raise AssertionError('Current probe requires the exact approved synthetic fixture bytes')
    environment = dict(os.environ)
    environment['PATH'] = str(args.dependency_bin) + os.pathsep + environment['PATH']
    cases = [('valid', 0), ('missing-house', 5), ('missing-spawn', 6),
             ('wrong-rent', 9), ('empty-spawns', 9), ('truncated-map', 4)]
    report = {'fixture_sha256': original,
              'probe_sha256': hashlib.sha256(args.probe.read_bytes()).hexdigest(),
              'dat_sha256': hashlib.sha256(args.dat.read_bytes()).hexdigest(), 'cases': []}
    if args.current_zones:
        if report['dat_sha256'] != '14bc3d04b8167744e3207a09da1ae1fecb89d4222e50ed93850defee032458cb':
            raise AssertionError('Current probe requires the approved 1098 DAT bytes')
        report['mode'] = 'current-zones-conversion'
        report['boundary'] = 'No server startup, activation, database or monster instantiation; fixed padding 2 replaces legacy radius/direction'
    for name, expected in cases:
        context = nullcontext(case_root / name) if args.current_zones else tempfile.TemporaryDirectory(prefix='rme-g0-iomap-')
        with context as temporary:
            directory = Path(temporary)
            if args.current_zones:
                directory.mkdir()
                (directory / '.g0-current-iomap').write_text('Private current IOMap/Zones fixture', encoding='utf-8')
            for filename in FILES:
                (directory / filename).write_bytes((args.fixture / filename).read_bytes())
            (directory / 'config').mkdir()
            monster = directory / 'data' / 'monster'
            monster.mkdir(parents=True)
            (monster / 'monsters.xml').write_text(
                '<monsters><monster name="Rat" file="rat.xml" /></monsters>', encoding='utf-8')
            (monster / 'rat.xml').write_text(
                '<monster name="Rat" race="blood" speed="100">'
                '<health now="20" max="20" /></monster>', encoding='utf-8')
            if name == 'missing-house':
                (directory / FILES[1]).unlink()
            elif name == 'missing-spawn':
                (directory / FILES[2]).unlink()
            elif name == 'wrong-rent':
                path = directory / FILES[1]
                text = path.read_text(encoding='utf-8')
                if 'rent="4321"' not in text:
                    raise AssertionError('Expected rent fixture missing')
                path.write_text(text.replace('rent="4321"', 'rent="4322"'), encoding='utf-8')
            elif name == 'empty-spawns':
                (directory / FILES[2]).write_text('<spawns/>', encoding='utf-8')
            elif name == 'truncated-map':
                path = directory / FILES[0]
                path.write_bytes(path.read_bytes()[:20])
            before = {p.name: p.read_bytes() for p in directory.iterdir() if p.is_file()}
            result = subprocess.run([str(args.probe), str(args.dat), str(directory / FILES[0])],
                                    cwd=directory, env=environment, capture_output=True,
                                    text=True, errors='replace', timeout=30)
            if args.current_zones:
                (args.report.resolve().parent / (name + '-process.json')).write_text(json.dumps({
                    'case': name, 'exit': result.returncode, 'expected': expected,
                    'stdout': result.stdout, 'stderr': result.stderr, 'case_directory': str(directory)
                }, indent=2), encoding='utf-8')
            after = {p.name: p.read_bytes() for p in directory.iterdir() if p.is_file()}
            conversion = {}
            if args.current_zones:
                allowed_extra = {FILES[2] + '.converted'} if name == 'valid' else set()
                if set(after) - set(before) != allowed_extra:
                    raise AssertionError('Unexpected root-level conversion output')
                output_root = directory / 'data/world/converted/g0-private'
                generated = {p.relative_to(output_root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in output_root.rglob('*') if p.is_file()}
                expected_generated = set()
                if name in ('valid', 'missing-spawn', 'empty-spawns'):
                    expected_generated = {'house-roundtrip.otbm', 'g0-private-zones/legacy-flags.toml'}
                if name == 'valid':
                    expected_generated.add('g0-private-zones/floor-7.toml')
                if set(generated) != expected_generated:
                    raise AssertionError('Unexpected conversion output set: ' + repr(generated))
                conversion = {'generated_sha256': generated,
                              'marker_created': FILES[2] + '.converted' in after}
                after = {key: after[key] for key in before if key in after}
            record = {'case': name, 'expected': expected, 'exit': result.returncode,
                      'stdout': result.stdout, 'stderr': result.stderr,
                      'fixture_unchanged': before == after}
            if args.current_zones:
                record['conversion'] = conversion
            report['cases'].append(record)
            if args.current_zones and args.report:
                args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
            if result.returncode != expected or before != after:
                print(json.dumps(report, indent=2))
                raise AssertionError(f'{name}: exit/content mismatch')
    if hashes(args.fixture) != original:
        raise AssertionError('Source fixture changed')
    if args.current_zones and hashlib.sha256(args.dat.read_bytes()).hexdigest() != report['dat_sha256']:
        raise AssertionError('Source DAT changed')
    output = json.dumps(report, indent=2)
    if args.report:
        args.report.write_text(output, encoding='utf-8')
    print(output)


if __name__ == '__main__':
    main()
