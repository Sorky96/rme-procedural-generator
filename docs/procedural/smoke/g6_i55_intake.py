"""bd89a30: offline I55 intake, human review, freeze, and unseen-family evaluation.

Only standard library; no ratings/key access, generation, fitting, or network.
All output and private input paths must be in scratchpad. See g6_i55_intake.md.
"""
import argparse
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

import importlib.util


def sibling(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


i55 = sibling('g6_i55')
g4 = i55.g6
copy = sibling('g6_build_copy_index')
POLICY = 'bd89a30103d9eb2bafdb627cc3c77081d5352bb0'
KNOWN = {f'M{i:02}' for i in range(1, 20)}


def canonical(data):
    return json.dumps(data, sort_keys=True, separators=(',', ':'), allow_nan=False).encode('utf-8')


def digest(data):
    return hashlib.sha256(canonical(data)).hexdigest()


def read(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('duplicate JSON key')
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique,
                      parse_constant=lambda _: (_ for _ in ()).throw(ValueError('nonfinite JSON')))


def text(value):
    if not isinstance(value, str) or not value.strip():
        raise ValueError('nonempty declaration/identifier required')
    return value


def token(value):
    if not re.fullmatch(r'[A-Za-z0-9_-]+', text(value)):
        raise ValueError('invalid alias/family identifier')
    return value


def related(entries, fingerprints):
    """G4 generic ceil(N/4), min 2; single linkage, >=16 and >=100 permille."""
    counts = Counter(h for fp in fingerprints for h in fp)
    threshold = max(2, (len(entries) + 3) // 4)
    filtered = [{h for h in fp if counts[h] < threshold} for fp in fingerprints]
    if any(len(fp) < 16 for e, fp in zip(entries, filtered) if not e['prior']):
        raise ValueError('relatedness not comparable after generic-window filter')
    parent = list(range(len(entries)))

    def root(x):
        while parent[x] != x:
            x = parent[x]
        return x

    overlaps, comparisons = [], []
    for a, ea in enumerate(entries):
        for b in range(a):
            eb = entries[b]
            shared = len(filtered[a] & filtered[b])
            permille = max(shared * 1000 // len(filtered[a]) if filtered[a] else 0,
                           shared * 1000 // len(filtered[b]) if filtered[b] else 0)
            declared = ea['family'] == eb['family'] or bool(set(ea['identifiers']) & set(eb['identifiers']))
            comparisons.append({'a': ea['alias'], 'b': eb['alias'], 'declared': declared,
                                'aWindows': len(filtered[a]), 'bWindows': len(filtered[b]),
                                'comparable': bool(filtered[a] and filtered[b]),
                                'shared': shared, 'maxPermille': permille})
            if declared or shared >= 16 and permille >= 100:
                parent[root(a)] = root(b)
                overlaps.append({'a': ea['alias'], 'b': eb['alias'], 'declared': declared,
                                 'shared': shared, 'maxPermille': permille})
    families = {}
    for n, e in enumerate(entries):
        families.setdefault(root(n), []).append(e['alias'])
    for members in families.values():
        if any(e['prior'] and e['alias'] in members for e in entries) and any(
                not e['prior'] and e['alias'] in members for e in entries):
            raise ValueError('new source belongs to a previously exposed family')
    return {'genericThreshold': threshold, 'before': [len(fp) for fp in fingerprints],
            'after': [len(fp) for fp in filtered], 'links': overlaps, 'comparisons': comparisons,
            'families': {min(v): sorted(v) for v in families.values()}}


def intake(spec_path):
    """Audit without candidate inputs or distances. Paths are absolute and explicit."""
    inputs = {}

    def bind(path):
        p = i55.private(path)
        inputs[str(p)] = i55.sha(p)
        return p

    def evidence(path):
        p = bind(path)
        if not p.stat().st_size:
            raise ValueError('empty declaration evidence')
        return p

    spec = read(bind(spec_path))
    if spec['schema'] != 'g6-i55-intake/1' or spec['policyCommit'] != POLICY:
        raise ValueError('intake schema/policy mismatch')
    if spec['evidenceKind'] not in ('synthetic-test', 'real-evaluation'):
        raise ValueError('explicit synthetic-test or real-evaluation evidence kind required')
    declaration = spec['declaration']
    for key in ('curator', 'date', 'historyScope'):
        text(declaration[key])
    if declaration['completePriorExposureRegistry'] is not True:
        raise ValueError('complete prior exposure registry required')
    evidence(declaration['evidence'])
    entries = spec['maps']
    if not entries or not any(not e['prior'] for e in entries):
        raise ValueError('no new-family sources supplied')
    aliases = [token(e['alias']) for e in entries]
    if len(set(aliases)) != len(aliases) or not KNOWN.issubset({e['alias'] for e in entries if e['prior'] is True}):
        raise ValueError('unique aliases and all 19 historical maps required')
    profiles = i55.private(spec['profiles'])
    for style in i55.STYLES:
        bind(profiles / f'{style}.profile')
        # Public bands/scripts are also bound; private() is not needed for public code.
        p = g4.BANDS_DIR / f'{style}.bands'
        inputs[str(p)] = i55.sha(p)
    assets = {k: i55.sha(bind(spec['assets'][k])) for k in ('dat', 'spr', 'otb')}
    identity = None
    for style, alias in (('m06', 'M06'), ('m19', 'M19')):
        current = copy.profile(profiles / f'{style}.profile', alias)
        if current[:3] != ('10.98', assets['otb'], assets['dat']) or identity is not None and current != identity:
            raise ValueError('profile/client/assets mismatch')
        identity = current
    refs, audits, fps, hashes = {}, {}, [], {}
    for e in entries:
        alias = e['alias']
        token(e['family'])
        if type(e['prior']) is not bool or not isinstance(e['identifiers'], list) or not e['identifiers']:
            raise ValueError('world/source/related identifiers and boolean prior required')
        for identifier in e['identifiers']:
            text(identifier)
        if len(set(e['identifiers'])) != len(e['identifiers']):
            raise ValueError('duplicate related identifier')
        source = bind(e['source'])
        source_sha = i55.sha(source)
        if source_sha in hashes and not (hashes[source_sha] and e['prior']):
            raise ValueError('duplicate source identity (including renamed known maps)')
        hashes[source_sha] = e['prior']
        if not e['prior']:
            d = e['declaration']
            for key in ('provenance', 'rightsBasis', 'rightsHolder', 'attestedBy', 'date'):
                text(d[key])
            if d['permissionToUse'] is not True or d['neverUsedInAnalysisOrTuning'] is not True:
                raise ValueError('permission and no-prior-exposure attestation required')
            for key in ('provenanceEvidence', 'permissionEvidence', 'exposureEvidence'):
                evidence(d[key])
        root = i55.private(e['bundles'])
        fingerprints = []
        for bundle in g4.BUNDLES:
            directory = root / bundle / alias
            run = directory / 'run'
            for name in ('verification.json', 'run/reference-report.json', 'run/semantic-report.json',
                         'run/keymap.tsv', 'run/client-id.txt', 'run/analysis/fingerprint.fp'):
                bind(directory / name)
            # Bind all summaries, including repeated fields used for consistency checks.
            for p in sorted((run / 'analysis').glob('*-surface.txt')):
                bind(p)
            otbs = list((run / 'data').glob('*/items.otb'))
            if len(otbs) != 1 or i55.sha(bind(otbs[0])) != assets['otb']:
                raise ValueError('bundle OTB differs from accepted profile')
            imported = read(run / 'reference-report.json')
            receipt = read(directory / 'verification.json')
            if imported['sha256'] != source_sha or receipt['source_hashes_unchanged']['Tibia.spr'] != assets['spr']:
                raise ValueError('source/client identity mismatch')
            _, fp = copy.parse_fp((run / 'analysis/fingerprint.fp').read_bytes())
            fp_lines = (run / 'analysis/fingerprint.fp').read_text(encoding='ascii').splitlines()
            counts = dict(field.split('=') for field in fp_lines[1].split()[1:])
            if (set(counts) != {'windows', 'sparse', 'candidates', 'uniform', 'kept'}
                    or any(not re.fullmatch(r'\d+', v) for v in counts.values())
                    or any(not re.fullmatch(r'[0-9a-f]{16}', line) for line in fp_lines[2:])):
                raise ValueError('invalid G4 fingerprint encoding/counts')
            if any(not 0 <= h < 2**64 for h in fp):
                raise ValueError('invalid G4 fingerprint hash')
            fingerprints.append(fp)
        if any(fp != fingerprints[0] for fp in fingerprints):
            raise ValueError('ground-name fingerprints disagree across bundles')
        r, a = g4.load_references(root, profiles, aliases=[alias], check_bands=False)
        if alias in ('M06', 'M19') and any(r[alias][m] != g4.load_bands(
                g4.BANDS_DIR / f'{alias.lower()}.bands')[m][0] for m in g4.METRICS):
            raise ValueError('historical profile references disagree with frozen bands')
        if a['datSha256'] != assets['dat'] or a['sprSha256'] != assets['spr']:
            raise ValueError('bundle assets mismatch')
        refs.update(r)
        audits[alias] = a
        fps.append(fingerprints[0])
    split = related(entries, fps)
    families = {f: ms for f, ms in split['families'].items() if not next(e['prior'] for e in entries if e['alias'] == ms[0])}
    for name in ('g6_i55_intake.py', 'g6_i55.py', 'g6_evaluate_holdout.py', 'g6_measure_candidates.py', 'g6_build_copy_index.py'):
        p = Path(__file__).with_name(name).resolve()
        inputs[str(p)] = i55.sha(p)
    check_inputs(inputs)
    return {'schema': 'g6-i55-family-manifest/1', 'policyCommit': POLICY, 'I55': 'OPEN',
            'evidenceKind': spec['evidenceKind'],
            'inputs': inputs, 'inputSha256': digest(inputs), 'split': split, 'families': families,
            'units': sorted(m for ms in families.values() for m in ms),
            'references': {m: refs[m] for ms in families.values() for m in ms}, 'referenceAudit': audits,
            'profiles': str(profiles), 'client': '10.98', 'assets': assets,
            'aggregation': 'equal families; equal maps within family; fixed 26 positions',
            'seeds': list(i55.SEEDS), 'strictWinsRequiredPerProfile': 23}


def check_inputs(inputs):
    for path, expected in inputs.items():
        if i55.sha(path) != expected:
            raise ValueError('frozen input changed: ' + path)


def template(output):
    """Write an intentionally ineligible, private form; never invent attestations."""
    root = i55.private(output).parent
    declared = {'M02': 'klank-samples', 'M03': 'klank-samples', 'M04': 'klank-samples',
                'M07': 'shape', 'M08': 'shape', 'M09': 'shape', 'M10': 'shape',
                'M11': 'winter', 'M18': 'winter', 'M12': 'swamp', 'M13': 'swamp',
                'M14': 'caves', 'M16': 'caves'}
    entries = []
    for alias in sorted(KNOWN) + ['NEW01']:
        prior = alias in KNOWN
        entry = {'alias': alias, 'prior': prior, 'family': declared.get(alias, alias.lower()),
                 'identifiers': [], 'source': str(root / 'sources' / f'{alias}.otbm'),
                 'bundles': str(root / 'bundles')}
        if not prior:
            entry['declaration'] = {'provenance': '', 'rightsBasis': '', 'rightsHolder': '',
                'attestedBy': '', 'date': '', 'permissionToUse': False, 'neverUsedInAnalysisOrTuning': False,
                **{k: str(root / 'evidence' / (k + '.txt')) for k in
                   ('provenanceEvidence', 'permissionEvidence', 'exposureEvidence')}}
        entries.append(entry)
    i55.write_json(output, {'schema': 'g6-i55-intake/1', 'policyCommit': POLICY,
        'evidenceKind': 'UNVERIFIED',
        'profiles': str(root / 'profiles'), 'assets': {k: str(root / 'assets' / name)
            for k, name in (('dat', 'Tibia.dat'), ('spr', 'Tibia.spr'), ('otb', 'items.otb'))},
        'declaration': {'curator': '', 'date': '', 'historyScope': '', 'completePriorExposureRegistry': False,
                        'evidence': str(root / 'evidence/history.txt')}, 'maps': entries})


def prepare(spec, output):
    manifest = intake(spec)
    i55.write_json(output, {'schema': 'g6-i55-intake-review/1', 'accepted': False,
                          'reviewer': '', 'date': '', 'inputSha256': manifest['inputSha256'],
                          'families': manifest['families'], 'units': manifest['units'],
                          'split': manifest['split'], 'inputs': manifest['inputs']})


def freeze(spec, review, output):
    output = i55.private(output)
    if output.exists():
        raise ValueError('freeze directory must be new')
    manifest = intake(spec)
    review = i55.private(review)
    approval = read(review)
    if (approval['schema'] != 'g6-i55-intake-review/1' or approval['accepted'] is not True
            or approval['inputSha256'] != manifest['inputSha256']):
        raise ValueError('explicit human acceptance of unchanged intake required')
    text(approval['reviewer'])
    text(approval['date'])
    manifest['review'] = {'path': str(review), 'sha256': i55.sha(review)}
    manifest['inputs'][str(review)] = i55.sha(review)
    manifest['frozenInputSha256'] = digest(manifest['inputs'])
    check_inputs(manifest['inputs'])
    output.mkdir(parents=True, exist_ok=False)
    i55.write_json(output / 'manifest.json', manifest)
    i55.write_json(output / 'seal.json', {'manifestSha256': i55.sha(output / 'manifest.json')})


def verify(manifest):
    manifest = i55.private(manifest)
    if read(manifest.with_name('seal.json'))['manifestSha256'] != i55.sha(manifest):
        raise ValueError('frozen manifest changed')
    frozen = read(manifest)
    if frozen['schema'] != 'g6-i55-family-manifest/1' or frozen['policyCommit'] != POLICY:
        raise ValueError('frozen family manifest schema/policy')
    check_inputs(frozen['inputs'])
    return frozen


def check_generator_inputs(frozen, profiles, index):
    """Use before generation as well as evaluation; no measurement here."""
    for style in i55.STYLES:
        if i55.sha(Path(profiles) / f'{style}.profile') != frozen['inputs'][str(Path(frozen['profiles']) / f'{style}.profile')]:
            raise ValueError('generator uses different profile')
    index = Path(index).read_text(encoding='utf-8').splitlines()
    if index[:2] != ['i53-index/1 g4-natural-v1',
                     f"10.98 {frozen['assets']['otb']} {frozen['assets']['dat']}"]:
        raise ValueError('matrix copy index assets/projection differ from intake')
    for alias in ('M06', 'M19'):
        source = frozen['referenceAudit'][alias]['maps'][alias]['mapSha256']
        rows = [s.split() for s in index if s.split()[:2] == [alias, source]]
        keymaps = {sha for path, sha in frozen['inputs'].items() if
                   Path(path).as_posix().endswith(f'/{alias}/run/keymap.tsv')}
        if len(rows) != 1 or len(rows[0]) != 5 or rows[0][3] not in keymaps:
            raise ValueError('matrix copy index uses different historical source')


def evaluate(manifest, matrix, output):
    frozen = verify(manifest)
    matrix = i55.private(matrix)
    matrix_manifest = read(matrix / 'manifest.json')
    binding = matrix_manifest['inputs']['familyManifest']
    if Path(binding['path']).resolve() != Path(manifest).resolve() or binding['sha256'] != i55.sha(manifest):
        raise ValueError('matrix was not generated with this frozen family manifest')
    for style in i55.STYLES:
        if matrix_manifest['inputs'][style]['sha256'] != frozen['inputs'][str(Path(frozen['profiles']) / f'{style}.profile')]:
            raise ValueError('matrix uses different profile')
    check_generator_inputs(frozen, frozen['profiles'], matrix_manifest['inputs']['index']['path'])
    groups, audit = i55.read_matrix(matrix)
    report = {'schema': 'g6-i55-unseen-result/1', 'policyCommit': POLICY, 'I55': 'OPEN', 'G6': 'OPEN',
              'evidenceKind': frozen['evidenceKind'], 'countsTowardI55': frozen['evidenceKind'] == 'real-evaluation',
              'blind': 'NOT READ', 'familyManifestSha256': i55.sha(manifest),
              'matrixManifestSha256': i55.sha(matrix / 'manifest.json'), 'I53': audit,
              'families': frozen['families'], 'profiles': {}}
    for style in i55.STYLES:
        report['profiles'][style] = i55.compare(groups[style], frozen['references'], frozen['families'])
    report['quantitativeThresholdPassed'] = all(r['summary']['atLeast23'] for r in report['profiles'].values())
    report['eligibleQuantitativeEvidence'] = report['countsTowardI55'] and report['quantitativeThresholdPassed']
    check_inputs(frozen['inputs'])
    i55.write_json(output, report)
    return 0 if report['quantitativeThresholdPassed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    for name, params in {'template': ('output',), 'prepare': ('spec', 'output'), 'freeze': ('spec', 'review', 'output'),
                         'verify': ('manifest',), 'evaluate': ('manifest', 'matrix', 'output')}.items():
        p = sub.add_parser(name)
        for param in params:
            p.add_argument(param, type=Path)
    args = vars(parser.parse_args())
    command = args.pop('command')
    try:
        result = globals()[command](**args)
        print('I55_INTAKE_' + command.upper() + ': G6 OPEN; no blind key read')
        return result if type(result) is int else 0
    except (OSError, ValueError, KeyError, TypeError, IndexError, StopIteration, OverflowError) as error:
        print(f'I55_INTAKE_BLOCKED: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
