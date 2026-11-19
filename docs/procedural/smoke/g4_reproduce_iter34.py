"""D21: reproduce the G4 iteration 3 (style_styles) and iteration 4 (water_eval) tables with one command.

  py -I docs/procedural/smoke/g4_reproduce_iter34.py OUT_DIR [--local-data DIR] [--current-bin DIR]

The tools are rebuilt from the commits that produced the tables (style_styles 269da1b, water_eval 99f617c) with MSVC
alone: `git archive` of source/procedural/core and the tool's main file, one `cl` call, no CMake, no vcpkg.
Each tool runs twice on the same explicit local-data inputs. The script then checks that
  1. both runs are byte-identical,
  2. the output is byte-identical to the stored historical report (local-data/procedural-smoke/g4-fit),
  3. the numbers quoted in G4_GATE_STATUS.md ("Iteracja 3", "Iteracja 4") match the report.
Row 4a of the iteration 4 table came from a model that was never committed; it is checked only against the stored
water-4a.json and reported as not rerunnable. --current-bin runs the given (newer) binaries on the same inputs for
information only: their output is expected to differ for style_styles, because the generator changed after iteration 3.

Everything written goes to OUT_DIR: build/, runs/, params.json (inputs and tool provenance with SHA-256), summary.json.
The last line is D21_PASS or D21_FAIL. Historical values are only read, never written.
"""
import argparse
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import tarfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from g5_local_data import main_checkout_local_data

REPO = Path(__file__).resolve().parents[3]

# Inputs of the historical runs, relative to <local-data>/procedural-smoke. Chosen by timestamp: the stats folder is
# the only one with decor-context-surface.txt written before styles-1.json (2026-11-08 10:42), fp-c the last
# fingerprint set before it; water shapes come from the only shape folder written before water-1.json (12:22).
MANIFEST = 'g4-split/manifest.tsv'
FINGERPRINTS = 'g4-split/fp-c'
STATS = 'g4-dood-20261108b'
SHAPES = 'g4-water-20261108a'
HISTORICAL = {'styles': 'g4-fit/styles-1.json', 'water': 'g4-fit/water-4b.json', 'water_4a': 'g4-fit/water-4a.json'}
STATS_FILES = ('kind-stats-surface.txt', 'decor-context-surface.txt', 'doodad-brushes.csv', 'ground-keys-surface.csv')

TOOLS = {
    'style_styles': {'commit': '269da1b', 'historical': 'styles'},
    'water_eval': {'commit': '99f617c', 'historical': 'water'},
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def manifest_aliases(path: Path):
    """Aliases from the first column of a g4-manifest/1 file (schema line and '#' comments skipped)."""
    return [line.split('\t')[0] for line in path.read_text(encoding='utf-8').splitlines() if '\t' in line and not line.startswith('#')]


def input_files(smoke: Path):
    """Every input file the two tools read, as {relative path: Path}; missing ones included (checked later)."""
    files = {MANIFEST: smoke / MANIFEST}
    if not files[MANIFEST].is_file():
        return files
    for alias in manifest_aliases(files[MANIFEST]):
        files[f'{FINGERPRINTS}/{alias}.fp'] = smoke / FINGERPRINTS / f'{alias}.fp'
        for name in STATS_FILES:
            files[f'{STATS}/{alias}/run/analysis/{name}'] = smoke / STATS / alias / 'run/analysis' / name
        files[f'{SHAPES}/{alias}/run/analysis/water-shapes-surface.txt'] = smoke / SHAPES / alias / 'run/analysis/water-shapes-surface.txt'
    return files


# Optional per-map files: style_styles reads them when present (doodad palette, ground keys); their absence is not fatal
# for the report, so a missing one is listed but does not stop the run.
OPTIONAL = ('doodad-brushes.csv', 'ground-keys-surface.csv')


def find_vcvars():
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        return None
    found = subprocess.run([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                            '-property', 'installationPath'], capture_output=True, text=True).stdout.strip()
    vcvars = Path(found) / 'VC/Auxiliary/Build/vcvarsall.bat' if found else None
    return vcvars if vcvars and vcvars.is_file() else None


def core_sources(commit: str):
    """core/*.cpp of rme_procedural_core as listed in source/procedural/CMakeLists.txt at the commit."""
    cmake = subprocess.run(['git', '-C', str(REPO), 'show', f'{commit}:source/procedural/CMakeLists.txt'],
                           capture_output=True, text=True, check=True).stdout
    sources = []
    for line in cmake.splitlines():
        if line.startswith('add_library(rme_procedural_core') or (line.startswith('target_sources(rme_procedural_core')):
            sources += re.findall(r'core/\w+\.cpp', line)
    return sources


def build_tool(name: str, commit: str, build: Path, vcvars: Path):
    """Compile tools/<name>_main.cpp and the core of `commit` into build/<name>-<commit>/<name>.exe."""
    folder = build / f'{name}-{commit}'
    folder.mkdir(parents=True, exist_ok=True)
    main = f'source/procedural/tools/{name}_main.cpp'
    archive = subprocess.run(['git', '-C', str(REPO), 'archive', commit, 'source/procedural/core', main], capture_output=True, check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        tar.extractall(folder / 'src', filter='data')
    sources = [f'source/procedural/{s}' for s in core_sources(commit)] + [main]
    (folder / 'obj').mkdir(exist_ok=True)
    # Flags of the release build in compile_commands.json (build_ninja.bat), minus the vcpkg include folders, plus
    # /Brepro (no PE timestamp). The style_styles EXE hash still differs between build folders; the report hash is the check.
    flags = '/nologo /TP -DUNICODE -D_UNICODE /I source /W3 /wd4996 /wd4800 /wd4100 /wd4706 /EHsc /O2 /DNDEBUG /std:c++latest /MD /GL /Brepro'
    script = folder / 'build.cmd'
    script.write_text('@echo off\r\n'
                      f'call "{vcvars}" x64 >nul || exit /b 1\r\n'
                      f'cd /d "{folder / "src"}" || exit /b 1\r\n'
                      f'cl 2>"{folder / "compiler.txt"}"\r\n'
                      f'cl {flags} /Fo"{folder / "obj"}\\\\" /Fe"{folder / (name + ".exe")}" '
                      + ' '.join(s.replace('/', '\\') for s in sources) + ' /link /LTCG /Brepro\r\n', encoding='ascii')
    result = subprocess.run(['cmd', '/d', '/c', str(script)], capture_output=True, text=True)
    (folder / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    exe = folder / f'{name}.exe'
    if result.returncode != 0 or not exe.is_file():
        sys.exit(f'build of {name} at {commit} failed, see {folder / "build.log"}')
    banner = (folder / 'compiler.txt').read_text(encoding='utf-8', errors='replace').splitlines()
    return exe, {'commit': commit, 'sources': sources, 'flags': flags, 'compiler': banner[0] if banner else '', 'sha256': sha256(exe)}


def run_tool(name: str, exe: Path, smoke: Path, out: Path):
    if name == 'style_styles':
        cmd = [str(exe), '--manifest', str(smoke / MANIFEST), '--fingerprints', str(smoke / FINGERPRINTS), '--stats', str(smoke / STATS), '--out', str(out)]
    else:
        cmd = [str(exe), '--manifest', str(smoke / MANIFEST), '--shapes', str(smoke / SHAPES), '--out', str(out)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    out.with_suffix('.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    if result.returncode != 0 or not out.is_file():
        sys.exit(f'{name} failed ({result.returncode}): {result.stderr[-500:]}')
    return cmd


def percent(candidate, baseline, digits):
    return round(100 * candidate / baseline, digits)


def iteration3_checks(report):
    """(label, value from the report, value quoted in G4_GATE_STATUS.md 'Iteracja 3')."""
    styles = {s['style']: s['A'] for s in report['styles']}
    held = {b['held_out']: b for b in report['B_leave_one_map_out']}
    matrix = report['C_matrix_median_S1_plus_S2']
    rows = {r['profile']: r['distances'] for r in matrix['rows']}
    column = {t: i for i, t in enumerate(matrix['targets'])}
    ratios = [percent(a['P1_candidate_median_ppm'], a['P1_baseline_median_ppm'], 1) for a in styles.values()]
    ratios += [percent(b['P1_candidate_median_ppm'], b['P1_baseline_median_ppm'], 1) for b in held.values()]
    return [
        ('A: styles', len(styles), 8),
        ('A: styles better in S1 and S2, 32/32 seeds', sum(a[m]['verdict'] == 'better' and a[m]['candidate_better'] == 32
                                                         for a in styles.values() for m in ('S1_shares_ppm', 'S2_adjacency_ppm')), 16),
        ('A: styles worse in S1 and S2', report['gate']['A_worse_in_S1_and_S2'], 0),
        ('A: S3 better in', sorted(k for k, a in styles.items() if a['S3_vegetation_microprm']['verdict'] == 'better'), ['caves', 'm05', 'm06', 'm19']),
        ('A+B: P1 range % (min, max)', [min(ratios), max(ratios)], [95.2, 100.0]),
        ('B: M12 held out S1 / S2 median ppm', [held['M12']['S1_shares_ppm']['median'], held['M12']['S2_adjacency_ppm']['median']], [-320252, -307163]),
        ('B: M13 held out S1 / S2 median ppm', [held['M13']['S1_shares_ppm']['median'], held['M13']['S2_adjacency_ppm']['median']], [-493180, -487248]),
        ('B: seeds better (M12 S1, S2, M13 S1, S2)', [held[m][k]['candidate_better'] for m in ('M12', 'M13') for k in ('S1_shares_ppm', 'S2_adjacency_ppm')], [32] * 4),
        ('B: P1 % (M12, M13)', [percent(held[m]['P1_candidate_median_ppm'], held[m]['P1_baseline_median_ppm'], 1) for m in ('M12', 'M13')], [95.9, 95.2]),
        ('C: recognized', report['gate']['C_recognized'], 6),
        ('C: not recognized', sorted(t for t, ok in zip(matrix['targets'], matrix['recognized']) if not ok), ['m01', 'swamp']),
        ('C: m01 target, own profile vs m06 profile', [rows['m01'][column['m01']], rows['m06'][column['m01']]], [2821743, 2811131]),
        ('caves target: manual (mln ppm, 1 decimal) vs caves profile', [round(rows['manual'][column['caves']] / 1e6, 1), rows['caves'][column['caves']]], [3.0, 12712]),
        ('gate: P1 profiles failing', report['gate']['P1_profiles_failing'], 0),
        ('gate: B any worse in both', report['gate']['B_any_worse_in_both'], False),
    ]


def water_row(report):
    fold = report['folds'][0]
    t, s = fold['tortuosity_distance_change'], fold['straight_distance_change']
    return [fold['fitted_meander'], fold['candidate_median']['tortuosity'], fold['candidate_median']['straight'],
            [t['candidate_better'], t['candidate_worse'], t['verdict']], [s['candidate_better'], s['candidate_worse'], s['verdict']],
            percent(fold['P1_candidate_median_ppm'], fold['P1_baseline_median_ppm'], 3), sum(f['river_invariant_failures'] for f in report['folds'])]


def iteration4_checks(report):
    """(label, value from the report, value quoted in G4_GATE_STATUS.md 'Iteracja 4', row 4b and text)."""
    refs = {m['alias']: m for m in report['reference_maps']}
    base = report['generator_baseline']
    folds = {f"{f['train']}->{f['test']}": f for f in report['folds']}
    forward, backward = folds['M06->M19'], folds['M19->M06']
    return [
        ('references M06 / M19: rivers, T, S', [[refs[a]['rivers'], refs[a]['tortuosity_permille'], refs[a]['straight_permille']] for a in ('M06', 'M19')], [[29, 1626, 0], [8, 1669, 0]]),
        ('qualifying references', sorted(a for a, m in refs.items() if m['qualifies']), ['M06', 'M19']),
        ('baseline: T, S, seeds with river', [base['median_tortuosity_permille'], base['median_straight_permille'], base['seeds_with_river']], [1361, 181, 31]),
        ('row 4b: meander, T, S, T b/w, S b/w, P1 %, river islands', water_row(report), [8, 1514, 24, [22, 6, 'undecided'], [16, 5, 'undecided'], 99.972, 0]),
        ('both folds: same row values', water_row({'folds': [backward]})[:6], water_row({'folds': [forward]})[:6]),
        ('M06->M19: T median (p10, p90)', [forward['tortuosity_distance_change'][k] for k in ('median', 'p10', 'p90')], [-58, -219, 54]),
        ('M06->M19: S median (p10, p90)', [forward['straight_distance_change'][k] for k in ('median', 'p10', 'p90')], [-77, -281, 79]),
        ('M19->M06: T / S median', [backward['tortuosity_distance_change']['median'], backward['straight_distance_change']['median']], [-56, -77]),
        ('best seed share 22/31 = 71 %', [max(f[k]['candidate_better'] for f in report['folds'] for k in ('tortuosity_distance_change', 'straight_distance_change')),
                                          forward['tortuosity_distance_change']['seeds'], round(100 * 22 / forward['tortuosity_distance_change']['seeds'])], [22, 31, 71]),
        ('gate: folds passing', report['gate']['folds_passing'], 0),
    ]


def iteration4a_checks(report):
    # Known deviation: the table quotes P1 99,997 %, the stored report gives 999669 / 999730 ppm = 99,994 %. The table is
    # left as written; the check passes only while the report still shows exactly the documented value.
    return [('row 4a: meander, T, S, T b/w, S b/w, P1 %, river islands', water_row(report), [8, 1398, 70, [18, 9, 'undecided'], [15, 6, 'undecided'], 99.997, 0],
             [8, 1398, 70, [18, 9, 'undecided'], [15, 6, 'undecided'], 99.994, 0])]


def evaluate(checks):
    """(label, report value, table value[, documented report value of a known deviation]) -> result rows."""
    rows = []
    for label, got, want, *documented in checks:
        row = {'check': label, 'report': got, 'table': want, 'match': got == want}
        if documented:
            row['documented_deviation'] = documented[0]
            row['status'] = 'match' if got == want else 'documented deviation' if got == documented[0] else 'mismatch'
        else:
            row['status'] = 'match' if got == want else 'mismatch'
        rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('out_dir', type=Path, help='new or empty folder for builds, runs and the summary (use the scratchpad)')
    parser.add_argument('--local-data', type=Path, default=None, help='local-data folder (default: the main checkout\'s)')
    parser.add_argument('--current-bin', type=Path, default=None, help='folder with newer style_styles.exe/water_eval.exe to run for information')
    parser.add_argument('--vcvars', type=Path, default=None, help='vcvarsall.bat (default: found with vswhere)')
    args = parser.parse_args()

    local_data = (args.local_data or main_checkout_local_data(REPO)).resolve()
    smoke = local_data / 'procedural-smoke'
    out = args.out_dir.resolve()
    if out.exists() and any(out.iterdir()):
        sys.exit(f'{out} is not empty; give a new folder so old runs cannot be mistaken for new ones')

    inputs = input_files(smoke)
    historical = {k: smoke / v for k, v in HISTORICAL.items()}
    missing = [rel for rel, p in inputs.items() if not p.is_file() and not rel.endswith(OPTIONAL)]
    missing += [HISTORICAL[k] for k, p in historical.items() if not p.is_file()]
    if missing:
        sys.exit('missing local data under ' + str(smoke) + ':\n  ' + '\n  '.join(missing) + '\nD21_FAIL')
    vcvars = args.vcvars or find_vcvars()
    if not vcvars or not Path(vcvars).is_file():
        sys.exit('MSVC vcvarsall.bat not found; pass --vcvars\nD21_FAIL')

    out.mkdir(parents=True, exist_ok=True)
    params = {
        'local_data': str(local_data),
        'repo_head': subprocess.run(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip(),
        'inputs': {rel: sha256(p) if p.is_file() else None for rel, p in sorted(inputs.items())},
        'historical_reports': {HISTORICAL[k]: sha256(p) for k, p in historical.items()},
        'tools': {},
    }
    summary = {'runs': {}, 'checks': {}}
    reports = {}
    for name, spec in TOOLS.items():
        exe, provenance = build_tool(name, spec['commit'], out / 'build', Path(vcvars))
        runs = []
        for tag in ('a', 'b'):
            target = out / 'runs' / f'{name}-{tag}.json'
            target.parent.mkdir(parents=True, exist_ok=True)
            provenance['command'] = run_tool(name, exe, smoke, target)
            runs.append(target)
        params['tools'][name] = provenance
        hist = historical[spec['historical']]
        summary['runs'][name] = {'run_a_sha256': sha256(runs[0]), 'run_b_sha256': sha256(runs[1]), 'historical_sha256': sha256(hist),
                                 'runs_identical': runs[0].read_bytes() == runs[1].read_bytes(),
                                 'identical_to_historical': runs[0].read_bytes() == hist.read_bytes()}
        reports[name] = json.loads(runs[0].read_text(encoding='utf-8'))

    summary['checks']['iteration3'] = evaluate(iteration3_checks(reports['style_styles']))
    summary['checks']['iteration4b'] = evaluate(iteration4_checks(reports['water_eval']))
    summary['checks']['iteration4a_stored_only'] = evaluate(iteration4a_checks(json.loads(historical['water_4a'].read_text(encoding='utf-8'))))
    summary['not_rerunnable'] = ['iteration 4a: generator model before 99f617c was never committed; only the stored water-4a.json is checked']

    if args.current_bin:
        info = {}
        for name, spec in TOOLS.items():
            exe = args.current_bin / f'{name}.exe'
            if not exe.is_file():
                info[name] = {'missing': str(exe)}
                continue
            target = out / 'runs' / f'{name}-current.json'
            run_tool(name, exe, smoke, target)
            info[name] = {'exe': str(exe), 'exe_sha256': sha256(exe), 'exe_mtime': os.path.getmtime(exe),
                          'identical_to_historical': target.read_bytes() == historical[spec['historical']].read_bytes()}
        summary['current_binaries_information_only'] = info

    (out / 'params.json').write_text(json.dumps(params, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    (out / 'summary.json').write_text(json.dumps(summary, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')

    ok = all(r['runs_identical'] and r['identical_to_historical'] for r in summary['runs'].values())
    ok = ok and all(c['status'] != 'mismatch' for group in summary['checks'].values() for c in group)
    for name, r in summary['runs'].items():
        print(f"{name}: runs identical {r['runs_identical']}, identical to historical {r['identical_to_historical']}")
    for group, checks in summary['checks'].items():
        print(f"{group}: {sum(c['match'] for c in checks)}/{len(checks)} table values match")
        for c in checks:
            if not c['match']:
                print(f"  {c['status'].upper()} {c['check']}: report {c['report']} table {c['table']}")
    for name, r in summary.get('current_binaries_information_only', {}).items():
        print(f"current {name} (information only): {r}")
    print(f'summary: {out / "summary.json"}')
    print('D21_PASS' if ok else 'D21_FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
