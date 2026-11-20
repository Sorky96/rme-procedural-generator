"""Frozen I53 1001..1032, N=1, side=512, I51 settings with --g5-levels.
py -I g6_measure_copy.py CANDIDATE_RANK_EXE PROFILE_DIR INDEX SCRATCH_DIR
Two full runs, deterministic byte comparison; all hits retained, no holdout read.
"""
import argparse
import csv
import hashlib
import io
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'profiles', 'index', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    if 'scratchpad' not in args.output.resolve().parts:
        parser.error('output must be in scratchpad')
    args.output.mkdir(parents=True, exist_ok=True)
    reports = {}
    for name, length, density, rock in (('m06', 40, 30, 0), ('m19', 80, 40, 1)):
        command = [str(args.exe.resolve()), str((args.profiles / f'{name}.profile').resolve()), '512', '1001', '1032',
                   '--count', '1', '--path-length', str(length), '--building-density', str(density),
                   '--rock-blocks', str(rock), '--g5-levels', '--copy-index', str(args.index.resolve()),
                   '--bands', str(Path(__file__).resolve().parents[1] / f'g6-bands-v1/{name}.bands')]
        runs = []
        for run in (1, 2):
            result = subprocess.run(command, capture_output=True)
            (args.output / f'{name}-run{run}.tsv').write_bytes(result.stdout)
            (args.output / f'{name}-run{run}.stderr').write_bytes(result.stderr)
            if result.returncode or result.stderr:
                raise RuntimeError(f'{name}: exit={result.returncode}: {result.stderr.decode(errors="replace")}')
            runs.append(result.stdout)
            print(f'{name} run {run} complete', flush=True)
        if runs[0] != runs[1]:
            raise RuntimeError('I53_REPEATABILITY_FAIL')
        lines = runs[0].decode().splitlines()
        start = next(i for i, line in enumerate(lines) if line.startswith('userSeed\t'))
        rows = list(csv.DictReader(io.StringIO('\n'.join([lines[start], *(s for s in lines[start + 1:] if s.split('\t')[0].isdigit())])), delimiter='\t'))
        if [int(r['userSeed']) for r in rows] != list(range(1001, 1033)) or any(r['index'] != '0' or r['seed'] != r['userSeed'] for r in rows):
            raise ValueError('incomplete or wrong matrix')
        reports[name] = {'sha256': hashlib.sha256(runs[0]).hexdigest(), 'candidates': len(rows),
                         'hits': [r for r in rows if r['copyState'].startswith('copy thresholds')],
                         'notEvaluated': [r for r in rows if r['copyState'].startswith('not-evaluated')],
                         'hardFails': sum(r['hardFail'] == 'true' for r in rows)}
    report = {'schema': 'i53-matrix/1', 'preregistration': '87e2246', 'result': 'I53_REPEATABILITY_PASS', 'profiles': reports}
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
