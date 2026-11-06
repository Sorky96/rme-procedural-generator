"""Compare fixture tiles and companion XML after an RME round-trip.

Usage: python g0_compare_roundtrip.py BEFORE.otbm AFTER.otbm BEFORE_DIR AFTER_DIR
Both directories must contain house-roundtrip-house.xml and
house-roundtrip-spawn.xml. Non-tile OTBM nodes are compared byte for byte.
"""
import argparse
import struct
import xml.etree.ElementTree as ET
from pathlib import Path

from g0_container_fixture import POSITION, check_fixture, parse


def read_tiles(root):
    tiles = {}
    for map_node in root[1]:
        if not map_node[0] or map_node[0][0] != 2:
            continue
        for area in map_node[1]:
            if not area[0] or area[0][0] != 4:
                continue
            x, y, z = struct.unpack_from('<HHB', area[0], 1)
            for tile in area[1]:
                if len(tile[0]) < 3 or tile[0][0] not in (5, 14):
                    raise ValueError('Unexpected tile node')
                position = (x + tile[0][1], y + tile[0][2], z)
                if position in tiles:
                    raise ValueError(f'Duplicate tile {position}')
                tiles[position] = tile
    return tiles


def without_tiles(node, depth=0):
    body, children = node
    if depth == 2 and body and body[0] == 4:
        return (bytes(body), [])
    return (bytes(body), [without_tiles(child, depth + 1) for child in children])


def xml_value(element):
    return (element.tag, dict(element.attrib), (element.text or '').strip(),
            [xml_value(child) for child in element])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'after', 'before_dir', 'after_dir'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    before_header, before_root = parse(args.before.read_bytes())
    after_header, after_root = parse(args.after.read_bytes())
    if before_header != after_header or without_tiles(before_root) != without_tiles(after_root):
        raise AssertionError('Non-tile OTBM data differs')
    before, after = read_tiles(before_root), read_tiles(after_root)
    if before.keys() != after.keys():
        raise AssertionError('Tile positions differ')
    changed = [p for p in before if p != POSITION and before[p] != after[p]]
    if changed:
        raise AssertionError(f'Other tile nodes differ: {changed[:10]}')
    first, second = before[POSITION], after[POSITION]
    if first[0] != second[0] or any(len(tile[1]) != 1 or not check_fixture(tile[1][0])
                                   for tile in (first, second)):
        raise AssertionError('Target tile or nested fixture differs')
    for suffix in ('house', 'spawn'):
        filename = f'house-roundtrip-{suffix}.xml'
        left = xml_value(ET.parse(args.before_dir / filename).getroot())
        right = xml_value(ET.parse(args.after_dir / filename).getroot())
        if left != right:
            raise AssertionError(f'{suffix} XML trees differ')
    print(f'PASS: {len(before)} tile positions; {len(before) - 1} unchanged tile nodes; '
          'non-tile OTBM data, nested fixture, house XML and spawn XML preserved')


if __name__ == '__main__':
    main()
