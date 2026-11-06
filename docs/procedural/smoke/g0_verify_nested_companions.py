"""Verify a completed private nested-path probe without restarting its process."""
import argparse
import hashlib
import json
from pathlib import Path
from g0_compare_roundtrip import read_tiles
from g0_verify_snapshot import canonical


def verify(output, fixture):
    repo = Path(__file__).resolve().parents[3]
    if not output.is_relative_to(repo / 'local-data/procedural-smoke'):
        raise AssertionError('Private procedural-smoke output required')
    run = output / 'run'
    if not (run / '.g0-nested-companions').is_file():
        raise AssertionError('Nested probe guard missing')
    observed = json.loads((output / 'observed-exit.json').read_text())
    if observed['exit_code'] != 0 or observed['pid'] != int((output / 'probe-pid.txt').read_text()):
        raise AssertionError('A matching observed successful child exit is required')
    manifest = json.loads((output / 'build-manifest.json').read_text())
    if hashlib.sha256((output / 'snapshot_probe.exe').read_bytes()).hexdigest() != manifest['exe_sha256']:
        raise AssertionError('Probe does not match its build manifest')
    expected = json.loads((run / 'inputs-original.json').read_text())
    for directory in (run, fixture):
        if any(hashlib.sha256((directory / name).read_bytes()).hexdigest() != digest for name, digest in expected.items()):
            raise AssertionError('Original or copied input changed')
    lines = (run / 'snapshot-probe.log').read_text().splitlines()
    required = ('FIRST_SAVE_MARKER_GUARD_PASS', 'FIRST_SAVE_VERSIONED_MARKER_PASS', 'FIRST_SAVE_ROLLBACK_PASS',
                'FIRST_SAVE_SUCCESS_PASS\t', 'NESTED_RELATIVE_PASS\t', 'NESTED_ROLLBACK_PASS',
                'NESTED_ABSOLUTE_PASS', 'NESTED_CONSTRAINED_PASS\t', 'FIRST_SAVE_PASS')
    if not all(any(line == prefix or line.startswith(prefix) for line in lines) for prefix in required) or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('Required runtime assertions did not finish')
    for path, companions in ((run / 'first-save-success/first.otbm', None),
            (run / 'nested-map/nested.otbm', {11: 'nested/spawns.xml', 13: 'outside-house.xml'})):
        _, tree, xml = canonical(path, expected_companions=companions)
        if read_tiles(tree) or len(xml) != 2:
            raise AssertionError('Expected empty map and complete actual companion set')
        # Both documents must be empty and have their expected domain roots.
        for tag, value in xml.items():
            if value[0] != ('spawns' if tag == 11 else 'houses') or value[3]:
                raise AssertionError('Wrong or nonempty companion XML')
    if (run / 'outside-house.xml').read_bytes() != b'PRIVATE_OUTSIDE_SENTINEL':
        raise AssertionError('Outside private sentinel changed')
    if (run / 'data/user/data/.saving.txt').exists() or list((run / 'nested-map').rglob('*~')):
        raise AssertionError('Nested test left recovery marker or temporary backups')
    if (output / 'stderr.log').read_text():
        raise AssertionError('Nested test has unreviewed diagnostics')
    result = dict(result='PASS', observed_child_exit_code=0, independent_exact_relative_header_paths=True,
        independent_empty_map_and_xml_check=True, source_hashes_unchanged=expected,
        runtime_assertions=list(required), build_manifest=manifest,
        scope='First save, nested relative/absolute XML, actual failed nested restore, constrained outside path')
    (output / 'verification-final.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=lambda value: Path(value).resolve())
    parser.add_argument('fixture', type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    print(json.dumps(verify(args.output, args.fixture), indent=2))
