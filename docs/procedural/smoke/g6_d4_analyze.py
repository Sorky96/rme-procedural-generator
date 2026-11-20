"""D4: summarise g1_run_leak_trend.py runs (memory after closing the dialog, cycles after warm-up).

Reads leak-trend.csv of each RUN directory (and heap-trend.csv / heap-buckets.csv when the run used
--heap) and prints JSON. Separates three things that private bytes alone cannot tell apart:
  - growth after warm-up: private bytes from cycle 2 on, slope per block of cycles, plateau test;
  - live allocations: bytes in busy CRT heap blocks (HeapWalk) and HeapSummary.cbAllocated of all heaps;
  - reserve: committed CRT heap memory that is not in a busy block, the part released by HeapCompact at
    the end, and private commit outside the Win32 heaps (VirtualQuery MEM_PRIVATE minus heap commit).
Usage: g6_d4_analyze.py RUN [RUN ...] (RUN = runner OUTPUT directory or its run/ subdirectory)
"""
import csv
import json
from pathlib import Path
import sys

MIB = 1024 * 1024
WARMUP = 2


def slope(points):
    n = len(points)
    if n < 2:
        return None
    mx = sum(x for x, _ in points) / n
    my = sum(y for _, y in points) / n
    den = sum((x - mx) ** 2 for x, _ in points)
    return sum((x - mx) * (y - my) for x, y in points) / den


def mib(value, scale=MIB):
    return None if value is None else round(value / scale, 2)


def blocks(cycles, size):
    """Consecutive cycle blocks after warm-up: 3..size+2, ... (last block may be shorter)."""
    first = WARMUP + 1
    return [(start, min(start + size - 1, cycles)) for start in range(first, cycles + 1, size)]


def series_summary(by_cycle, key, cycles, scale=MIB):
    def mib(value):
        return None if value is None else round(value / scale, 2)
    after = [(c, by_cycle[c][key]) for c in range(WARMUP, cycles + 1)]
    out = {
        'baseline': mib(by_cycle[0][key]),
        'after_warmup_cycle2': mib(by_cycle[WARMUP][key]),
        'last': mib(by_cycle[cycles][key]),
        'growth_warmup_phase_0_to_2': mib(by_cycle[WARMUP][key] - by_cycle[0][key]),
        'growth_after_warmup_2_to_last': mib(by_cycle[cycles][key] - by_cycle[WARMUP][key]),
        'slope_after_warmup_mib_per_cycle': mib(slope(after[1:])),
    }
    size = 10 if cycles <= 30 else 50
    out[f'slope_per_{size}_cycle_block'] = {
        f'{a}-{b}': mib(slope([(c, by_cycle[c][key]) for c in range(a, b + 1)])) for a, b in blocks(cycles, size)}
    if cycles >= 200:
        mean = lambda a, b: sum(by_cycle[c][key] for c in range(a, b + 1)) / (b - a + 1)
        out['mean_101_150'] = mib(mean(101, 150))
        out['mean_151_200'] = mib(mean(151, 200))
        out['plateau_diff_mean_151_200_minus_101_150'] = mib(mean(151, 200) - mean(101, 150))
    return out


def analyze(path):
    run = path / 'run' if (path / 'run').is_dir() else path
    rows = list(csv.DictReader((run / 'leak-trend.csv').open(newline='')))
    cycles = int(rows[-1]['cycle'])
    proc = {int(r['cycle']): {k: int(v) for k, v in r.items() if k != 'cycle'} for r in rows}
    result = {'run': path.name, 'cycles': cycles,
              'process_private_bytes': series_summary(proc, 'private_bytes', cycles),
              'process_working_set': series_summary(proc, 'working_set', cycles)}
    for key in ('gdi_objects', 'user_objects', 'handles'):
        values = [proc[c][key] for c in range(WARMUP, cycles + 1)]
        result[key] = {'after_warmup_min': min(values), 'after_warmup_max': max(values), 'last': proc[cycles][key]}
    heap_file = run / 'heap-trend.csv'
    if not heap_file.is_file():
        return result
    heap_rows = list(csv.DictReader(heap_file.open(newline='')))
    numeric = lambda r: {k: float(v) for k, v in r.items() if k not in ('cycle', 'stage')}
    heap = {int(r['cycle']): numeric(r) for r in heap_rows if r['stage'] == 'cycle'}
    compact = next(numeric(r) for r in heap_rows if r['stage'] == 'compact')
    for sample in list(heap.values()) + [compact]:
        sample['crt_reserve_committed_not_busy'] = sample['crt_summary_committed'] - sample['crt_summary_allocated']
        sample['private_outside_heaps'] = sample['vq_private_commit'] - sample['summary_committed']
    keys = ('private_bytes', 'vq_private_commit', 'summary_allocated', 'summary_committed', 'crt_summary_allocated',
            'crt_summary_committed', 'walk_busy', 'crt_reserve_committed_not_busy', 'walk_free',
            'private_outside_heaps', 'walk_busy_blocks')
    result['heap'] = {}
    for key in keys:
        result['heap'][key] = series_summary(heap, key, cycles, scale=1 if key == 'walk_busy_blocks' else MIB)
    last = heap[cycles]
    result['heap']['final_compact'] = {
        'private_bytes_before': mib(last['private_bytes']), 'private_bytes_after': mib(compact['private_bytes']),
        'released_by_heapcompact': mib(last['private_bytes'] - compact['private_bytes']),
        'busy_before': mib(last['walk_busy']), 'busy_after': mib(compact['walk_busy']),
        'heap_committed_before': mib(last['summary_committed']), 'heap_committed_after': mib(compact['summary_committed']),
    }
    result['heap']['faults_total'] = int(sum(s['faults'] for s in heap.values()) + compact['faults'])
    result['heap']['walk_ms_max'] = max(s['walk_ms'] for s in heap.values())
    result['heap']['heap_count'] = sorted({int(s['heap_count']) for s in heap.values()})
    buckets = {}
    for r in csv.DictReader((run / 'heap-buckets.csv').open(newline='')):
        if r['stage'] == 'cycle' and int(r['cycle']) in (WARMUP, cycles):
            buckets.setdefault(int(r['log2_size']), {})[int(r['cycle'])] = (int(r['blocks']), int(r['bytes']))
    growth = []
    for bucket, values in buckets.items():
        b0, by0 = values.get(WARMUP, (0, 0))
        b1, by1 = values.get(cycles, (0, 0))
        growth.append({'size_range': f'[2^{bucket}, 2^{bucket + 1})', 'blocks_delta': b1 - b0,
                       'bytes_delta_mib': mib(by1 - by0)})
    growth.sort(key=lambda g: -abs(g['bytes_delta_mib']))
    result['heap']['busy_bucket_delta_cycle2_to_last_top'] = growth[:8]
    return result


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    print(json.dumps([analyze(Path(arg).resolve()) for arg in sys.argv[1:]], indent=2))
