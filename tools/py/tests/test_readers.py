import struct
import tempfile
import unittest
from pathlib import Path

from f3vb.classify import classify
from f3vb.grp import GrpError, GrpFile, parse_grp
from f3vb.rht import RhtError, parse_rht


def build_grp(blobs, v=(1, 0)):
    table_end = 12 + 8 * len(blobs)
    head = struct.pack("<III", v[0], v[1], len(blobs))
    table, data, off = b"", b"", table_end
    for b in blobs:
        table += struct.pack("<II", off, len(b))
        data += b
        off += len(b)
    return head + table + data


def build_rht(entries, packs=b"psf\x00int\x00", v=(1, 0)):
    n = len(entries)
    off_packs = 20 + 20 * n
    off_res = off_packs + len(packs)
    out = struct.pack("<5I", v[0], v[1], n, off_packs, off_res)
    for e in entries:
        out += struct.pack("<5I", *e)
    return out + packs + b"\xaa" * 8


class GrpTests(unittest.TestCase):
    def test_roundtrip(self):
        blobs = [b"B3D \x00\x00", b"hello", b""]
        info = parse_grp(build_grp(blobs, v=(2, 5)))
        self.assertEqual((info.v_major, info.v_minor, info.n_files), (2, 5, 3))
        self.assertEqual([lp.length for lp in info.lumps], [6, 5, 0])
        self.assertEqual(info.issues, [])

    def test_table_too_big(self):
        with self.assertRaises(GrpError):
            parse_grp(struct.pack("<III", 1, 0, 1_000_000))

    def test_out_of_range_lump(self):
        buf = bytearray(build_grp([b"abcd"]))
        struct.pack_into("<II", buf, 12, 20, 9999)
        info = parse_grp(bytes(buf))
        self.assertTrue(any("fora do arquivo" in i for i in info.issues))

    def test_overlap_and_table_overlap(self):
        buf = bytearray(build_grp([b"abcdefgh", b"ijkl"]))
        struct.pack_into("<II", buf, 12, 20, 8)
        struct.pack_into("<II", buf, 20, 24, 4)   # sobrepõe o primeiro
        self.assertTrue(any("sobrepõem" in i for i in parse_grp(bytes(buf)).issues))
        struct.pack_into("<II", buf, 12, 4, 4)    # dentro da tabela
        self.assertTrue(any("dentro da tabela" in i for i in parse_grp(bytes(buf)).issues))

    def test_too_small(self):
        with self.assertRaises(GrpError):
            parse_grp(b"\x00" * 5)

    def test_file_read(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "a.grp"
            p.write_bytes(build_grp([b"xx", b"8TRE12"]))
            with GrpFile(p) as g:
                self.assertEqual(g.read(1), b"8TRE12")
                self.assertEqual(g.head(1, 4), b"8TRE")
            (Path(d) / "e.grp").write_bytes(b"")
            with self.assertRaises(GrpError):
                GrpFile(Path(d) / "e.grp")


class RhtTests(unittest.TestCase):
    def test_parse_and_summary(self):
        ents = [(1, 11, 100, 0, 0), (1, 12, 700, 1, 92), (1, 13, 9999, 2, 0),
                (1, 14, 1200, 3, 0xFFFFFFFF)]
        info = parse_rht(build_rht(ents))
        s = info.summary()
        self.assertEqual(s["entries"], 4)
        self.assertEqual(s["by_type"]["G3D_Model"]["count"], 1)
        self.assertEqual(s["by_type"]["GR2_Animation"]["count"], 1)
        self.assertEqual(s["unknown_type_ids"], [9999])
        self.assertEqual(s["by_pack"]["INVALID"], 1)
        self.assertEqual(s["by_pack"]["psf"], 2)             # pack=0 -> primeiro nome da região de packs
        self.assertEqual(s["unknown_pack_values"], [92])     # 92 não aponta para uma string
        self.assertEqual(s["number_unique"], 4)
        self.assertEqual(info.pack_strings(), [(0, "psf"), (4, "int")])
        self.assertEqual(info.entries[1].ext, "gr2")

    def test_errors(self):
        with self.assertRaises(RhtError):
            parse_rht(b"\x00" * 4)
        with self.assertRaises(RhtError):
            parse_rht(struct.pack("<5I", 1, 0, 1000, 0, 0))

    def test_offsets_outside(self):
        buf = struct.pack("<5I", 1, 0, 0, 5000, 6000)
        info = parse_rht(buf)
        self.assertEqual(len(info.issues), 2)


class ClassifyTests(unittest.TestCase):
    def test_labels(self):
        self.assertEqual(classify(b"B3D 1.1xxxxxxxxxx"), "b3d")
        self.assertEqual(classify(b"8TRE" + b"\x00" * 20), "8tre")
        self.assertEqual(classify(b"RIFF\x00\x00\x00\x00WAVEfmt "), "wav")
        self.assertEqual(classify(b""), "vazio")
        self.assertEqual(classify(b"Hello world, texto simples\n"), "texto?")
        ch = struct.pack("<4sII", b"EMAP", 1, 16) + b"abcd"
        self.assertEqual(classify(ch), "chunk:EMAP")
        amx = struct.pack("<iH", 64, 0xF1E2) + b"\x00" * 58
        self.assertTrue(classify(amx).startswith("amx?"))
        self.assertEqual(classify(bytes(range(200, 256)) * 3 + b"\x00\x01"), "bin")


if __name__ == "__main__":
    unittest.main()
