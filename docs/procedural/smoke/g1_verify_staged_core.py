"""Verify the current private G1 core regression and compare its deterministic results."""

import argparse
import hashlib
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def rows(report, marker):
    return [line.split('\t')[1:] for line in report.splitlines() if line.startswith(marker + '\t')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    base = repo / 'local-data/procedural-smoke'
    run = args.run
    require(run.is_relative_to(base), 'Expected a private smoke directory')
    result = json.loads((run / 'verification.json').read_text())
    manifest = json.loads((run / 'build-manifest.json').read_text())
    report = (run / 'run/resolver-probe.log').read_text(encoding='utf-8')
    stderr = (run / 'stderr.log').read_text(encoding='utf-8', errors='replace').splitlines()
    focus_only = bool(stderr) and all("'SetFocus' failed with error 0x00000057" in line
                                     and '\\src\\msw\\window.cpp(' in line for line in stderr)
    require(result['exit_code'] == 0 and result['input_assets_unchanged'], 'Runtime or assets failed')
    require(result['passed'] or focus_only, 'Unexpected runtime failure')
    require(result['report'] == report and report.endswith('PREVIEW_PASS\n') and 'FAILED\t' not in report,
            'Core regression incomplete')
    require(manifest['entry_override'] == 'normal application entry', 'Wrong entry point')
    for name, expected in manifest['sources'].items():
        require(sha(repo / name) == expected, f'Changed source {name}')
    for name, expected in manifest['objects'].items():
        path = run / name if name == 'input_probe.obj' else repo / 'build-ninja' / name
        require(sha(path) == expected, f'Changed object {name}')
    for path, expected in result['source_assets'].items():
        require(sha(Path(path)) == expected, f'Changed asset {path}')
    require(sha(Path(manifest['probe_source'])) == manifest['probe_source_sha256'], 'Changed probe source')
    require(sha(run / 'snapshot_probe.exe') == manifest['exe_sha256'], 'Changed probe executable')
    require(sha(repo / 'build-ninja/rme.exe') == manifest['normal_exe_sha256'], 'Changed normal executable')

    prior = base / 'g1-commit-complete-rme-20261103/run/resolver-probe.log'
    historical = prior.read_text(encoding='utf-8')
    current_markers = {line.split('\t', 1)[0] for line in report.splitlines()}
    prior_checks = {line.split('\t', 1)[0] for line in historical.splitlines()
                    if line.split('\t', 1)[0].endswith('_PASS')}
    require(prior_checks <= current_markers, f'Missing core checks: {sorted(prior_checks-current_markers)}')
    for marker, key_fields in (('PREVIEW_SIZE_PASS', 2), ('PREPARATION_SIZE_PASS', 1),
                               ('EXACT_SELECTION_SIZE_PASS', 1), ('TERRAIN_COMMIT_SIZE_PASS', 1)):
        current = {tuple(row[:key_fields]): row[-1] for row in rows(report, marker)}
        previous = {tuple(row[:key_fields]): row[-1] for row in rows(historical, marker)}
        expected_count = 4 if marker == 'PREVIEW_SIZE_PASS' else 2
        require(len(current) == expected_count and current == previous, f'Fingerprint drift: {marker}')
    checks = ('PREVIEW_FAILURES_PASS', 'PREPARATION_FAILURES_PASS', 'TERRAIN_COMMIT_FAILURES_PASS',
              'TERRAIN_RESERVATION_FAILURES_PASS', 'PREVIEW_MASK_EPOCH_PASS')
    counts = {marker: rows(report, marker) for marker in checks}
    for marker, fields in counts.items():
        old_fields = rows(historical, marker)
        require(len(fields) == len(old_fields) == 1 and int(fields[0][0]) >= int(old_fields[0][0]),
                f'Incomplete regression: {marker}')
    output = {
        'result': 'PARTIAL_G1_STAGED_CORE_FOCUS_WARNING' if focus_only else 'PASS_G1_STAGED_CORE_ONLY',
        'g1_gate': 'IN PROGRESS',
        'rme_objects_checked': len(manifest['objects']) - 1,
        'sources_checked': len(manifest['sources']),
        'normal_exe_sha256': manifest['normal_exe_sha256'],
        'probe_exe_sha256': manifest['exe_sha256'],
        'historical_fingerprints_match': True,
        'counts': counts,
        'runtime_stderr': stderr,
    }
    (run / 'staged-core-verification.json').write_text(json.dumps(output, indent=2), encoding='utf-8')
    print(json.dumps(output, indent=2))


if __name__ == '__main__':
    main()
