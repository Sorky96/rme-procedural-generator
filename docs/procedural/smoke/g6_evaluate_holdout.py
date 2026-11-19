"""Frozen I51 holdout evaluation. Reads explicit private G4 bundles, never maps/images.

py -I g6_evaluate_holdout.py PRIVATE_ROOT PROFILE_DIR REPORT_DIR OUTPUT.json
REPORT_DIR contains m06-run{1,2}.tsv and m19-run{1,2}.tsv from g6_measure_candidates.
Any missing/malformed input blocks measurement; a failed threshold is a valid result (exit 1).
"""
import argparse
import csv
import hashlib
import io
import json
import math
import re
import statistics
from fractions import Fraction
from pathlib import Path

METRICS = ('channels.L channels.W channels.B channels.C scale.E16 scale.E64 '
           'patch.D patch.F patch.P patch.RB patch.RF patch.RS '
           'trans.R0 trans.R1 trans.R2 trans.R3 trans.D0 trans.D1 trans.D2 trans.D3 '
           'paths.L paths.W build.per64k build.long build.short outline.median').split()
CAP = math.log(64)
BUNDLES = ('g4-channels-20261108a', 'g4-scale-20261108a',
           'g4-trans-20261108a', 'g4-build-20261108a')
MAIN_MAPS = ('M01', 'M05', 'M11', 'M18')
RELATED = tuple(f'M{i:02}' for i in range(12, 18))
BANDS_DIR = Path(__file__).resolve().parents[1] / 'g6-bands-v1'


def integer(text):
    if not re.fullmatch(r'-?\d+', text):
        raise ValueError(f'invalid integer: {text!r}')
    return int(text)


def value(text):
    return None if text == 'NA' else integer(text)


def contribution(measured, reference, signed=False):
    if (measured is not None and measured < 0 or reference is not None and reference < 0) and not signed:
        raise ValueError('negative nonnegative metric')
    if measured is None or reference is None:
        return CAP
    if signed:
        return min(CAP, abs(math.asinh(measured) - math.asinh(reference)))
    if reference == 0:
        return min(CAP, math.log1p(measured))
    return CAP if measured == 0 else min(CAP, abs(math.log(measured / reference)))


def map_distance(metrics, reference):
    # No intermediate fixed-point rounding in evaluation; the policy rounds only ranking.
    return sum(contribution(metrics[m], reference[m], m.startswith('scale.')) for m in METRICS) / 26


def family_distances(per_map):
    winter = (per_map['M11'] + per_map['M18']) / 2
    return {'M01': per_map['M01'], 'M05': per_map['M05'], 'winter': winter,
            'main': (per_map['M01'] + per_map['M05'] + winter) / 3,
            'M19-related': sum(per_map[m] for m in RELATED) / 6}


def pair_result(distances, hard, winner):
    median = statistics.median(distances)
    eligible = [not h and d < median for d, h in zip(distances, hard, strict=True)]
    return {'winnerDistance': distances[winner], 'median': median, 'success': eligible[winner],
            'baseline': Fraction(sum(eligible), len(eligible))}


def fields(path):
    rows = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        key, sep, body = line.partition('\t')
        if not sep or key in rows:
            raise ValueError(f'{path}: malformed/duplicate row {key}')
        rows[key] = body
    return rows


def load_bands(path):
    lines = path.read_text(encoding='utf-8').splitlines()
    if lines[0] != 'g6-ranking-bands/1':
        raise ValueError('bands schema')
    result = {}
    for line in lines[1:]:
        if not line or line.startswith('#'):
            continue
        name, ref, lo, hi = line.split()
        if name in result:
            raise ValueError('duplicate band')
        result[name] = (integer(ref), integer(lo), integer(hi))
    if list(result) != METRICS:
        raise ValueError('bands must contain the frozen 26 metrics in table order')
    return result


def load_references(root, profile_dir, *, aliases=None, check_bands=True):
    """Audit all 19 maps, source identity, DAT and keymap, then read G4 summaries.

    Old bundles predate subsequent measurements, so each metric is taken from its explicit
    authoritative bundle. Repeated channels/scale/paths are also checked for agreement.
    No OTBM or sprite file is copied/read. Existing import and verification receipts are evidence.
    Explicit aliases reuse this audit for intake; check_bands=False skips only the
    historical M06/M19 reference-value check (the intake caller checks those separately).
    """
    references, audit, profile_audit = {}, {}, {}
    global_keymap = global_dat = global_spr = None
    for alias in (aliases if aliases is not None else (f'M{i:02}' for i in range(1, 20))):
        receipts, tables, summary_hashes = [], {}, {}
        for bundle in BUNDLES:
            directory = root / bundle / alias
            run = directory / 'run'
            receipt = json.loads((directory / 'verification.json').read_text())
            imported = json.loads((run / 'reference-report.json').read_text())
            semantic = json.loads((run / 'semantic-report.json').read_text())
            hashes = receipt['source_hashes_unchanged']
            dat, spr = hashes['Tibia.dat'], hashes['Tibia.spr']
            keymap = (run / 'keymap.tsv').read_text(encoding='utf-8')
            if not keymap.startswith('g4-keymap/1\n'):
                raise ValueError('keymap schema')
            if global_keymap is None:
                global_keymap, global_dat, global_spr = keymap, dat, spr
            if (keymap, dat, spr) != (global_keymap, global_dat, global_spr):
                raise ValueError(f'{directory}: inconsistent mapping/client assets')
            if (receipt['result'], receipt['exit_code'], receipt['client_id'], imported['client'],
                    (run / 'client-id.txt').read_text().strip(), imported['compatibility']) != ('PASS', 0, '10.98', '10.98', '10.98', 'compatible'):
                raise ValueError(f'{directory}: unverified or incompatible input')
            if imported['schema'] != 'g3-import-report/1' or semantic['schema'] != 'g3-semantic-report/1':
                raise ValueError('reference report schema mismatch')
            if imported['sha256'] != semantic['sha256'] or imported['sha256'] not in hashes.values():
                raise ValueError(f'{directory}: source identity not verified')
            identity = (imported['sha256'], imported['header'], imported['tiles'])
            if receipts and identity != receipts[0]:
                raise ValueError(f'{alias}: reference versions disagree across bundles')
            receipts.append(identity)
            analysis = run / 'analysis'
            required = {BUNDLES[0]: ['channels'], BUNDLES[1]: ['scale'],
                        BUNDLES[2]: ['patches', 'transitions'], BUNDLES[3]: ['paths', 'buildings']}[bundle]
            for name in required:
                source = analysis / f'{name}-surface.txt'
                tables[name] = fields(source)
                summary_hashes[f'{bundle}/{name}-surface.txt'] = hashlib.sha256(source.read_bytes()).hexdigest()
            # These metrics did not change definitions between the supplied series.
            for name in ('channels', 'scale', 'paths'):
                path = analysis / f'{name}-surface.txt'
                if path.exists():
                    text = path.read_text()
                    tag = f'check-{name}'
                    if tag in tables and tables[tag] != text:
                        raise ValueError(f'{alias}: inconsistent {name} summaries')
                    tables[tag] = text
        r, reasons = {}, {}
        def put(metric, raw, reason='G4 definitional N/A'):
            r[metric] = value(raw)
            if r[metric] is None:
                reasons[metric] = reason
        for name, table, keys in (
            ('channels', 'channels', ('length_per_64k', 'width_milli', 'branches_per_100_milli', 'connected_permille')),
            ('paths', 'paths', ('length_per_64k', 'width_milli'))):
            t = tables[table]
            count = integer(t['count'])
            if count < 0:
                raise ValueError('negative object count')
            for suffix, key in zip(('L', 'W', 'B', 'C'), keys, strict=False):
                raw = t[key]  # Required even when the count makes the value N/A.
                if integer(raw) < 0:
                    raise ValueError('negative channel/path measurement')
                put(f'{name}.{suffix}', '0' if count == 0 and suffix == 'L' else 'NA' if count < 5 else raw,
                    f'{count} components < G4 reference minimum 5')
        t = tables['scale']
        pairs = integer(t['pairs_64'])
        if pairs < 0:
            raise ValueError('negative lag pair count')
        for lag in (16, 64):
            integer(t[f'excess_{lag}'])
            put(f'scale.E{lag}', t[f'excess_{lag}'])  # G4 lagExcessPermille defines 0 with no pairs.
        t = tables['patches']
        for metric, key in zip(METRICS[6:12], ('boundary', 'compact', 'beach', 'rock_boundary', 'rock_compact', 'rock_dust'), strict=True):
            put(metric, t[key], 'G4 patch denominator < 1000')
        t = tables['transitions']
        for prefix, key in (('R', 'rock_comp'), ('D', 'dirt_comp')):
            pairs = integer(t['rock_pairs' if prefix == 'R' else 'dirt_pairs'])
            raw = t[key].split()
            if pairs < 0 or (pairs < 1000) != (raw == ['NA']):
                raise ValueError('transition N/A disagrees with G4 denominator')
            if raw == ['NA']:
                raw *= 4
            if len(raw) != 4:
                raise ValueError('transition requires 4 values or NA')
            for i, v in enumerate(raw):
                put(f'trans.{prefix}{i}', v, 'G4 boundary pairs < 1000')
        t = tables['buildings']
        count = integer(t['count'])
        if count < 0:
            raise ValueError('negative building count')
        for metric, key in zip(METRICS[22:], ('per_64k', 'long_median', 'short_median', 'wall_median'), strict=True):
            raw = t[key]
            if integer(raw) < 0:
                raise ValueError('negative building measurement')
            put(metric, '0' if count == 0 and metric == 'build.per64k' else 'NA' if count < 5 else raw,
                f'{count} buildings < G4 reference minimum 5')
        for metric in METRICS:
            contribution(r[metric], r[metric], metric.startswith('scale.'))  # domain validation
        if all(r[m] is None for m in METRICS):
            raise ValueError(f'{alias}: no differentiating positions')
        references[alias] = r
        audit[alias] = {'mapSha256': receipts[0][0], 'metrics': r, 'naReasons': reasons,
                        'differentiatingPositions': 26 - len(reasons), 'summarySha256': summary_hashes}
    for style, alias in (('m06', 'M06'), ('m19', 'M19')):
        profile = (profile_dir / f'{style}.profile').read_text(encoding='utf-8').splitlines()
        if profile[0] != 'g4-profile/6' or 'client\t10.98' not in profile or 'level\tsurface' not in profile or 'floors\t0\t7' not in profile:
            raise ValueError('profile version/client/floors mismatch')
        if f'assets\tdat_sha256\t{global_dat}' not in profile:
            raise ValueError('profile DAT differs from references')
        if [s for s in profile if s.startswith('key\t')] != global_keymap.splitlines()[1:]:
            raise ValueError('profile mapping differs from references')
        bands = load_bands(BANDS_DIR / f'{style}.bands')
        if check_bands and any(references[alias][m] != bands[m][0] for m in METRICS):
            raise ValueError('frozen profile references disagree with G4 data')
        profile_audit[style] = {'profileSha256': hashlib.sha256((profile_dir / f'{style}.profile').read_bytes()).hexdigest(),
                                'bandsSha256': hashlib.sha256((BANDS_DIR / f'{style}.bands').read_bytes()).hexdigest()}
    return references, {'bundles': list(BUNDLES), 'client': '10.98', 'datSha256': global_dat,
                        'sprSha256': global_spr, 'keymapSha256': hashlib.sha256(global_keymap.encode()).hexdigest(),
                        'profiles': profile_audit, 'maps': audit}


def read_candidates(path, bands, *, i51b=False, first_seed=None, count=16):
    text = path.read_text(encoding='utf-8').splitlines()
    if text[0] != 'g6-candidate-report/1':
        raise ValueError('candidate schema')
    experiments = [line for line in text if line.startswith('experiment\t')]
    if experiments != (['experiment\tI51b'] if i51b else []):
        raise ValueError('candidate experiment mismatch')
    settings = dict(s.split('=', 1) for s in text[1].split('\t')[1:])
    style = path.name.split('-')[0]
    expected = {'side': '512', 'count': str(count), 'pathLength': '40' if style == 'm06' else '80',
                'buildingDensity': '30' if style == 'm06' else '40', 'rockBlocks': 'false' if style == 'm06' else 'true',
                'g5': 'true', 'mode': 'ranking'}
    if settings != expected:
        raise ValueError(f'{path}: matrix settings mismatch')
    actual_bands = {}
    for line in text:
        if line.startswith('band\t'):
            _, name, ref, lo, hi = line.split('\t')
            if name in actual_bands:
                raise ValueError('duplicate reported band')
            actual_bands[name] = (integer(ref), integer(lo), integer(hi))
    if actual_bands != bands:
        raise ValueError('reported bands differ from frozen policy')
    header = next(i for i, s in enumerate(text) if s.startswith('userSeed\t'))
    columns = text[header].split('\t')
    if len(set(columns)) != len(columns) or not set(METRICS).issubset(columns):
        raise ValueError('missing/duplicate metric column')
    records = list(csv.DictReader(io.StringIO('\n'.join(s for s in text[header:]
        if not s.startswith('winner\t') and not (i51b and s.startswith('winnerB\t')))), delimiter='\t'))
    if first_seed is None:
        first_seed = 2001 if i51b else 1001
    groups = {seed: [] for seed in range(first_seed, first_seed + 32)}
    for row in records:
        if None in row or any(v is None for v in row.values()):
            raise ValueError('malformed candidate row')
        seed, index = integer(row['userSeed']), integer(row['index'])
        if seed not in groups or row['hardFail'] not in ('true', 'false'):
            raise ValueError('unexpected seed/hard flag')
        metrics = {m: value(row[m]) for m in METRICS}
        in_band = sum(v is not None and bands[m][1] <= v * 100 <= bands[m][2] for m, v in metrics.items())
        distance = math.floor(1000 * sum(contribution(metrics[m], bands[m][0], m.startswith('scale.')) for m in METRICS) + .5)
        if (integer(row['inBand']), integer(row['distance'])) != (in_band, distance):
            raise ValueError('core score disagrees with independent Python calculation')
        groups[seed].append({'index': index, 'hard': row['hardFail'] == 'true', 'metrics': metrics,
                             'inBand': in_band, 'distance': distance})
        if index == 0 and integer(row['seed']) != seed:
            raise ValueError('candidate zero changed user seed')
    winners = {}
    for line in text[header + 1:]:
        if line.startswith('winner\t'):
            _, seed, index, hard, in_band, distance = line.split('\t')
            seed = integer(seed)
            if seed in winners:
                raise ValueError('duplicate winner')
            winners[seed] = (integer(index), hard, integer(in_band), integer(distance))
    for seed, rows in groups.items():
        rows.sort(key=lambda r: r['index'])
        if [r['index'] for r in rows] != list(range(count)):
            raise ValueError(f'matrix requires exactly {count} candidates for every seed')
        best = min(rows, key=lambda r: (r['hard'], -r['inBand'], r['distance'], r['index']))
        if winners.get(seed) != (best['index'], str(best['hard']).lower(), best['inBand'], best['distance']):
            raise ValueError('missing or incorrect winner')
    if set(winners) != set(groups):
        raise ValueError('unexpected winners')
    return groups, {s: w[0] for s, w in winners.items()}


def evaluate(groups, winners, references):
    coverage = {}
    for alias in MAIN_MAPS + RELATED:
        known = [m for m in METRICS if references[alias][m] is not None]
        if not known:
            raise ValueError(f'{alias}: no differentiating reference positions')
        varying = [m for m in known if len({contribution(r['metrics'][m], references[alias][m], m.startswith('scale.'))
                    for rows in groups.values() for r in rows}) > 1]
        coverage[alias] = {'referenceKnownPositions': len(known), 'varyingContributionPositions': len(varying),
                           'varyingMetrics': varying}
    pairs = []
    for seed, rows in groups.items():
        distances = {m: [map_distance(r['metrics'], references[m]) for r in rows] for m in MAIN_MAPS + RELATED}
        families = [family_distances({m: distances[m][i] for m in distances}) for i in range(16)]
        distances.update({f'family.{f}': [r[f] for r in families] for f in families[0]})
        results = {name: pair_result(d, [r['hard'] for r in rows], winners[seed]) for name, d in distances.items()}
        pairs.append({'seed': seed, 'winner': winners[seed], 'hardFail': rows[winners[seed]]['hard'], 'results': results})
    summary = {}
    for name in pairs[0]['results']:
        baseline = sum((p['results'][name]['baseline'] for p in pairs), Fraction()) / len(pairs)
        summary[name] = {'successes': sum(p['results'][name]['success'] for p in pairs), 'pairs': len(pairs),
                         'randomBaselineExact': str(baseline), 'randomBaseline': float(baseline)}
    return {'thresholdPassed': summary['family.main']['successes'] >= 23, 'mapCoverage': coverage,
            'summary': summary, 'pairs': pairs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('private_root', 'profile_dir', 'report_dir', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        references, audit = load_references(args.private_root, args.profile_dir)
        report = {'schema': 'g6-holdout/1', 'policyCommit': '396e057', 'referenceAudit': audit, 'profiles': {}}
        for style in ('m06', 'm19'):
            first, second = (args.report_dir / f'{style}-run{i}.tsv' for i in (1, 2))
            if first.read_bytes() != second.read_bytes():
                raise ValueError('candidate repeats differ')
            groups, winners = read_candidates(first, load_bands(BANDS_DIR / f'{style}.bands'))
            result = evaluate(groups, winners, references)
            result['candidateReportSha256'] = hashlib.sha256(first.read_bytes()).hexdigest()
            report['profiles'][style] = result
        report['thresholdPassed'] = all(r['thresholdPassed'] for r in report['profiles'].values())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True, default=str) + '\n', encoding='utf-8')
        for style, result in report['profiles'].items():
            print(style, json.dumps(result['summary']['family.main'], sort_keys=True))
        print('I51_HOLDOUT_PASS' if report['thresholdPassed'] else 'I51_HOLDOUT_FAIL')
        return 0 if report['thresholdPassed'] else 1
    except (OSError, ValueError, KeyError, TypeError, OverflowError, StopIteration, IndexError) as error:
        print(f'I51_MEASUREMENT_BLOCKED: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
