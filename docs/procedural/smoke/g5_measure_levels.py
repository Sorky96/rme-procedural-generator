"""Iteration 39 gate: identical level repair runs, entrances and cap vegetation."""
import json
import re
import statistics
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

root = Path(__file__).resolve().parents[3]
profiles = Path('C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10')
output = root / 'local-data/i39-measurement'
output.mkdir(parents=True, exist_ok=True)
def measure(spec):
    profile, density, length = spec
    rows = []
    for retries in [0, 8]:
        logs = []
        errors = []
        for run in [1, 2]:
            command = [str(root / 'build-ninja/source/procedural/style_render.exe'),
                       str(profiles / f'{profile}.profile'), str(output), '512',
                       'levelReach=1', f'retrySeeds={retries}', f'buildingDensity={density}',
                       f'pathLength={length}', 'ruinPercent=20', 'campCount=4', 'fordLength=48',
                       'repairReach=1', 'caveEntrances=16', 'caveExitShapes=ramp',
                       'plateauMinTop=64', 'plateauMaxPercent=10', 'plateauLevels=3']
            command += [str(seed) for seed in range(1001, 1033)]
            result = subprocess.run(command, capture_output=True)
            (output / f'{profile}-retry{retries}-run{run}.log').write_bytes(result.stdout)
            (output / f'{profile}-retry{retries}-run{run}.stderr').write_bytes(result.stderr)
            if result.returncode:
                raise RuntimeError((command, result.returncode, result.stderr.decode()))
            logs.append(result.stdout)
            errors.append(result.stderr)
            print(profile, retries, run, 'completed', flush=True)
        assert logs[0] == logs[1], 'nonidentical output'
        assert errors[0] == errors[1], 'nonidentical stderr'
        text = logs[0].decode()
        seeds = re.findall(r'level seed (\d+) used (\d+) attempts (\d+) all (true|false) edges (\d+)/(\d+)', text)
        assert len(seeds) == 32
        categories = {}
        for category in ['doors', 'sites', 'caveEntrances', 'caveExits', 'caps', 'caveCells']:
            counts = [(int(a), int(b)) for a, b in re.findall(rf'level category seed \d+ {category} (\d+)/(\d+)', text)]
            assert len(counts) == 32
            categories[category] = {'sum': [sum(a for a, _ in counts), sum(b for _, b in counts)],
                                    'median': [statistics.median(a for a, _ in counts), statistics.median(b for _, b in counts)]}
        pockets = {}
        for floor in range(5):
            values = [int(x) for x in re.findall(rf'level pockets seed \d+ floor {floor} count (\d+)', text)]
            if values:
                pockets[floor] = {'sum': sum(values), 'median': statistics.median(values), 'range': [min(values), max(values)]}
        repairs = {}
        repair_lines = re.findall(r'level repair seed \d+ repairs (\d+) bridges (\d+) removedCaps (\d+) removedPois (\d+) removedDecor (\d+) unrepaired (true|false)', text)
        assert len(repair_lines) == 32
        for i, name in enumerate(['repairs', 'bridges', 'removedCaps', 'removedPois', 'removedDecor']):
            values = [int(line[i]) for line in repair_lines]
            repairs[name] = {'sum': sum(values), 'median': statistics.median(values), 'range': [min(values), max(values)]}
        retained_pois = sum(categories[c]['sum'][1] for c in ['doors', 'sites', 'caveEntrances'])
        removed_pois = repairs['removedPois']['sum']
        entrance_counts = [int(b) for _, b in re.findall(r'level category seed \d+ caveEntrances (\d+)/(\d+)', text)]
        vegetation = {}
        for level in range(1, 4):
            counts = [tuple(map(int, row)) for row in re.findall(
                rf'level flora level {level} grass (\d+) flora (\d+) surfaceGrass (\d+) surfaceFlora (\d+)', text)]
            cap = [f / g for g, f, sg, sf in counts if g and sg and sf]
            surface = [sf / sg for g, f, sg, sf in counts if g and sg and sf]
            ratios = [f / g / (sf / sg) for g, f, sg, sf in counts if g and sg and sf]
            if ratios:
                vegetation[level] = {'seeds': len(ratios), 'capMedian': statistics.median(cap),
                    'surfaceMedian': statistics.median(surface), 'ratioMedian': statistics.median(ratios)}
        pruned = list(map(int, re.findall(r'level cave pruning seed \d+ removedComponents (\d+)', text)))
        assert len(pruned) == 32
        rows.append({'profile': profile, 'retries': retries, 'identical': True,
                        'allSeeds': sum(s[3] == 'true' for s in seeds),
                        'edges': [sum(int(s[i]) for s in seeds) for i in [4, 5]],
                        'categories': categories, 'pockets': pockets, 'repairs': repairs,
                        'entrancesPerSeed': entrance_counts, 'entranceMedian': statistics.median(entrance_counts),
                        'vegetation': vegetation, 'removedCaveComponents': sum(pruned),
                        'unrepairedSeeds': sum(line[5] == 'true' for line in repair_lines),
                        'removedPoiFraction': removed_pois / max(1, retained_pois + removed_pois)})
        print(rows[-1], flush=True)
    return rows

with ThreadPoolExecutor(max_workers=2) as executor:
    summary = [row for rows in executor.map(measure, [('m06', 30, 40), ('m19', 40, 80)]) for row in rows]
(output / 'summary.json').write_text(json.dumps(summary, indent=2))
passed = all(row['allSeeds'] >= (32 if row['retries'] else 30)
             and row['edges'][0] == row['edges'][1]
             and row['removedPoiFraction'] <= .02 and row['entranceMedian'] >= 8
             and row['vegetation'] and all(v['ratioMedian'] >= .7 for v in row['vegetation'].values()) for row in summary)
print('G5_GATE_PASS' if passed else 'G5_GATE_FAIL', flush=True)
