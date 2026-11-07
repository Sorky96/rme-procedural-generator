"""G3-B: build a synthetic OTBM 3 fixture with item ids the client cannot resolve.

Usage: py g3_make_unknown_fixture.py SOURCE_OTBM OUTPUT_DIR
SOURCE_OTBM is the synthetic 400-tile grass fixture (g0-metadata.otbm). The output is a new
directory holding g3-unknown.otbm and g3-unknown-expected.json; nothing is overwritten.

Ids 60000..60003 are used as "absent from the client" ids. The probe proves that they are
unknown to the loaded client (they must show up in unknown_ids); this script does not.
"""
import importlib.util
import json
import struct
import sys
from pathlib import Path

spec = importlib.util.spec_from_file_location('container_fixture', Path(__file__).with_name('g0_container_fixture.py'))
cf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cf)

GRASS_ATTR = 9  # OTBM_ATTR_ITEM: inline item id in the tile body
MISSING_GROUND, MISSING_ITEM, MISSING_ITEM_B, MISSING_ONLY = 60000, 60001, 60002, 60003

source, output = (Path(a).resolve() for a in sys.argv[1:3])
if output.exists():
    raise SystemExit('Refusing to reuse an output directory')
magic, root = cf.parse(source.read_bytes())
map_node = next(n for n in root[1] if n[0] and n[0][0] == 2)
area = next(a for a in map_node[1] if a[0][0] == cf.TILE_AREA and struct.unpack_from('<HHB', a[0], 1) == (0, 0, 7) and len(a[1]) == 400)


def item(item_id):
    return (bytearray([cf.ITEM]) + struct.pack('<H', item_id), [])


def with_ground_id(tile, item_id):
    body = bytearray(tile[0])
    assert body[3] == GRASS_ATTR, 'expected an inline ground item'
    body[4:6] = struct.pack('<H', item_id)
    return (body, [])


tiles = list(area[1])
tiles[0] = with_ground_id(tiles[0], MISSING_GROUND)                       # unresolvable ground
tiles[1] = (bytearray(tiles[1][0]), [item(MISSING_ITEM)])                  # known ground + unknown stacked item
tiles[2] = (bytearray(tiles[2][0]), [item(MISSING_ITEM)])                  # same unknown id again
tiles[3] = with_ground_id(tiles[3], MISSING_ITEM_B)                        # unresolvable ground ...
tiles[3] = (tiles[3][0], [item(MISSING_ITEM)])                             # ... plus unknown stacked item
tiles[4] = (bytearray(tiles[4][0][:3]), [item(MISSING_ONLY)])              # tile without ground, only an unknown item
new_area = (bytearray(area[0]), tiles)
new_map = (bytearray(map_node[0]), [new_area])
new_root = (bytearray(root[0]), [new_map])

output.mkdir(parents=True)
(output / 'g3-unknown.otbm').write_bytes(magic + bytes(cf.emit(new_root)))
# Totals that are true regardless of how the loader files an invalid inline item.
(output / 'g3-unknown-expected.json').write_text(json.dumps({
    'tiles': 400,
    'floors': {'7': 400},
    'unknown_ids': {str(MISSING_GROUND): 1, str(MISSING_ITEM): 3, str(MISSING_ITEM_B): 1, str(MISSING_ONLY): 1},
    'unknown_total': 6,
    'tiles_with_unknown_trait': 5,
    'tiles_with_ground_trait_at_most': 399,
}, indent=2), encoding='utf-8')
print('created', output)
