"""I51: run the full candidate matrix twice, compare bytes, retain reports outside local-data.

py -I docs/procedural/smoke/g6_measure_candidates.py EXE PROFILE_DIR SCRATCH_DIR
    [--bands-dir DIR] [--g5-levels]

Uses the frozen, versioned 396e057 bands by default. Holdout evaluation is separate.
"""
import argparse
import hashlib
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def merge_reports(reports):
    """Join independent contiguous seed blocks, retaining exactly one identical preamble."""
    preamble, bodies = None, []
    for report in reports:
        lines = report.splitlines(keepends=True)
        header = next(i for i, line in enumerate(lines) if line.startswith(b'userSeed\t'))
        prefix = b''.join(lines[:header + 1])
        if preamble is not None and prefix != preamble:
            raise RuntimeError('seed blocks disagree on settings, bands or columns')
        preamble = prefix
        bodies.append(b''.join(lines[header + 1:]))
    return preamble + b''.join(bodies)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('profile_dir', type=Path)
    parser.add_argument('scratch_dir', type=Path)
    parser.add_argument('--bands-dir', type=Path, default=Path(__file__).resolve().parents[1] / 'g6-bands-v1')
    parser.add_argument('--g5-levels', action='store_true')
    args = parser.parse_args()
    args.scratch_dir.mkdir(parents=True, exist_ok=True)

    def measure(spec):
        name, length, density, rock = spec
        command = [str(args.exe.resolve()), str((args.profile_dir / f'{name}.profile').resolve()), '512', '1001', '1032',
                   '--count', '16', '--path-length', str(length), '--building-density', str(density), '--rock-blocks', str(rock)]
        command += ['--bands', str((args.bands_dir / f'{name}.bands').resolve())]
        if args.g5_levels:
            command += ['--g5-levels']
        reports = []
        for run in (1, 2):
            def block(first):
                block_command = command.copy()
                block_command[3:5] = [str(first), str(first + 7)]
                result = subprocess.run(block_command, capture_output=True)
                prefix = args.scratch_dir / f'{name}-run{run}-seeds{first}-{first + 7}'
                prefix.with_suffix('.tsv').write_bytes(result.stdout)
                prefix.with_suffix('.stderr').write_bytes(result.stderr)
                if result.returncode or result.stderr:
                    raise RuntimeError(f'{name} run {run}, seed {first}: exit {result.returncode}, {result.stderr.decode(errors="replace")}')
                print(f'{name} run {run}: seeds {first}-{first + 7} complete', flush=True)
                return result.stdout
            # Worlds are independent per seed. Restore canonical order after bounded parallel execution.
            with ThreadPoolExecutor(max_workers=4) as blocks:
                report = merge_reports(list(blocks.map(block, range(1001, 1033, 8))))
            (args.scratch_dir / f'{name}-run{run}.tsv').write_bytes(report)
            (args.scratch_dir / f'{name}-run{run}.stderr').write_bytes(b'')
            records = [line for line in report.splitlines() if line.split(b'\t', 1)[0].isdigit()]
            if len(records) != 32 * 16:
                raise RuntimeError(f'{name}: expected 512 candidates, got {len(records)}')
            reports.append(report)
            print(f'{name} run {run}: 512 candidates complete', flush=True)
        if reports[0] != reports[1]:
            raise RuntimeError(f'{name}: REPEATABILITY_FAIL')
        print(f'{name}: REPEATABILITY_PASS sha256={hashlib.sha256(reports[0]).hexdigest()}', flush=True)

    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(measure, [('m06', 40, 30, 0), ('m19', 80, 40, 1)]))
    print('RANKING_REPEATABILITY_PASS (holdout threshold evaluated separately)')


if __name__ == '__main__':
    main()
