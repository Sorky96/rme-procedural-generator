"""I53 semantic-raster decoder and provenance contract, synthetic inputs only."""
import importlib.util
import struct
import sys
import unittest
import zlib
from pathlib import Path

spec = importlib.util.spec_from_file_location('builder', Path(__file__).resolve().parents[1] / 'docs/procedural/smoke/g6_build_copy_index.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
scratch = Path(sys.argv.pop()).resolve() if len(sys.argv) == 2 else Path(__file__).resolve().parents[1] / 'scratchpad/i53-python'
scratch.mkdir(parents=True, exist_ok=True)


def png(w, h, raw):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0)) + chunk(b'PLTE', bytes(27)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')


class Decoder(unittest.TestCase):
    def test_roundtrip_and_corruption(self):
        path = scratch / 'synthetic.png'
        data = png(3, 2, bytes([0, 0, 4, 8, 0, 1, 5, 6]))
        path.write_bytes(data)
        self.assertEqual(builder.indexed_png(path), (3, 2, bytearray([0, 4, 8, 1, 5, 6])))
        for bad in (data[:-1], data + b'trailing', data[:50] + b'X' + data[51:], png(3, 2, bytes(7)), png(3, 2, bytes([1, 0, 4, 8, 0, 1, 5, 6]))):
            path.write_bytes(bad)
            with self.assertRaises((ValueError, struct.error)):
                builder.indexed_png(path)

    def test_holdout_profile_rejected(self):
        path = scratch / 'synthetic.profile'
        path.write_text('g4-profile/6\nsource\tM01\t1000\n')
        with self.assertRaises(ValueError):
            builder.profile(path, 'M06')

    def test_fingerprint_unique_and_options(self):
        valid = b'g4-fingerprint/1 window=16 modulo=4 minpresent=50\ncounts windows=2 sparse=0 candidates=2 uniform=0 kept=2\n0000000000000001\n0000000000000002\n'
        counts, hashes = builder.parse_fp(valid)
        self.assertEqual(builder.fingerprint(counts, hashes), valid)
        for bad in (valid.replace(b'modulo=4', b'modulo=1'), valid.replace(b'0000000000000002', b'0000000000000001')):
            with self.assertRaises(ValueError):
                builder.parse_fp(bad)
        # D10: copy_index output must declare square-symmetry fingerprints; older tools are refused.
        self.assertEqual(builder.copy_index_fp(b'i53-fingerprint/2 d4-canonical-v1\n' + valid), (counts, hashes))
        with self.assertRaises(ValueError):
            builder.copy_index_fp(valid)


if __name__ == '__main__':
    unittest.main()
