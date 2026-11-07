"""G3: independently check the semantic-map PNGs of a run against its analysis report.

Usage: py -I g3_verify_semantic_png.py RUN_OUTPUT_DIR
Decodes each PNG with the Python standard library (zlib, struct), not with the code under test,
and compares pixel counts with reference-analysis.json. Reads only; exit code 0 only if all pass.
"""
import json
import struct
import sys
import zlib
from pathlib import Path

root = Path(sys.argv[1]).resolve() / 'run' / 'analysis'
report = json.loads((root / 'reference-analysis.json').read_text(encoding='utf-8'))
legend = json.loads((root / 'semantic' / 'legend.json').read_text(encoding='utf-8'))
floors = {f['z']: f for f in report['floors']}
checks = []


def check(name, ok):
    checks.append((name, bool(ok)))


def decode(path):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        return None
    pos, chunks, idat = 8, [], b''
    ihdr = palette = None
    while pos < len(data):
        length, kind = struct.unpack_from('>I4s', data, pos)
        body = data[pos + 8:pos + 8 + length]
        crc, = struct.unpack_from('>I', data, pos + 8 + length)
        if zlib.crc32(kind + body) != crc:
            return None
        chunks.append(kind)
        if kind == b'IHDR':
            ihdr = struct.unpack('>IIBBBBB', body)
        elif kind == b'PLTE':
            palette = body
        elif kind == b'IDAT':
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    width, height = ihdr[0], ihdr[1]
    if ihdr[2:] != (8, 3, 0, 0, 0) or len(raw) != height * (width + 1):
        return None
    if any(raw[y * (width + 1)] != 0 for y in range(height)):
        return None
    pixels = b''.join(raw[y * (width + 1) + 1:(y + 1) * (width + 1)] for y in range(height))
    return {'w': width, 'h': height, 'pixels': pixels, 'palette': palette, 'chunks': chunks}


check('legend lists every floor of the report', sorted(f['z'] for f in legend['floors']) == sorted(floors))
fixed_rgb = {e['slot']: e['rgb'] for e in legend['legend'] if e['slot'] < 9}
for entry in legend['floors']:
    z = entry['z']
    f = floors[z]
    image = decode(root / 'semantic' / f'floor-{z:02d}.png')
    tag = f'floor {z}'
    check(f'{tag}: PNG decodes, CRCs and filters are valid', image is not None)
    if image is None:
        continue
    check(f'{tag}: chunk order', image['chunks'] == [b'IHDR', b'PLTE', b'IDAT', b'IEND'])
    check(f'{tag}: size equals the floor box', (image['w'], image['h']) == (f['width'], f['height']) == (entry['width'], entry['height']))
    counts = [0] * 256
    for p in image['pixels']:
        counts[p] += 1
    trait = f['trait_tiles']
    check(f'{tag}: non-empty pixels equal tiles', len(image['pixels']) - counts[0] == f['tiles'])
    check(f'{tag}: unknown pixels equal unknown cells', counts[1] == f['unknown_cells'])
    check(f'{tag}: wall/door/teleport/floor-change pixels do not exceed the trait counts',
          counts[2] <= trait['wall'] and counts[3] <= trait['door'] and counts[4] <= trait['teleport'] and counts[5] <= trait['floor_change'])
    check(f'{tag}: palette matches the legend for fixed slots',
          all(list(image['palette'][s * 3:s * 3 + 3]) == rgb for s, rgb in fixed_rgb.items()))
    check(f'{tag}: file size matches the legend', (root / 'semantic' / f'floor-{z:02d}.png').stat().st_size == entry['png_bytes'])

failed = [name for name, ok in checks if not ok]
for name, ok in checks:
    if not ok:
        print('FAIL', name)
print(f'{len(checks) - len(failed)}/{len(checks)} checks passed' + ('' if not failed else ', FAILED'))
sys.exit(1 if failed else 0)
