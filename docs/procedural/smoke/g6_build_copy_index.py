"""I53: build an explicit private index from verified G4 SEMANTIC class rasters.

py -I g6_build_copy_index.py COPY_INDEX_EXE G4_BUNDLE PROFILE_DIR SCRATCH_DIR
Only G4_BUNDLE/M06 and M19 are read. Window hashes are canonical over the eight
symmetries of the square (D10); the index format is i53-index/2. Existing ground-name fingerprint.fp is never read.
Uses Python standard library only; output, class rasters and audit stay in scratchpad.
"""
import argparse
import hashlib
import json
import struct
import subprocess
import zlib
from pathlib import Path


def digest(data):
    return hashlib.sha256(data).hexdigest()


def indexed_png(path):
    """Strict decoder for the indexed 8-bit, noninterlaced, filter-0 G4 exporter.

    Refuse unsupported encodings rather than guessing classes. Validate every CRC,
    dimensions, decompression length and absence of trailing compressed data.
    """
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('PNG signature')
    offset, compressed, dimensions, ended = 8, bytearray(), None, False
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        kind, body = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
        crc = struct.unpack_from('>I', data, offset + 8 + size)[0]
        if zlib.crc32(kind + body) != crc:
            raise ValueError('PNG CRC')
        offset += size + 12
        if kind == b'IHDR':
            w, h, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', body)
            if dimensions or not w or not h or w * h > 64 * 1024**2 or (depth, color, compression, filtering, interlace) != (8, 3, 0, 0, 0):
                raise ValueError('unsupported PNG format')
            dimensions = w, h
        elif kind == b'IDAT':
            compressed.extend(body)
        elif kind == b'IEND':
            ended = True
            break
    if not dimensions or not ended or offset != len(data):
        raise ValueError('incomplete PNG')
    w, h = dimensions
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed, (w + 1) * h + 1)
    if len(raw) != (w + 1) * h or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('PNG decompression length')
    pixels = bytearray()
    for y in range(h):
        row = raw[y * (w + 1):(y + 1) * (w + 1)]
        if row[0] != 0:
            raise ValueError('G4 exporter requires PNG filter 0')
        pixels.extend(row[1:])
    return w, h, pixels


def profile(path, alias):
    lines = path.read_text(encoding='utf-8').splitlines()
    if lines[0] != 'g4-profile/6' or [s for s in lines if s.startswith('source\t')] != [f'source\t{alias}\t1000']:
        raise ValueError('profile must use only the named M06 or M19 reference')
    client = next(s.split('\t')[1] for s in lines if s.startswith('client\t'))
    assets = {parts[1]: parts[2] for s in lines if s.startswith('assets\t') for parts in [s.split('\t')]}
    keys = [s for s in lines if s.startswith('key\t')]
    return client, assets['otb_sha256'], assets['dat_sha256'], keys


FINGERPRINT = 'i53-fingerprint/2 d4-canonical-v1'
INDEX_HEADER = 'i53-index/2 g4-natural-v1 d4-canonical-v1'


def copy_index_fp(data):
    """copy_index output: the D10 version line, then a G4-format fingerprint."""
    first, _, rest = data.partition(b'\n')
    if first.rstrip(b'\r').decode('ascii', errors='replace') != FINGERPRINT:
        raise ValueError('copy_index predates D10 square-symmetry fingerprints; rebuild it')
    return parse_fp(rest)


def parse_fp(data):
    lines = data.decode('ascii').splitlines()
    if lines[0] != 'g4-fingerprint/1 window=16 modulo=4 minpresent=50':
        raise ValueError('fingerprint options')
    counts = {k: int(v) for field in lines[1].split()[1:] for k, v in [field.split('=')]}
    hashes = [int(s, 16) for s in lines[2:]]
    if hashes != sorted(set(hashes)) or counts['kept'] != len(hashes):
        raise ValueError('invalid fingerprint')
    return counts, set(hashes)


def fingerprint(counts, hashes):
    text = 'g4-fingerprint/1 window=16 modulo=4 minpresent=50\n'
    text += 'counts ' + ' '.join(f'{k}={counts[k]}' for k in ('windows', 'sparse', 'candidates', 'uniform')) + f' kept={len(hashes)}\n'
    return (text + ''.join(f'{h:016x}\n' for h in sorted(hashes))).encode('ascii')


def scratch(path):
    path = path.resolve()
    if 'scratchpad' not in path.parts:
        raise ValueError('private output must be in scratchpad')
    path.mkdir(parents=True, exist_ok=True)
    return path


def build(exe, bundle, profiles, output):
    output = scratch(output)
    audit = {'schema': 'i53-index-audit/2', 'projection': 'g4-natural-v1', 'fingerprint': FINGERPRINT,
             'mapping': 'G4 Grass,Dirt,Rock,Shore,Water present; Lake/River -> Water; all other classes absent; unknown-content mask absent',
             'sources': {}}
    entries, identity = [], None
    for alias in ('M06', 'M19'):
        directory = bundle / alias
        run = directory / 'run'
        receipt = json.loads((directory / 'verification.json').read_text())
        imported = json.loads((run / 'reference-report.json').read_text())
        semantic = json.loads((run / 'semantic-report.json').read_text())
        current = profile(profiles / f'{alias.lower()}.profile', alias)
        if identity is None:
            identity = current
        if current != identity:
            raise ValueError('profiles disagree on client/assets/keymap')
        client, otb, dat, keys = current
        otb_files = list((run / 'data').glob('*/items.otb'))
        if len(otb_files) != 1 or digest(otb_files[0].read_bytes()) != otb:
            raise ValueError('G4 imported client OTB differs from profile/index assets')
        audit['clientAssets'] = {'client': client, 'otbSha256': otb, 'datSha256': dat}
        keymap = (run / 'keymap.tsv').read_bytes()
        if keymap.decode('utf-8').splitlines() != ['g4-keymap/1', *keys]:
            raise ValueError('profile keymap differs from approved G4 raster mapping')
        audit['approvedKeyMapping'] = [dict(zip(('tag', 'class', 'rule', 'key'), row.split('\t'), strict=True)) for row in keys]
        if (receipt['result'], receipt['exit_code'], receipt['client_id'], imported['client'], imported['compatibility']) != ('PASS', 0, client, client, 'compatible'):
            raise ValueError('unverified input')
        if receipt['source_hashes_unchanged']['Tibia.dat'] != dat or imported['sha256'] != semantic['sha256'] or imported['sha256'] not in receipt['source_hashes_unchanged'].values():
            raise ValueError('reference identity mismatch')
        if semantic['schema'] != 'g3-semantic-report/1' or imported['schema'] != 'g3-import-report/1':
            raise ValueError('reference schema mismatch')
        total = dict.fromkeys(('windows', 'sparse', 'candidates', 'uniform'), 0)
        hashes, sources = set(), {}
        for floor in semantic['floors']:
            z, w, h = (floor[k] for k in ('z', 'width', 'height'))
            if z > 7 or w < 16 or h < 16:
                continue
            ground_path = run / 'analysis' / f'kind-map-z{z:02}.png'
            unknown_path = run / 'analysis/semantic' / f'floor-{z:02}.png'
            gw, gh, ground = indexed_png(ground_path)
            uw, uh, unknown = indexed_png(unknown_path)
            if (gw, gh) != (w, h) or (uw, uh) != (w, h) or max(ground) > 8:
                raise ValueError('semantic floor dimensions/classes mismatch')
            classes = bytes(c if c < 5 and u != 1 else 255 for c, u in zip(ground, unknown, strict=True))
            raster = f'i53-raster/1 {w} {h}\n'.encode() + classes
            path = output / f'{alias}-z{z:02}.raster'
            path.write_bytes(raster)
            result = subprocess.run([str(exe.resolve()), str(path)], capture_output=True, check=True)
            if result.stderr:
                raise ValueError(result.stderr.decode(errors='replace'))
            counts, windows = copy_index_fp(result.stdout)
            hashes |= windows
            for k in total:
                total[k] += counts[k]
            sources[str(z)] = {'groundSha256': digest(ground_path.read_bytes()), 'unknownMaskSha256': digest(unknown_path.read_bytes()),
                               'rasterSha256': digest(raster), 'cells': len(classes)}
        if not sources:
            raise ValueError('no semantic floors')
        semantic_sha = digest(json.dumps(sources, sort_keys=True).encode())
        fp = fingerprint(total, hashes)
        entries.append((alias, imported['sha256'], semantic_sha, digest(keymap), fp, hashes))
        audit['sources'][alias] = {'sourceSha256': imported['sha256'], 'semanticSha256': semantic_sha,
                                    'keymapSha256': digest(keymap), 'before': len(hashes), 'floors': sources}
    generic = entries[0][5] & entries[1][5]
    for entry in entries:
        audit['sources'][entry[0]]['after'] = len(entry[5] - generic)
        if len(entry[5] - generic) < 16:
            raise ValueError('insufficient reference after generic filter')
    audit['genericUnique'] = len(generic)
    client, otb, dat, _ = identity
    data = f'{INDEX_HEADER}\n{client} {otb} {dat}\n'.encode()
    for alias, source, sem, keymap, fp, _ in entries:
        data += f'{alias} {source} {sem} {keymap} {len(fp)}\n'.encode() + fp
    data += f'sha256 {digest(data)}\n'.encode()
    index_path = output / 'references.i53'
    index_path.write_bytes(data)
    audit['indexSha256'] = digest(data)
    (output / 'index-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    print(json.dumps({'index': str(index_path), 'sha256': digest(data), 'generic': len(generic),
                      'windows': {a: {k: v[k] for k in ('before', 'after')} for a, v in audit['sources'].items()}}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('exe', 'bundle', 'profiles', 'output'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    build(args.exe, args.bundle, args.profiles, args.output)
