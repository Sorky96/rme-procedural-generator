"""G5 gate: --i41 includes cave exits/doors; --i42 also compares mountain passes=0/4;
--i43 runs passes=4 with building storeys off and on (default 30 %)."""
import argparse
import json
import re
import statistics
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

root = Path(__file__).resolve().parents[3]
profiles = Path('C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--i41', action='store_true')
parser.add_argument('--i42', action='store_true')
parser.add_argument('--i43', action='store_true')
args = parser.parse_args()
args.i41 |= args.i42 or args.i43
output = root / ('local-data/i43-measurement' if args.i43 else 'local-data/i42-measurement' if args.i42 else 'local-data/i41-measurement' if args.i41 else 'local-data/i40-measurement')
output.mkdir(parents=True, exist_ok=True)
def measure(spec):
    profile, density, length, passes, storeys = spec
    label = f'{profile}-storeys{storeys}' if args.i43 else f'{profile}-passes{passes}' if args.i42 else profile
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
            command += ['caveFarExits=1', 'caveDoorCorridors=1'] if args.i41 else ['caveFarExits=0']
            if args.i42 or args.i43:
                command += [f'passes={passes}']
            if args.i43:
                command += [f'buildingStoreys={storeys}']
            command += [str(seed) for seed in range(1001, 1033)]
            result = subprocess.run(command, capture_output=True)
            (output / f'{label}-retry{retries}-run{run}.log').write_bytes(result.stdout)
            (output / f'{label}-retry{retries}-run{run}.stderr').write_bytes(result.stderr)
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
        for category in ['doors', 'sites', 'caveEntrances', 'caveExits', 'caps', 'caveCells', 'storeys']:
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
        islands = [tuple(map(int, row)) for row in re.findall(r'level islands total (\d+) nonShoreIslands (\d+) nonShoreCells (\d+)', text)]
        assert len(islands) == 32
        caves = [tuple(map(int, row)) for row in re.findall(
            r'level cave i41 components (\d+) far (\d+) exits (\d+) distanceSum (\d+) minDistance (\d+) corridors (\d+) invalidDoors (\d+)', text)]
        cave_metrics = {}
        if args.i41:
            assert len(caves) == 32
            cave_metrics = {
                'componentExitShareMedian': statistics.median(f / c if c else 1 for c, f, *_ in caves),
                'components': sum(c[0] for c in caves), 'componentsWithFarExit': sum(c[1] for c in caves),
                'distanceMean': sum(c[3] for c in caves) / max(1, sum(c[2] for c in caves)),
                'distanceMinimum': min(c[4] for c in caves if c[2]),
                'seedsWithDoors': sum(c[5] > 0 for c in caves),
                'corridors': sum(c[5] for c in caves), 'invalidDoors': sum(c[6] for c in caves),
            }
        rows.append({'profile': profile, 'passes': passes, 'storeys': storeys, 'retries': retries, 'identical': True,
                        'allSeeds': sum(s[3] == 'true' for s in seeds),
                        'edges': [sum(int(s[i]) for s in seeds) for i in [4, 5]],
                        'categories': categories, 'pockets': pockets, 'repairs': repairs,
                        'entrancesPerSeed': entrance_counts, 'entranceMedian': statistics.median(entrance_counts),
                        'vegetation': vegetation, 'removedCaveComponents': sum(pruned),
                        'islands': sum(i for i, _, _ in islands),
                        'nonShoreIslands': sum(i for _, i, _ in islands),
                        'islandNonShoreCells': sum(c for _, _, c in islands),
                        'unrepairedSeeds': sum(line[5] == 'true' for line in repair_lines),
                        'removedPoiFraction': removed_pois / max(1, retained_pois + removed_pois)})
        rows[-1]['caveI41'] = cave_metrics
        if args.i43 and storeys:
            built = [tuple(map(int, row)) for row in re.findall(
                r'level storeys qualifying (\d+) chosen (\d+) placed (\d+) collisions (\d+) noStairs (\d+) removed (\d+) '
                r'final (\d+) withStairs (\d+) overlaps (\d+)', text)]
            assert len(built) == 32
            rows[-1]['buildingStoreys'] = {
                'shareMedian': statistics.median(b[6] / b[0] for b in built if b[0]),
                'shares': [round(b[6] / b[0], 3) if b[0] else None for b in built],
                'qualifying': sum(b[0] for b in built), 'chosen': sum(b[1] for b in built),
                'placed': sum(b[2] for b in built), 'collisions': sum(b[3] for b in built),
                'noStairs': sum(b[4] for b in built), 'removed': sum(b[5] for b in built),
                'final': sum(b[6] for b in built), 'withStairs': sum(b[7] for b in built),
                'overlaps': sum(b[8] for b in built)}
        if (args.i42 or args.i43) and passes:
            cuts = [tuple(map(int, row)) for row in re.findall(
                r'level passes count (\d+) blocked (\d+) covered (\d+) smallCaps (\d+) invalidRoutes (\d+)', text)]
            assert len(cuts) == 32
            routes = [tuple(map(int, row)) for row in re.findall(r'level pass route through (-?\d+) bypass (-?\d+)', text)]
            rows[-1]['mountainPasses'] = {'median': statistics.median(c[0] for c in cuts),
                'sum': sum(c[0] for c in cuts), 'blocked': sum(c[1] for c in cuts),
                'covered': sum(c[2] for c in cuts), 'smallCaps': sum(c[3] for c in cuts),
                'invalidRoutes': sum(c[4] for c in cuts),
                'maximumRatio': max((a / b for a, b in routes if b > 0), default=0)}
        print(rows[-1], flush=True)
    return rows

with ThreadPoolExecutor(max_workers=2) as executor:
    specs = [(p, d, l, passes, storeys) for passes in ([4] if args.i43 else [0, 4] if args.i42 else [0])
             for storeys in ([0, 1] if args.i43 else [0])
             for p, d, l in [('m06', 30, 40), ('m19', 40, 80)]]
    summary = [row for rows in executor.map(measure, specs) for row in rows]
(output / 'summary.json').write_text(json.dumps(summary, indent=2))
passed = all(row['allSeeds'] >= (32 if row['retries'] or args.i41 else 30)
             and row['edges'][0] == row['edges'][1]
             and row['unrepairedSeeds'] == 0
             and row['removedPoiFraction'] <= .02 and row['entranceMedian'] >= 8
             and row['islandNonShoreCells'] == 0
             and row['vegetation'] and all(v['ratioMedian'] >= .7 for v in row['vegetation'].values()) for row in summary)
if args.i41:
    passed &= all(row['caveI41']['componentExitShareMedian'] >= .8
                  and row['caveI41']['distanceMean'] >= 24 and row['caveI41']['distanceMinimum'] >= 24
                  and row['caveI41']['seedsWithDoors'] >= 26 and row['caveI41']['invalidDoors'] == 0 for row in summary)
if args.i42 or args.i43:
    passed &= all(row['mountainPasses']['median'] >= 2
                  and all(row['mountainPasses'][key] == 0 for key in ['blocked', 'covered', 'smallCaps', 'invalidRoutes'])
                  for row in summary if row['passes'])
if args.i43:
    passed &= all(row['categories']['storeys']['sum'][0] == row['categories']['storeys']['sum'][1] for row in summary)
    passed &= all(.2 <= row['buildingStoreys']['shareMedian'] <= .4
                  and row['buildingStoreys']['withStairs'] == row['buildingStoreys']['final']
                  and row['buildingStoreys']['overlaps'] == 0 for row in summary if row['storeys'])
print('G5_GATE_PASS' if passed else 'G5_GATE_FAIL', flush=True)
raise SystemExit(0 if passed else 1)
