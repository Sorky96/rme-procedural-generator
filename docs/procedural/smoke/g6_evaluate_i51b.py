"""Preregistered I51b A/B evaluation (9ab1ca0); frozen I51 policy 396e057.

py -I g6_evaluate_i51b.py PRIVATE_ROOT PROFILE_DIR REPORT_DIR OUTPUT.json
Exit 0: I51B_PASS, 1: valid I51B_FAIL, 2: incomplete/invalid measurement.
Holdout is evaluated only after both winners have been independently reconstructed.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

spec = importlib.util.spec_from_file_location('i51', Path(__file__).with_name('g6_evaluate_holdout.py'))
i51 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(i51)


def ranking_key(row, variant):
    if variant == 'A':
        return row['hard'], -row['inBand'], row['distance'], row['index']
    if variant == 'B':
        return row['hard'], row['distance'], -row['inBand'], row['index']
    raise ValueError('only preregistered A and B are permitted')


def read_candidates(path, bands):
    # Includes independent score arithmetic, exact settings/bands, all A winners,
    # candidate zero and every index of every new seed, before reading any holdout.
    groups, winners_a = i51.read_candidates(path, bands, i51b=True)
    winners_b = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if not line.startswith('winnerB\t'):
            continue
        _, seed, index, hard, in_band, distance = line.split('\t')
        seed = i51.integer(seed)
        if seed in winners_b:
            raise ValueError('duplicate B winner')
        winners_b[seed] = (i51.integer(index), hard, i51.integer(in_band), i51.integer(distance))
    if set(winners_b) != set(groups):
        raise ValueError('B winners require the complete 2001..2032 matrix')
    for seed, rows in groups.items():
        best = min(rows, key=lambda row: ranking_key(row, 'B'))
        if winners_b[seed] != (best['index'], str(best['hard']).lower(), best['inBand'], best['distance']):
            raise ValueError('B winner disagrees with independent reconstruction')
    return groups, {'A': winners_a, 'B': {seed: w[0] for seed, w in winners_b.items()}}


def compare_pairs(a, b):
    """Paired distances and success transitions, separately for every map/family."""
    if [p['seed'] for p in a['pairs']] != [p['seed'] for p in b['pairs']]:
        raise ValueError('paired seed mismatch')
    summary, pairs = {}, []
    for pa, pb in zip(a['pairs'], b['pairs'], strict=True):
        results = {}
        for name, ra in pa['results'].items():
            rb = pb['results'][name]
            if (ra['median'], ra['baseline']) != (rb['median'], rb['baseline']):
                raise ValueError('A/B candidates or baseline differ')
            delta = rb['winnerDistance'] - ra['winnerDistance']
            change = 'improved' if delta < 0 else 'worsened' if delta > 0 else 'tied'
            transition = ('success' if ra['success'] else 'failure') + 'To' + ('Success' if rb['success'] else 'Failure')
            results[name] = {'distanceChange': change, 'distanceDeltaBMinusA': delta,
                             'successTransition': transition}
            counts = summary.setdefault(name, dict(improved=0, worsened=0, tied=0,
                failureToSuccess=0, successToFailure=0, successToSuccess=0, failureToFailure=0))
            counts[change] += 1
            counts[transition] += 1
        pairs.append({'seed': pa['seed'], 'winnerChanged': pa['winner'] != pb['winner'], 'results': results})
    return {'winnerChanges': sum(p['winnerChanged'] for p in pairs), 'summary': summary, 'pairs': pairs}


def evaluate_ab(groups, winners, references):
    if set(groups) != set(range(2001, 2033)) or set(winners) != {'A', 'B'}:
        raise ValueError('I51b requires both orders and exactly seeds 2001..2032')
    for variant in ('A', 'B'):
        if set(winners[variant]) != set(groups):
            raise ValueError('incomplete winner matrix')
        for seed, rows in groups.items():
            if [r['index'] for r in rows] != list(range(16)):
                raise ValueError('I51b requires exactly 16 ordered candidates per seed')
            if winners[variant][seed] != min(rows, key=lambda row: ranking_key(row, variant))['index']:
                raise ValueError('incorrect winner matrix')
    variants = {v: i51.evaluate(groups, winners[v], references) for v in ('A', 'B')}
    for v, result in variants.items():
        for pair in result['pairs']:
            row = groups[pair['seed']][pair['winner']]
            pair['score'] = {key: row[key] for key in ('index', 'inBand', 'distance')}
    a, b = (variants[v]['summary']['family.main']['successes'] for v in ('A', 'B'))
    return {'variants': variants, 'paired': compare_pairs(variants['A'], variants['B']),
            'candidateCount': sum(map(len, groups.values())),
            'candidateHardFailCount': sum(r['hard'] for rows in groups.values() for r in rows),
            'thresholdPassed': b >= 23 and b > a}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('private_root', 'profile_dir', 'report_dir', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        matrices, hashes = {}, {}
        for style in ('m06', 'm19'):
            first, second = (args.report_dir / f'{style}-run{i}.tsv' for i in (1, 2))
            if first.read_bytes() != second.read_bytes():
                raise ValueError('full candidate repeats differ')
            matrices[style] = read_candidates(first, i51.load_bands(i51.BANDS_DIR / f'{style}.bands'))
            hashes[style] = hashlib.sha256(first.read_bytes()).hexdigest()
        references, audit = i51.load_references(args.private_root, args.profile_dir)
        report = {'schema': 'g6-i51b/1', 'preregistrationCommit': '9ab1ca0', 'policyCommit': '396e057',
                  'referenceAudit': audit, 'profiles': {}}
        for style, (groups, winners) in matrices.items():
            result = evaluate_ab(groups, winners, references)
            result['candidateReportSha256'] = hashes[style]
            report['profiles'][style] = result
        report['thresholdPassed'] = all(r['thresholdPassed'] for r in report['profiles'].values())
        report['result'] = 'I51B_PASS' if report['thresholdPassed'] else 'I51B_FAIL'
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True, default=str) + '\n', encoding='utf-8')
        for style, result in report['profiles'].items():
            print(style, {v: r['summary']['family.main'] for v, r in result['variants'].items()}, flush=True)
        print(report['result'])
        return 0 if report['thresholdPassed'] else 1
    except (OSError, ValueError, KeyError, TypeError, OverflowError, StopIteration, IndexError) as error:
        print(f'I51B_MEASUREMENT_BLOCKED: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
