import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import shutil
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'docs/procedural/smoke'))
from g0_compare_roundtrip import read_tiles, without_tiles, xml_value
from g0_container_fixture import POSITION, check_fixture, parse, emit


def canonical(path, *, expected_companions=None):
    header, tree = parse(path.read_bytes())
    tree = copy.deepcopy(tree)
    companions = {}
    for map_node in tree[1]:
        if map_node[0][0] != 2:
            continue
        body = map_node[0]
        normalized = bytearray([2])
        offset = 1
        while offset < len(body):
            tag = body[offset]
            offset += 1
            if tag not in (1, 11, 13):
                raise AssertionError(f'Unhandled fixture header attribute {tag}')
            length = struct.unpack_from('<H', body, offset)[0]
            offset += 2
            value = bytes(body[offset:offset + length])
            offset += length
            if tag in (11, 13):
                name = value.decode('utf-8')
                if expected_companions is not None:
                    relative = Path(name)
                    if (expected_companions.get(tag) != name or relative.is_absolute() or
                            '..' in relative.parts or '\\' in name or
                            not (path.parent / relative).resolve().is_relative_to(path.parent.resolve())):
                        raise AssertionError('Companion must match its expected confined relative path')
                elif Path(name).name != name or '/' in name or '\\' in name:
                    raise AssertionError('Snapshot companion must be a local basename')
                companions[tag] = xml_value(ET.parse(path.parent / name).getroot())
                value = b'#spawn' if tag == 11 else b'#house'
            normalized.extend(bytes([tag]) + struct.pack('<H', len(value)) + value)
        body[:] = normalized
    if companions.keys() != {11, 13}:
        raise AssertionError('Missing companion header attributes')
    return header, tree, companions


def compare(before_path, after_path):
    first_header, first_root, first_xml = canonical(before_path)
    second_header, second_root, second_xml = canonical(after_path)
    if first_header != second_header or without_tiles(first_root) != without_tiles(second_root):
        raise AssertionError('Non-tile map data changed beyond companion basenames')
    first, second = read_tiles(first_root), read_tiles(second_root)
    if first.keys() != second.keys() or len(first) != 402:
        raise AssertionError('Snapshot tile positions/count changed')
    for position in first:
        if position != POSITION and first[position] != second[position]:
            raise AssertionError(f'Tile changed: {position}')
    for tile in (first[POSITION], second[POSITION]):
        if tile[0] != first[POSITION][0] or len(tile[1]) != 1 or not check_fixture(tile[1][0]):
            raise AssertionError('Nested fixture changed')
    if first_xml != second_xml:
        raise AssertionError('Companion XML data changed')


def item_attributes(node):
    body, children = node
    if children or body[0] != 6:
        raise AssertionError('Expected a ground item node')
    attributes = {}
    offset = 3
    while offset < len(body):
        tag = body[offset]
        offset += 1
        if tag in (4, 5):
            value = struct.unpack_from('<H', body, offset)[0]
            offset += 2
        elif tag in (6, 7):
            size = struct.unpack_from('<H', body, offset)[0]
            offset += 2
            value = bytes(body[offset:offset + size]).decode('utf-8')
            offset += size
        else:
            raise AssertionError(f'Unhandled ground attribute {tag}')
        if tag in attributes:
            raise AssertionError('Duplicate ground attribute')
        attributes[tag] = value
    return bytes(body[:3]), attributes


def compare_save_as(before_path, after_path):
    first_header, first_root, first_xml = canonical(before_path)
    second_header, second_root, second_xml = canonical(after_path)
    points = [node for parent in second_root[1] for group in parent[1]
              if group[0][0] == 15 for node in group[1]]
    expected_point = b'\x10' + struct.pack('<H', 7) + b'G0 Both' + struct.pack('<HHB', 160, 160, 7)
    if len(points) != 1 or points[0] != (bytearray(expected_point), []):
        raise AssertionError('BOTH binary waypoint missing/changed')
    for tree in (first_root, second_root):
        for parent in tree[1]:
            parent[1][:] = [group for group in parent[1] if group[0][0] != 15]
    if first_header != second_header or without_tiles(first_root) != without_tiles(second_root):
        raise AssertionError('Save As changed unexpected non-tile data')
    first, second = read_tiles(first_root), read_tiles(second_root)
    changed_position = (160, 160, 7)
    if first.keys() != second.keys() or len(first) != 402:
        raise AssertionError('Save As positions/count changed')
    if any(first[p] != second[p] for p in first if p != changed_position):
        raise AssertionError('Save As changed another tile')
    old, new = first[changed_position], second[changed_position]
    if old[0] != new[0] or len(old[1]) != 1 or len(new[1]) != 1:
        raise AssertionError('Save As changed ground tile structure')
    old_id, old_attrs = item_attributes(old[1][0])
    new_id, new_attrs = item_attributes(new[1][0])
    expected_attrs = dict(old_attrs)
    expected_attrs.update({6: 'G0 Save As text', 7: 'G0 Save As description'})
    if old_id != new_id or new_attrs != expected_attrs or first_xml != second_xml:
        raise AssertionError('Save As ground attributes/companion data mismatch')
    xml = ET.parse(after_path.with_name(after_path.stem + '-waypoint.xml')).getroot()
    if xml.tag != 'waypoints' or len(xml) != 1 or xml[0].tag != 'waypoint' or xml[0].attrib != {
            'name': 'G0 Both', 'x': '160', 'y': '160', 'z': '7'}:
        raise AssertionError('BOTH waypoint XML mismatch')
    for filename, expected in [('copy-house.xml', b'KEEP_HOUSE'), ('copy-spawn.xml', b'KEEP_SPAWN'),
                               ('blocked-waypoint.xml', b'KEEP_WAYPOINT')]:
        if after_path.with_name(filename).read_bytes() != expected:
            raise AssertionError('Save As sentinel changed')
    if after_path.with_name('blocked.otbm').exists():
        raise AssertionError('Rejected Save As created OTBM')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('fixture', type=Path)
    parser.add_argument('log', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    lines = args.log.read_text(encoding='utf-8').splitlines()
    if 'SNAPSHOT_RUNTIME_PASS' not in lines or 'SAVE_AS_RUNTIME_PASS' not in lines or any(
            line.startswith('FAILED\t') for line in lines):
        raise AssertionError('Runtime assertions did not finish')
    snapshots = [Path(line.split('\t')[1]) for line in lines if line.startswith('SUCCESS\t')]
    if len(snapshots) != 4 or len(set(snapshots)) != 4:
        raise AssertionError('Expected four distinct successful snapshots')
    for snapshot in snapshots:
        compare(args.fixture, snapshot)
    save_as = [line.split('\t') for line in lines if line.startswith('SAVE_AS_BOTH_PASS\t')]
    if len(save_as) != 2 or {fields[1] for fields in save_as} != {'absolute', 'relative'}:
        raise AssertionError('Save As cases incomplete')
    for fields in save_as:
        compare_save_as(args.fixture, Path(fields[2]))
    repeated_names = {p.name for p in snapshots if sum(q.name == p.name for q in snapshots) > 1}
    if not repeated_names:
        raise AssertionError('No same-second filename collision was observed; collision test incomplete')
    failures = [line.split('\t') for line in lines if line.startswith('WRITE_ATTEMPT\t')]
    observed = [fields for fields in failures if fields[2:4] == ['0', '1']]
    if not observed:
        raise AssertionError('No observed serialization failure under a file lock')
    for fields in observed:
        path = Path(fields[4])
        if not path.is_file() or not fields[5]:
            raise AssertionError('Partial snapshot or failure description missing')
    negatives = 0
    for case in ('house-rent', 'map-width'):
        with tempfile.TemporaryDirectory(prefix='rme-g0-snapshot-oracle-') as temporary:
            copied = Path(temporary) / 'copy'
            shutil.copytree(snapshots[0].parent, copied)
            changed = copied / snapshots[0].name
            if case == 'house-rent':
                house = next(copied.glob('*-house.xml'))
                xml = ET.parse(house)
                element = xml.getroot().find('house')
                if element is None:
                    raise AssertionError('House fixture missing')
                element.set('rent', '4322')
                xml.write(house)
            else:
                header, tree = parse(changed.read_bytes())
                tree[0][5] ^= 1  # Width byte, independent of companion name normalization.
                changed.write_bytes(header + emit(tree))
            try:
                compare(args.fixture, changed)
            except AssertionError:
                negatives += 1
            else:
                raise AssertionError(f'Oracle accepted changed {case}')
    for case in ('waypoint-xml', 'waypoint-binary'):
        original_copy = Path(save_as[0][2])
        with tempfile.TemporaryDirectory(prefix='rme-g0-saveas-oracle-') as temporary:
            copied = Path(temporary) / 'copy'
            shutil.copytree(original_copy.parent, copied)
            changed = copied / original_copy.name
            if case == 'waypoint-xml':
                waypoint = changed.with_name(changed.stem + '-waypoint.xml')
                xml = ET.parse(waypoint)
                xml.getroot()[0].set('x', '161')
                xml.write(waypoint)
            else:
                header, tree = parse(changed.read_bytes())
                points = [node for parent in tree[1] for group in parent[1]
                          if group[0][0] == 15 for node in group[1]]
                points[0][0][3] ^= 1
                changed.write_bytes(header + emit(tree))
            try:
                compare_save_as(args.fixture, changed)
            except AssertionError:
                negatives += 1
            else:
                raise AssertionError(f'Oracle accepted changed {case}')
    hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in snapshots}
    output = json.dumps({'result': 'PASS', 'snapshots': hashes,
                      'scope': '402 positions, 401 exact tile nodes, nested container semantics, '
                               'other OTBM data except companion basenames, both XML trees',
                      'locked_write_failures': len(observed),
                      'same_second_names': sorted(repeated_names),
                      'save_as_both_cases': len(save_as),
                      'oracle_negative_controls': negatives}, indent=2)
    if args.report:
        args.report.write_text(output, encoding='utf-8')
    print(output)


if __name__ == '__main__':
    main()
