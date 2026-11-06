"""Exercise the fixture comparator on disposable OTBM and XML copies."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from g0_container_fixture import POSITION, emit, parse
from g0_compare_roundtrip import read_tiles


class ComparatorTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.path = self.folder / 'after.otbm'
        self.path.write_bytes(AFTER.read_bytes())
        for suffix in ('house', 'spawn'):
            name = f'house-roundtrip-{suffix}.xml'
            (self.folder / name).write_bytes((XML_DIR / name).read_bytes())

    def check(self, succeeds):
        command = [sys.executable, str(Path(__file__).with_name('g0_compare_roundtrip.py')),
                   str(BEFORE), str(self.path), str(XML_DIR), str(self.folder)]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)

    def mutate_map(self, mutation):
        header, root = parse(self.path.read_bytes())
        mutation(root)
        self.path.write_bytes(header + emit(root))

    def test_unchanged(self):
        self.check(True)

    def test_changed_non_tile_metadata(self):
        self.mutate_map(lambda root: root[0].__setitem__(-1, root[0][-1] ^ 1))
        self.check(False)

    def test_changed_other_tile(self):
        def change(root):
            tiles = read_tiles(root)
            tile = next(tile for pos, tile in tiles.items() if pos != POSITION)
            tile[0][-1] ^= 1
        self.mutate_map(change)
        self.check(False)

    def test_changed_container(self):
        def change(root):
            coin = read_tiles(root)[POSITION][1][0][1][0][1][0][0]
            coin[-1] ^= 1
        self.mutate_map(change)
        self.check(False)

    def test_changed_house_xml(self):
        path = self.folder / 'house-roundtrip-house.xml'
        path.write_text('<houses><house name="changed" /></houses>', encoding='utf-8')
        self.check(False)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        raise SystemExit('Expected BEFORE.otbm BEFORE_XML_DIR AFTER.otbm')
    BEFORE, XML_DIR, AFTER = map(Path, sys.argv[1:])
    del sys.argv[1:]
    unittest.main()
