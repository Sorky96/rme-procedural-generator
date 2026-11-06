from pathlib import Path
from struct import unpack_from
import sys
from xml.etree import ElementTree

if len(sys.argv) != 2 or not __debug__:
    raise SystemExit('Usage: python g0_verify_zone_fixture.py <fixture-directory> (without -O)')
directory = Path(sys.argv[1])
data = (directory / 'g0-otbm5-zone.otbm').read_bytes()
assert data[:4] in (b'OTBM', b'\0\0\0\0')
stack = []
roots = []
escaped = False
for byte in data[4:]:
    if escaped:
        assert stack
        stack[-1][0].append(byte)
        escaped = False
    elif byte == 0xfd:
        escaped = True
    elif byte == 0xfe:
        node = (bytearray(), [])
        (stack[-1][1] if stack else roots).append(node)
        stack.append(node)
    elif byte == 0xff:
        assert stack
        stack.pop()
    else:
        assert stack
        stack[-1][0].append(byte)
assert not escaped and not stack and len(roots) == 1
root = roots[0]
assert root[0][0] == 0 and unpack_from('<I', root[0], 1)[0] == 4
map_nodes = [n for n in root[1] if n[0][0] == 2]
assert len(map_nodes) == 1
properties = map_nodes[0][0]
offset = 1
zone_files = []
while offset < len(properties):
    tag = properties[offset]
    offset += 1
    assert tag in (1, 11, 13, 23, 24), f'unexpected map attribute {tag}'
    length = unpack_from('<H', properties, offset)[0]
    offset += 2
    assert offset + length <= len(properties)
    value = bytes(properties[offset:offset + length]).decode('utf-8')
    offset += length
    if tag == 24:
        zone_files.append(value)
assert zone_files == ['g0-otbm5-zone-1-zones.xml'], zone_files
zones = []
tile_count = 0
for area in map_nodes[0][1]:
    if area[0][0] != 4:
        continue
    x, y, z = unpack_from('<HHB', area[0], 1)
    for tile in area[1]:
        if tile[0][0] not in (5, 14):
            continue
        tile_count += 1
        position = (x + tile[0][1], y + tile[0][2], z)
        for child in tile[1]:
            if child[0][0] == 19:
                count = unpack_from('<H', child[0], 1)[0]
                assert len(child[0]) == 3 + 2 * count and not child[1]
                zones.append((position, list(unpack_from('<' + 'H' * count, child[0], 3))))
assert tile_count == 402
assert zones == [((104, 104, 7), [1])], zones
doc = ElementTree.parse(directory / 'g0-otbm5-zone-1-zones.xml')
assert [(n.get('name'), n.get('zoneid')) for n in doc.getroot()] == [('G0 Zone', '1')]
print('PASS: 402 tiles, zone 1 at (104,104,7), XML name and ID')
