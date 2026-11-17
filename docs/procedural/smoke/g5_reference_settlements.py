"""Measure settlement structure (G4 iteration 12a) in every private reference map copy and write one table.

Runs g3_run_reference_import.py (with a probe built from g3_reference_import_probe.cpp) once per map and collects
analysis/settlements-surface.txt. Reference maps are read only through the runner, which hashes them before and
after; nothing is copied outside local-data/.

  py -I docs/procedural/smoke/g5_reference_settlements.py RUNS_DIR OUT_DIR --exe PROBE_EXE --client-path DIR --keymap TSV

RUNS_DIR holds M01..M19, each with run/reference.otbm, run/data and run/config.toml (an earlier import output).
"""
import argparse
import subprocess
import sys
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
for name in ('runs_dir', 'out_dir'):
    parser.add_argument(name, type=lambda value: Path(value).resolve())
parser.add_argument('--exe', type=lambda value: Path(value).resolve(), required=True)
parser.add_argument('--client-path', type=lambda value: Path(value).resolve(), required=True)
parser.add_argument('--keymap', type=lambda value: Path(value).resolve(), required=True)
parser.add_argument('--client-id', default='10.98')
parser.add_argument('--maps', default='', help='comma separated subset, e.g. M06,M19 (default: all found)')
args = parser.parse_args()
if not args.out_dir.is_relative_to(repo / 'local-data'):
    parser.error('OUT_DIR must be under local-data/')

wanted = set(filter(None, args.maps.split(',')))
maps = sorted(p.name for p in args.runs_dir.iterdir() if (p / 'run/reference.otbm').is_file() and (not wanted or p.name in wanted))
if not maps:
    sys.exit('no reference copies found')
args.out_dir.mkdir(parents=True, exist_ok=True)

columns = None
rows = []
for name in maps:
    run = args.runs_dir / name / 'run'
    out = args.out_dir / name
    if not out.exists():
        done = subprocess.run([sys.executable, '-I', str(repo / 'docs/procedural/smoke/g3_run_reference_import.py'), str(out), str(run / 'reference.otbm'),
                               str(run / 'data'), str(run / 'config.toml'), '--exe', str(args.exe), '--client-id', args.client_id,
                               '--client-path', str(args.client_path), '--max-file-mib', '256', '--keymap', str(args.keymap), '--name-unbrushed'],
                              capture_output=True, text=True)
        if done.returncode != 0:
            print(f'{name}: runner failed: {(done.stdout + done.stderr)[-300:]}')
            rows.append((name, None))
            continue
    report = out / 'run/analysis/settlements-surface.txt'
    values = dict(line.split('\t') for line in report.read_text(encoding='utf-8').splitlines() if line)
    columns = columns or list(values)
    rows.append((name, values))
    print(f'{name}: buildings={values["buildings"]}')

table = ['map\t' + '\t'.join(columns or [])]
for name, values in rows:
    table.append(name + '\t' + ('\t'.join(values[c] for c in columns) if values else 'FAILED'))
(args.out_dir / 'settlements-table.tsv').write_text('\n'.join(table) + '\n', encoding='utf-8')
print('\n'.join(table))
sys.exit(0 if all(values for _, values in rows) else 1)
