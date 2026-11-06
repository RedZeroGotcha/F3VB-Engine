import struct
import tempfile
import unittest
from pathlib import Path

from f3vb.chunk import ChunkError, iter_chunks, plausible_chunk_at, read_chunk
from f3vb.inventory import inventory
from f3vb.probe import probe_file


def mk(tag: bytes, version: int, payload: bytes) -> bytes:
    return struct.pack("<4sII", tag, version, 12 + len(payload)) + payload


class ChunkTests(unittest.TestCase):
    def test_iter_two_chunks(self):
        buf = mk(b"EMAP", 3, b"abcd") + mk(b"EME2", 1, b"")
        chunks = list(iter_chunks(buf))
        self.assertEqual([c.tag for c in chunks], ["EMAP", "EME2"])
        self.assertEqual(chunks[0].payload_size, 4)
        self.assertEqual(chunks[1].offset, 16)

    def test_size_too_small(self):
        bad = struct.pack("<4sII", b"EMAP", 1, 4)
        with self.assertRaises(ChunkError):
            read_chunk(bad)

    def test_overflow(self):
        bad = struct.pack("<4sII", b"EMAP", 1, 999)
        with self.assertRaises(ChunkError):
            read_chunk(bad)

    def test_trailing_bytes(self):
        with self.assertRaises(ChunkError):
            list(iter_chunks(mk(b"EMAP", 1, b"") + b"xx"))

    def test_plausible(self):
        buf = b"\x00\x00" + mk(b"GCRE", 2, b"zz")
        self.assertTrue(plausible_chunk_at(buf, 2, b"GCRE"))
        self.assertFalse(plausible_chunk_at(buf, 0, b"GCRE"))


class InventoryProbeTests(unittest.TestCase):
    def test_inventory_and_probe(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / "Override").mkdir()
            (root / "F3.exe").write_bytes(b"MZ")
            (root / "x.grp").write_bytes(
                b"\x00" * 32 + b"B3D " + b"\x00" * 8 + mk(b"EMAP", 1, b"hi") + b"8TRE")
            inv = inventory(root, with_hash=True)
            self.assertEqual(inv["total_files"], 2)
            self.assertEqual(len(inv["grp_files"]), 1)
            self.assertIsNotNone(inv["notable"]["f3.exe"])
            r = probe_file(root / "x.grp")
            self.assertEqual(r["magics"]["b3d (modelo)"]["count"], 1)
            self.assertEqual(r["magics"]["8tre (geometria de mapa)"]["count"], 1)
            self.assertIn("EMAP", r["chunks"])

    def test_empty_file(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "e.grp"
            p.write_bytes(b"")
            self.assertEqual(probe_file(p)["size"], 0)


if __name__ == "__main__":
    unittest.main()
