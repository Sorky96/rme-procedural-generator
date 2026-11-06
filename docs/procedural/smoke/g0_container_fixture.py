"""Create or inspect an isolated OTBM 3 nested-container round-trip fixture.

Usage: python g0_container_fixture.py create input.otbm new-output.otbm
       python g0_container_fixture.py verify output.otbm
The input and output paths must be distinct; creation never overwrites a file.
"""
import argparse
import struct
from pathlib import Path

START, END, ESCAPE = 0xFE, 0xFF, 0xFD
TILE_AREA, TILE, ITEM = 4, 5, 6
POSITION = (100, 100, 7)


def parse(data, *, otbm_version=3):
    if data[:4] not in (b"OTBM", b"\0\0\0\0"):
        raise ValueError("Expected an uncompressed OTBM file")
    stack, roots = [], []
    escaped = False
    for byte in data[4:]:
        if escaped:
            if not stack:
                raise ValueError("Escaped byte outside node")
            stack[-1][0].append(byte)
            escaped = False
        elif byte == ESCAPE:
            escaped = True
        elif byte == START:
            node = (bytearray(), [])
            (stack[-1][1] if stack else roots).append(node)
            stack.append(node)
        elif byte == END:
            if not stack:
                raise ValueError("Unbalanced node")
            stack.pop()
        elif stack:
            stack[-1][0].append(byte)
        else:
            raise ValueError("Byte outside node")
    if stack or escaped or len(roots) != 1:
        raise ValueError("Incomplete OTBM tree")
    # The stored version is zero-based.
    if struct.unpack_from("<I", roots[0][0], 1)[0] != otbm_version - 1:
        raise ValueError(f"Fixture requires OTBM {otbm_version}")
    return data[:4], roots[0]


def emit(node):
    body, children = node
    result = bytearray([START])
    for byte in body:
        if byte in (START, END, ESCAPE):
            result.append(ESCAPE)
        result.append(byte)
    for child in children:
        result.extend(emit(child))
    result.append(END)
    return result


def target(root):
    matches = []
    for map_node in root[1]:
        if not map_node[0] or map_node[0][0] != 2:
            continue
        for area in map_node[1]:
            if not area[0] or area[0][0] != TILE_AREA:
                continue
            x, y, z = struct.unpack_from("<HHB", area[0], 1)
            for tile in area[1]:
                if len(tile[0]) >= 3 and tile[0][0] == TILE:
                    if (x + tile[0][1], y + tile[0][2], z) == POSITION:
                        matches.append(tile)
    if len(matches) != 1:
        raise ValueError(f"Expected one target tile, got {len(matches)}")
    return matches[0]


def fixture_node():
    # DAT 10.98 identifies 1987 as bag and 2148 as stackable gold coin.
    # OTBM node 6: item ID (u16); attributes 4=action ID (u16), 15=count (u8).
    coin = (bytearray(struct.pack("<BHBHB", ITEM, 2148, 4, 12042, 15) + b"\x07"), [])
    nested = (bytearray(struct.pack("<BH", ITEM, 1987)), [coin])
    return (bytearray(struct.pack("<BH", ITEM, 1987)), [nested])


def check_fixture(node):
    body, children = node
    if body != bytearray(struct.pack("<BH", ITEM, 1987)) or len(children) != 1:
        return False
    nested_body, nested_children = children[0]
    if nested_body != bytearray(struct.pack("<BH", ITEM, 1987)) or len(nested_children) != 1:
        return False
    coin, coin_children = nested_children[0]
    if coin_children or coin[:3] != bytearray(struct.pack("<BH", ITEM, 2148)):
        return False
    # Attribute order is not significant: RME writes count before action ID.
    attributes, offset = {}, 3
    while offset < len(coin):
        tag = coin[offset]
        offset += 1
        size = {4: 2, 15: 1}.get(tag)
        if size is None or tag in attributes or offset + size > len(coin):
            return False
        attributes[tag] = int.from_bytes(coin[offset:offset + size], "little")
        offset += size
    return attributes == {4: 12042, 15: 7}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("create", "verify"))
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path, nargs="?")
    args = parser.parse_args()
    header, root = parse(args.input.read_bytes())
    tile = target(root)
    if args.mode == "create":
        if args.output is None or args.input.resolve() == args.output.resolve():
            parser.error("create requires a distinct output path")
        if tile[1]:
            raise ValueError("Target tile already contains child nodes")
        tile[1].append(fixture_node())
        with args.output.open("xb") as out:
            out.write(header + emit(root))
        print(f"Created isolated fixture: {args.output}")
    else:
        if args.output is not None:
            parser.error("verify takes one input path")
        matches = [node for node in tile[1] if check_fixture(node)]
        if len(matches) != 1:
            raise AssertionError(f"Expected nested bag and gold coin, found {len(matches)} matches")
        print("NESTED CONTAINER OTBM PASS: bag / bag / 7 gold, AID 12042")


if __name__ == "__main__":
    main()
