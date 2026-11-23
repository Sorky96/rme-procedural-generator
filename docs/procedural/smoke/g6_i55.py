"""Preregistered I55 offline workflow (6251dbb). Private outputs must be in scratchpad.

measure EXE PROFILE_DIR COPY_INDEX OUTPUT_DIR: freeze inputs, generate N4 twice.
diagnose PRIVATE_ROOT PROFILE_DIR MATRIX OUTPUT.json: already exposed G4 data only.
package MATRIX PACKAGE_DIR KEY.json: anonymous 20 pairs; never prints selections.
score PACKAGE_DIR RATINGS.json KEY.json OUTPUT.json: refuses incomplete ratings first.

No dependencies, network, retries, fitting, or production ranking changes.
"""
import argparse
import csv
import hashlib
import importlib.util
import io
import json
import secrets
import shutil
import statistics
import subprocess
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def sibling(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


g6 = sibling('g6_evaluate_holdout')
matrix = sibling('g6_measure_candidates')
STYLES = ('m06', 'm19')
SEEDS = tuple(range(3001, 3033))
BLIND_SEEDS = SEEDS[:10]
POLICY = '6251dbb'


def candidate_seed(seed, index):
    if index == 0:
        return seed
    mask = (1 << 64) - 1
    x = (seed + index * 0x9e3779b97f4a7c15) & mask
    x = ((x ^ (x >> 30)) * 0xbf58476d1ce4e5b9) & mask
    x = ((x ^ (x >> 27)) * 0x94d049bb133111eb) & mask
    return x ^ (x >> 31)


def private(path):
    path = Path(path).resolve()
    if 'scratchpad' not in [p.lower() for p in path.parts]:
        raise ValueError('private output must be inside scratchpad')
    return path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, data):
    path = private(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    # Never silently overwrite a frozen manifest, key, result, or ratings.
    with path.open('x', encoding='utf-8', newline='\n') as out:
        out.write(json.dumps(data, indent=2, sort_keys=True, allow_nan=False) + '\n')


def selections(rows):
    eligible = [r for r in rows if not r['hard']]
    if not eligible:
        raise ValueError('no admissible candidate: series incomplete; seeds cannot be replaced')
    return (min(eligible, key=lambda r: r['index']),
            min(eligible, key=lambda r: (-r['inBand'], r['distance'], r['index'])))


def read_matrix(directory):
    """Independently verify scoring, settings, winners, complete seeds and I53 evidence."""
    directory = Path(directory)
    frozen = json.loads((directory / 'manifest.json').read_text())
    if frozen['schema'] != 'g6-i55-inputs/1' or frozen['policyCommit'] != POLICY:
        raise ValueError('missing frozen I55 input manifest')
    for entry in frozen['inputs'].values():
        if sha(entry['path']) != entry['sha256']:
            raise ValueError('frozen input changed')
    all_groups, audit = {}, {}
    for style in STYLES:
        path = directory / f'{style}-run1.tsv'
        if path.read_bytes() != (directory / f'{style}-run2.tsv').read_bytes():
            raise ValueError('candidate repeats differ')
        groups, _ = g6.read_candidates(path, g6.load_bands(g6.BANDS_DIR / f'{style}.bands'),
                                       first_seed=3001, count=4)
        lines = path.read_text().splitlines()
        similarity = [s for s in lines if s.startswith('similarity\t')]
        if similarity != ['similarity\ti53-index/2\tg4-natural-v1\td4-canonical-v1\twindow=16\tmodulo=4\tminpresent=50\tshared>=16\tcontainment>=100']:
            raise ValueError('I55 requires the frozen I53 index policy')
        refs = [s.split('\t') for s in lines if s.startswith('reference\t')]
        if len(refs) != 2 or {r[1] for r in refs} != {'M06', 'M19'}:
            raise ValueError('I53 must use only M06/M19')
        # Index serialization is already validated by Candidate Rank; bind report identities too.
        index_lines = Path(frozen['inputs']['index']['path']).read_text().splitlines()
        for ref in refs:
            if not any(s.split()[:4] == ref[1:5] for s in index_lines):
                raise ValueError('I53 report source differs from frozen index')
        header = next(i for i, s in enumerate(lines) if s.startswith('userSeed\t'))
        raw = csv.DictReader(io.StringIO('\n'.join(s for s in lines[header:] if not s.startswith('winner\t'))), delimiter='\t')
        counts = Counter()
        for r in raw:
            row = groups[int(r['userSeed'])][int(r['index'])]
            if int(r['seed']) != candidate_seed(int(r['userSeed']), row['index']):
                raise ValueError('candidate seed differs from frozen SplitMix64 policy')
            before, after = (g6.integer(r[k]) for k in ('copyBefore', 'copyAfter'))
            shared = [g6.integer(r[k]) for k in ('M06shared', 'M19shared')]
            permille = [g6.integer(r[k]) for k in ('M06permille', 'M19permille')]
            if not 0 <= after <= before or any(not 0 <= n <= after for n in shared):
                raise ValueError('invalid I53 window counts')
            if permille != [n * 1000 // after if after else 0 for n in shared]:
                raise ValueError('invalid I53 containment')
            hit = any(n >= 16 and p >= 100 for n, p in zip(shared, permille))
            expected = ('not-evaluated (too few windows)' if after < 16 else
                        'copy thresholds met; Place blocked' if hit else 'evaluated (below copy thresholds)')
            if r['copyState'] != expected or hit and not row['hard']:
                raise ValueError('invalid I53 state/hard failure')
            row.update(actualSeed=int(r['seed']), fingerprint=r['surfaceFingerprint'])
            counts[r['copyState']] += 1
        all_groups[style] = groups
        audit[style] = {'candidates': 128, 'seeds': 32, 'copyStates': dict(counts),
                        'hardFailures': sum(r['hard'] for rows in groups.values() for r in rows),
                        'seedsWithoutAdmissible': sum(all(r['hard'] for r in rows) for rows in groups.values()),
                        'reportSha256': sha(path)}
    return all_groups, audit


def measure(exe, profiles, index, output, family_manifest=None):
    output = private(output)
    if output.exists():
        raise ValueError('measurement directory must be new; no overwriting a frozen run')
    inputs = {'exe': Path(exe).resolve(), 'index': Path(index).resolve()}
    family_data = None
    if family_manifest is not None:
        family_data = sibling('g6_i55_intake').verify(family_manifest)
        inputs['familyManifest'] = Path(family_manifest).resolve()
    for style in STYLES:
        inputs[style] = Path(profiles).resolve() / f'{style}.profile'
        inputs[style + '-bands'] = g6.BANDS_DIR / f'{style}.bands'
    if family_data is not None:
        sibling('g6_i55_intake').check_generator_inputs(family_data, profiles, index)
    for name in ('g6_i55.py', 'g6_i55_form.html', 'g6_evaluate_holdout.py', 'g6_measure_candidates.py'):
        inputs[name] = Path(__file__).with_name(name)
    frozen = {'schema': 'g6-i55-inputs/1', 'policyCommit': POLICY,
              'seeds': list(SEEDS), 'blindSeeds': list(BLIND_SEEDS), 'count': 4, 'side': 512,
              'newFamilies': [], 'newFamilyStatus': 'BLOCKED: no frozen eligible new-family sources supplied',
              'inputs': {k: {'path': str(p), 'sha256': sha(p)} for k, p in inputs.items()}}
    if family_data is not None:
        frozen.update(newFamilies=family_data['families'], newFamilyStatus='FROZEN: pending quantitative evaluation; I55 OPEN')
    write_json(output / 'manifest.json', frozen)  # Before any generation or comparison.

    def run_block(spec):
        style, run, first = spec
        renders = output / f'{style}-images{run}'
        command = [str(inputs['exe']), str(inputs[style]), '512', str(first), str(first + 7),
                   '--count', '4', '--bands', str(inputs[style + '-bands']), '--path-length',
                   '40' if style == 'm06' else '80', '--building-density', '30' if style == 'm06' else '40',
                   '--rock-blocks', '0' if style == 'm06' else '1', '--g5-levels',
                   '--copy-index', str(inputs['index']), '--render-dir', str(renders)]
        result = subprocess.run(command, capture_output=True)
        prefix = output / f'{style}-run{run}-{first}'
        prefix.with_suffix('.tsv').write_bytes(result.stdout)
        prefix.with_suffix('.stderr').write_bytes(result.stderr)
        if result.returncode or result.stderr:
            raise ValueError('Candidate Rank failed; partial run retained, no retry')
        print(f'{style} run {run}: block completed', flush=True)
        return style, run, first, result.stdout

    jobs = [(style, run, first) for run in (1, 2) for style in STYLES for first in range(3001, 3033, 8)]
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(run_block, jobs))
    for style in STYLES:
        for run in (1, 2):
            reports = [r[3] for r in results if r[:2] == (style, run)]
            (output / f'{style}-run{run}.tsv').write_bytes(matrix.merge_reports(reports))
        for seed in SEEDS:
            for i in range(4):
                if sha(output / f'{style}-images1/{seed}-{i}.png') != sha(output / f'{style}-images2/{seed}-{i}.png'):
                    raise ValueError('semantic image repeatability failed')
    _, audit = read_matrix(output)
    write_json(output / 'matrix-audit.json', audit)
    print('I55_MATRIX_REPEATABILITY_PASS (not a quality gate)')


def summarize(pairs):
    if len(pairs) != 32:
        raise ValueError('comparison requires all 32 seeds')
    l2, l3 = ([p[k] for p in pairs] for k in ('L2', 'L3'))
    wins = sum(b < a for a, b in zip(l2, l3))
    return {'seeds': 32, 'strictL3Wins': wins, 'ties': sum(a == b for a, b in zip(l2, l3)),
            'L2median': statistics.median(l2), 'L3median': statistics.median(l3),
            'medianDifferenceL3MinusL2': statistics.median(l3) - statistics.median(l2),
            'medianPairedDifferenceL3MinusL2': statistics.median(b - a for a, b in zip(l2, l3)),
            'atLeast23': wins >= 23}


def compare(groups, references, families):
    if not references or not families or set().union(*(set(v) for v in families.values())) != set(references):
        raise ValueError('missing sources or incomplete family allocation')
    flat = [m for maps in families.values() for m in maps]
    if len(flat) != len(set(flat)) or any(not v for v in families.values()):
        raise ValueError('duplicate source or empty family')
    coverage = {}
    for alias, ref in references.items():
        if set(ref) != set(g6.METRICS):
            raise ValueError('reference must have exactly the frozen 26 metrics')
        known = [m for m in g6.METRICS if ref[m] is not None]
        if not known:
            raise ValueError('insufficient reference coverage')
        for m in g6.METRICS:
            g6.contribution(ref[m], ref[m], m.startswith('scale.'))
        varying = [m for m in known if len({g6.contribution(r['metrics'][m], ref[m], m.startswith('scale.'))
                    for rows in groups.values() for r in rows}) > 1]
        coverage[alias] = {'known': len(known), 'varying': len(varying),
                           'naMetrics': [m for m in g6.METRICS if m not in known]}
    records = []
    for seed in SEEDS:
        l2, l3 = selections(groups[seed])
        maps = {m: {'L2': g6.map_distance(l2['metrics'], ref), 'L3': g6.map_distance(l3['metrics'], ref)}
                for m, ref in references.items()}
        fs = {name: {k: statistics.mean(maps[m][k] for m in aliases) for k in ('L2', 'L3')}
              for name, aliases in families.items()}
        main = {k: statistics.mean(f[k] for f in fs.values()) for k in ('L2', 'L3')}
        # Selection identities intentionally omitted: this diagnostic must not unblind the package.
        records.append({'seed': seed, 'maps': maps, 'families': fs, **main})
    return {'summary': summarize(records), 'coverage': coverage, 'perSeed': records,
            'maps': {m: summarize([r['maps'][m] for r in records]) for m in references},
            'families': {f: summarize([r['families'][f] for r in records]) for f in families}}


def diagnose(root, profiles, directory, output):
    references, ref_audit = g6.load_references(Path(root), Path(profiles))
    groups, audit = read_matrix(directory)
    report = {'schema': 'g6-i55-diagnostics/1', 'policyCommit': POLICY, 'I55': 'OPEN',
              'blind': 'PENDING: no ratings read', 'newFamilies': 'BLOCKED: missing provenance, consent and G4 relatedness evidence',
              'referenceAudit': ref_audit, 'I53': audit, 'profiles': {}}
    for style in STYLES:
        report['profiles'][style] = {
            'diagnostic': compare(groups[style], {m: references[m] for m in g6.MAIN_MAPS},
                                  {'M01': ['M01'], 'M05': ['M05'], 'winter': ['M11', 'M18']}),
            'inSample': compare(groups[style], {m: references[m] for m in ('M06', 'M19')},
                                {'M06': ['M06'], 'M19': ['M19']}),
            'relatedAppendix': compare(groups[style], {m: references[m] for m in g6.RELATED},
                                      {'M19-related': list(g6.RELATED)})}
    write_json(output, report)
    print('I55_DIAGNOSTICS_WRITTEN; I55 OPEN, unseen-family evidence BLOCKED')


def assignments():
    rng = secrets.SystemRandom()
    pairs = []
    for style in STYLES:
        sides = ['A'] * 5 + ['B'] * 5
        rng.shuffle(sides)
        for seed, side in zip(BLIND_SEEDS, sides):
            pairs.append({'id': secrets.token_hex(8), 'style': style, 'seed': seed, 'L3side': side})
    rng.shuffle(pairs)
    return pairs


def validate_key(pairs):
    if len(pairs) != 20 or len({p['id'] for p in pairs}) != 20:
        raise ValueError('key must contain exactly 20 unique pairs')
    for style in STYLES:
        rows = [p for p in pairs if p['style'] == style]
        if sorted(p['seed'] for p in rows) != list(BLIND_SEEDS) or Counter(p['L3side'] for p in rows) != {'A': 5, 'B': 5}:
            raise ValueError('key seeds or side balance mismatch')


def package(directory, destination, key_path):
    directory, destination, key_path = Path(directory), private(destination), private(key_path)
    if key_path.is_relative_to(destination) or destination.exists() or key_path.exists():
        raise ValueError('new package and separate new key path required')
    groups, _ = read_matrix(directory)
    pairs = assignments()
    validate_key(pairs)
    # Resolve every pair before writing any evaluator material. No replacement seeds.
    for p in pairs:
        l2, l3 = selections(groups[p['style']][p['seed']])
        p.update(L2index=l2['index'], L3index=l3['index'], identical=l2['index'] == l3['index'])
        p['views'] = {}
        for side in ('A', 'B'):
            index = p['L3index'] if side == p['L3side'] else p['L2index']
            src = directory / f'{p["style"]}-images1/{p["seed"]}-{index}.png'
            p['views'][side] = {'file': f'{p["id"]}-{side}.png', 'sha256': sha(src)}
    key = {'schema': 'g6-i55-key/1', 'policyCommit': POLICY, 'matrixManifestSha256': sha(directory / 'manifest.json'), 'pairs': pairs}
    write_json(key_path, key)  # Freeze assignment before publishing any view or collecting ratings.
    destination.mkdir(parents=True)
    public = []
    for p in pairs:
        entry = {'id': p['id'], 'views': {}}
        for side in ('A', 'B'):
            index = p['L3index'] if side == p['L3side'] else p['L2index']
            src = directory / f'{p["style"]}-images1/{p["seed"]}-{index}.png'
            name = f'{p["id"]}-{side}.png'
            shutil.copyfile(src, destination / name)
            entry['views'][side] = {'file': name, 'sha256': sha(src)}
        public.append(entry)
    manifest = {'schema': 'g6-i55-package/1', 'pairs': public}
    write_json(destination / 'manifest.json', manifest)
    template = Path(__file__).with_name('g6_i55_form.html').read_text(encoding='utf-8')
    (destination / 'index.html').write_text(template.replace('__PAIRS__', json.dumps(public)), encoding='utf-8')
    (destination / 'README.txt').write_text('Open index.html locally. Evaluate all 20 pairs, then export ratings.\n'
        'Share only this directory. Keep answers in scratchpad. Do not request the key before submitting all ratings.\n'
        'Semantic views evaluate terrain shape and distribution, not sprite aesthetics or tested gameplay.\n', encoding='utf-8')
    print('I55_ANONYMOUS_PACKAGE_READY (20 pairs; assignments and pair summary withheld)')


def validate_ratings(ratings, ids):
    if ratings.get('schema') != 'g6-i55-ratings/1' or not isinstance(ratings.get('ratings'), list):
        raise ValueError('ratings schema mismatch')
    rows = ratings['ratings']
    if len(ids) != 20 or len(set(ids)) != 20 or len(rows) != 20 or {r['id'] for r in rows} != set(ids):
        raise ValueError('all 20 unique ratings required before opening key')
    if any(r['choice'] not in ('A', 'B', 'tie') or not isinstance(r.get('note', ''), str)
           or len(r.get('note', '').encode('utf-8')) > 1024 for r in rows):
        raise ValueError('invalid choice/note')
    return {r['id']: r for r in rows}


def blind_result(pairs, answers):
    validate_key(pairs)
    result = {}
    for style in (*STYLES, 'all'):
        rows = [p for p in pairs if style == 'all' or p['style'] == style]
        wins = sum(answers[p['id']]['choice'] == p['L3side'] for p in rows)
        result[style] = {'pairs': len(rows), 'L3wins': wins,
                         'L3proportion': wins / len(rows),
                         'ties': sum(answers[p['id']]['choice'] == 'tie' for p in rows),
                         'identicalPairs': sum(p['identical'] for p in rows)}
    return {'schema': 'g6-i55-blind-result/1', 'profiles': result, 'blindThresholdPassed': result['all']['L3wins'] >= 13,
            'I55': 'OPEN: quantitative unseen-family evidence and regression also required'}


def score(destination, ratings_path, key_path, output):
    destination, ratings_path, key_path = private(destination), private(ratings_path), private(key_path)
    manifest = json.loads((destination / 'manifest.json').read_text())
    if manifest['schema'] != 'g6-i55-package/1':
        raise ValueError('package schema mismatch')
    ids = [p['id'] for p in manifest['pairs']]
    answers = validate_ratings(json.loads(Path(ratings_path).read_text(encoding='utf-8')), ids)
    # Deliberately no key read above this point.
    key = json.loads(Path(key_path).read_text(encoding='utf-8'))
    if key['schema'] != 'g6-i55-key/1' or key['policyCommit'] != POLICY or {p['id'] for p in key['pairs']} != set(ids):
        raise ValueError('key/package mismatch')
    if manifest['pairs'] != [{'id': p['id'], 'views': p['views']} for p in key['pairs']]:
        raise ValueError('package differs from frozen key')
    for p in manifest['pairs']:
        for side in ('A', 'B'):
            view = p['views'][side]
            if view['file'] != f'{p["id"]}-{side}.png' or sha(destination / view['file']) != view['sha256']:
                raise ValueError('package image changed')
    result = blind_result(key['pairs'], answers)
    result.update(ratingsSha256=sha(ratings_path), keySha256=sha(key_path), packageSha256=sha(destination / 'manifest.json'))
    write_json(output, result)
    print('I55_BLIND_RESULT_WRITTEN (G6 remains open)')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    for name, params in {'measure': ('exe', 'profiles', 'index', 'output'),
                         'diagnose': ('root', 'profiles', 'directory', 'output'),
                         'package': ('directory', 'destination', 'key_path'),
                         'score': ('destination', 'ratings_path', 'key_path', 'output')}.items():
        p = sub.add_parser(name)
        for param in params:
            p.add_argument(param, type=Path)
        if name == 'measure':
            p.add_argument('--family-manifest', type=Path)
    args = vars(parser.parse_args())
    command = args.pop('command')
    try:
        globals()[command](**args)
        return 0
    except (OSError, ValueError, KeyError, TypeError, IndexError, StopIteration, OverflowError) as error:
        print(f'I55_BLOCKED: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
