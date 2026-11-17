"""I51: run the full candidate matrix twice, compare bytes, retain reports outside local-data.

py -I docs/procedural/smoke/g6_measure_candidates.py EXE PROFILE_DIR SCRATCH_DIR
    [--bands-dir DIR] [--g5-levels]

Without reviewed <profile>.bands inputs this measures repeatability only, NOT an I51 ranking gate pass.
The script never derives references/bands or assigns holdout maps to styles.
"""
import argparse
import hashlib
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('profile_dir', type=Path)
    parser.add_argument('scratch_dir', type=Path)
    parser.add_argument('--bands-dir', type=Path)
    parser.add_argument('--g5-levels', action='store_true')
    args = parser.parse_args()
    args.scratch_dir.mkdir(parents=True, exist_ok=True)

    def measure(spec):
        name, length, density, rock = spec
        command = [str(args.exe.resolve()), str((args.profile_dir / f'{name}.profile').resolve()), '512', '1001', '1032',
                   '--count', '16', '--path-length', str(length), '--building-density', str(density), '--rock-blocks', str(rock)]
        if args.bands_dir:
            command += ['--bands', str((args.bands_dir / f'{name}.bands').resolve())]
        if args.g5_levels:
            command += ['--g5-levels']
        reports = []
        for run in (1, 2):
            result = subprocess.run(command, capture_output=True)
            (args.scratch_dir / f'{name}-run{run}.tsv').write_bytes(result.stdout)
            (args.scratch_dir / f'{name}-run{run}.stderr').write_bytes(result.stderr)
            if result.returncode or result.stderr:
                raise RuntimeError(f'{name} run {run}: exit {result.returncode}, {result.stderr.decode(errors="replace")}')
            records = [line for line in result.stdout.splitlines() if line.split(b'\t', 1)[0].isdigit()]
            if len(records) != 32 * 16:
                raise RuntimeError(f'{name}: expected 512 candidates, got {len(records)}')
            reports.append(result.stdout)
            print(f'{name} run {run}: 512 candidates complete', flush=True)
        if reports[0] != reports[1]:
            raise RuntimeError(f'{name}: REPEATABILITY_FAIL')
        print(f'{name}: REPEATABILITY_PASS sha256={hashlib.sha256(reports[0]).hexdigest()}', flush=True)

    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(measure, [('m06', 40, 30, 0), ('m19', 80, 40, 1)]))
    print('RANKING_REPEATABILITY_PASS' if args.bands_dir else 'MEASUREMENT_REPEATABILITY_PASS (ranking policy missing; I51 not passed)')


if __name__ == '__main__':
    main()
