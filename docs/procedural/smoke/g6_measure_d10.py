"""D10: I53 detection of rotated and mirrored copies (preregistration 7bd6968).

py -I g6_measure_d10.py COPY_INDEX_EXE INDEX_DIR SCRATCH_DIR

INDEX_DIR is the private output of g6_build_copy_index.py: references.i53 and the
M06-z07.raster / M19-z07.raster class rasters the index was built from.

Fragment rule, fixed before any measurement: in each reference's z07 class raster,
take the 128x128 block on the 128-aligned grid (fully inside the raster) with the
most horizontally or vertically adjacent present-cell pairs of different classes.
Ties: smallest y, then smallest x. The rule reads class content only, never a
fingerprint or an assessment.

Each fragment is transformed by all eight symmetries of the square, fingerprinted
by COPY_INDEX_EXE as an independent candidate raster and assessed against the
index: generic windows (intersection of the two references) are removed from the
candidate, then per reference shared unique hashes and candidate containment
(integer per mille) are computed. Copy means shared >= 16 AND containment >= 100
for either reference, as in copy_guard::assessCopy. Fewer than 16 candidate windows
after filtering is reported as too small. The negative control is a synthetic
Voronoi terrain (seed 1729) under the same eight symmetries.

The complete measurement runs twice; the two reports must be byte-identical.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

SIDE = 128
SYMMETRIES = ('identity', 'rot90', 'rot180', 'rot270', 'mirrorX', 'mirrorY', 'transpose', 'antitranspose')


def read_raster(path):
    data = path.read_bytes()
    header, _, cells = data.partition(b'\n')
    schema, w, h = header.decode('ascii').split()
    w, h = int(w), int(h)
    if schema != 'i53-raster/1' or len(cells) != w * h:
        raise ValueError(f'{path}: bad raster')
    return w, h, cells


def choose_fragment(w, h, cells):
    best = None
    for y0 in range(0, h - SIDE + 1, SIDE):
        for x0 in range(0, w - SIDE + 1, SIDE):
            edges = 0
            for y in range(y0, y0 + SIDE):
                row = y * w
                for x in range(x0, x0 + SIDE):
                    c = cells[row + x]
                    if c == 255:
                        continue
                    if x + 1 < x0 + SIDE and cells[row + x + 1] not in (255, c):
                        edges += 1
                    if y + 1 < y0 + SIDE and cells[row + w + x] not in (255, c):
                        edges += 1
            if best is None or edges > best[0]:
                best = (edges, x0, y0)
    if best is None:
        raise ValueError('raster smaller than fragment')
    edges, x0, y0 = best
    block = [cells[(y0 + y) * w + x0:(y0 + y) * w + x0 + SIDE] for y in range(SIDE)]
    return {'x': x0, 'y': y0, 'side': SIDE, 'boundaryPairs': edges}, block


def transform(block, name):
    n = len(block)
    source = {
        'identity': lambda x, y: (x, y),
        'rot90': lambda x, y: (y, n - 1 - x),
        'rot180': lambda x, y: (n - 1 - x, n - 1 - y),
        'rot270': lambda x, y: (n - 1 - y, x),
        'mirrorX': lambda x, y: (n - 1 - x, y),
        'mirrorY': lambda x, y: (x, n - 1 - y),
        'transpose': lambda x, y: (y, x),
        'antitranspose': lambda x, y: (n - 1 - y, n - 1 - x),
    }[name]
    out = bytearray()
    for y in range(n):
        for x in range(n):
            sx, sy = source(x, y)
            out.append(block[sy][sx])
    return bytes(out)


def voronoi(seed, sites=40):
    rng = seed
    def nxt():
        nonlocal rng
        rng = (rng * 6364136223846793005 + 1442695040888963407) % 2**64
        return rng >> 33
    points = [(nxt() % SIDE, nxt() % SIDE, nxt() % 5) for _ in range(sites)]
    rows = []
    for y in range(SIDE):
        row = bytearray()
        for x in range(SIDE):
            row.append(min(points, key=lambda p: ((p[0] - x) ** 2 + (p[1] - y) ** 2, p[0], p[1]))[2])
        rows.append(bytes(row))
    return rows


def parse_fp(text):
    lines = text.splitlines()
    projection = lines.pop(0) if lines[0].startswith('i53-fingerprint/') else 'g4-fingerprint (unversioned)'
    if lines[0] != 'g4-fingerprint/1 window=16 modulo=4 minpresent=50':
        raise ValueError('fingerprint options')
    hashes = [int(s, 16) for s in lines[2:]]
    if hashes != sorted(set(hashes)):
        raise ValueError('fingerprint order')
    return projection, set(hashes)


def read_index(path):
    data = path.read_bytes()
    body, tail = data[:-72], data[-72:]
    if tail != f'sha256 {hashlib.sha256(body).hexdigest()}\n'.encode():
        raise ValueError('index checksum')
    header, rest = body.split(b'\n', 1)
    _, rest = rest.split(b'\n', 1)
    refs = {}
    for alias in ('M06', 'M19'):
        row, rest = rest.split(b'\n', 1)
        fields = row.decode().split()
        if fields[0] != alias:
            raise ValueError('index reference order')
        length = int(fields[4])
        refs[alias] = parse_fp(rest[:length].decode('ascii'))[1]
        rest = rest[length:]
    return header.decode(), hashlib.sha256(data).hexdigest(), refs


def assess(candidate, refs, generic):
    filtered = candidate - generic
    result = {'before': len(candidate), 'after': len(filtered), 'references': {}}
    copied = False
    for alias, ref in refs.items():
        shared = len(filtered & ref)
        permille = shared * 1000 // len(filtered) if filtered else 0
        result['references'][alias] = {'shared': shared, 'permille': permille}
        copied |= shared >= 16 and permille >= 100
    result['state'] = 'too-small' if len(filtered) < 16 else 'copy' if copied else 'clear'
    return result


def measure(exe, index_dir, work):
    header, index_sha, refs = read_index(index_dir / 'references.i53')
    generic = refs['M06'] & refs['M19']
    sources = {}
    for alias in ('M06', 'M19'):
        path = index_dir / f'{alias}-z07.raster'
        w, h, cells = read_raster(path)
        fragment, block = choose_fragment(w, h, cells)
        fragment['rasterSha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        sources[alias] = (fragment, block)
    sources['control-voronoi-1729'] = ({'synthetic': 'voronoi seed=1729 sites=40 classes=0..4'}, voronoi(1729))
    report = {'schema': 'd10-symmetry/1', 'preregistration': '7bd6968', 'index': header, 'indexSha256': index_sha,
              'generic': len(generic), 'sources': {}}
    projection = None
    for name, (fragment, block) in sources.items():
        rows = {}
        for symmetry in SYMMETRIES:
            raster = work / f'{name}-{symmetry}.raster'
            raster.write_bytes(f'i53-raster/1 {SIDE} {SIDE}\n'.encode() + transform(block, symmetry))
            result = subprocess.run([str(exe), str(raster)], capture_output=True, check=True)
            if result.stderr:
                raise ValueError(result.stderr.decode(errors='replace'))
            projection, hashes = parse_fp(result.stdout.decode('ascii'))
            rows[symmetry] = assess(hashes, refs, generic)
        report['sources'][name] = {'fragment': fragment, 'symmetries': rows,
                                   'copies': sum(r['state'] == 'copy' for r in rows.values())}
    report['fingerprint'] = projection
    expected = {'M06': 8, 'M19': 8, 'control-voronoi-1729': 0}
    report['criterion'] = 'PASS' if all(report['sources'][k]['copies'] == v for k, v in expected.items()) else 'FAIL'
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    for name in ('exe', 'index_dir', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if 'scratchpad' not in output.parts:
        parser.error('output must be in scratchpad')
    texts = []
    for run in (1, 2):
        work = output / f'run{run}'
        work.mkdir(parents=True, exist_ok=True)
        text = json.dumps(measure(args.exe.resolve(), args.index_dir.resolve(), work), indent=2) + '\n'
        (output / f'report-run{run}.json').write_text(text)
        texts.append(text)
    if texts[0] != texts[1]:
        raise RuntimeError('D10_REPEATABILITY_FAIL')
    print(texts[0], end='')
    print('D10_REPEATABILITY_PASS', json.loads(texts[0])['criterion'])


if __name__ == '__main__':
    main()
