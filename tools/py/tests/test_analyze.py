import json
import struct
import tempfile
import unittest
from pathlib import Path

from f3vb.analyze import format_resumo
from f3vb.classify import classify
from f3vb.cli import main as cli_main
from f3vb.probes import probe_amx
from tests.test_extract import make_b3d, make_gr2, make_tga
from tests.test_readers import build_grp, build_rht


def make_amx(defsize=8):
    names = b"main\0print\0random\0"
    hdr_size = 56
    publics = hdr_size
    natives = publics + defsize * 1
    libraries = natives + defsize * 2
    nametable = libraries
    base = nametable
    ofs = {"main": base, "print": base + 5, "random": base + 11}
    body = struct.pack("<II", 0, ofs["main"]) + struct.pack("<II", 0, ofs["print"]) + struct.pack("<II", 0, ofs["random"])
    total = hdr_size + len(body) + len(names)
    head = struct.pack("<iHbbhh", total, 0xF1E2, 8, 8, 0, defsize)
    head += struct.pack("<11i", 0, 0, 0, 0, 0, publics, natives, libraries, libraries, libraries, nametable)
    return head + body + names


class AmxTests(unittest.TestCase):
    def test_probe(self):
        d = make_amx()
        self.assertTrue(classify(d[:64], len(d)).startswith("amx?"))
        p = probe_amx(d)
        self.assertEqual(p["publics"], ["main"])
        self.assertEqual(p["natives"], ["print", "random"])

    def test_head_only_needs_total_size(self):
        d = make_amx()
        self.assertFalse(classify(d[:64]).startswith("amx?"))        # sem total_size, o tamanho declarado não confere
        self.assertTrue(classify(d[:64], len(d)).startswith("amx?"))  # como o CLI usa: início + tamanho real
        ch = struct.pack("<4sII", b"EMAP", 1, 500) + b"\0" * 488
        self.assertEqual(classify(ch[:64], len(ch)), "chunk:EMAP")

    def test_invalid(self):
        self.assertIn("error", probe_amx(b"\x00" * 100))
        self.assertIn("error", probe_amx(b"\x00" * 10))


class ReportTests(unittest.TestCase):
    def test_report_and_resumo(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game = t / "demo"
            game.mkdir()
            (game / "F3.exe").write_bytes(b"MZ")
            (game / "a.grp").write_bytes(build_grp([make_gr2(), make_tga(), make_b3d(), make_amx(), b"x" * 50]))
            (game / "resource.rht").write_bytes(build_rht([(1, 1, 100, 0, 0), (1, 2, 700, 1, 92), (1, 3, 5555, 2, 6)]))
            out, res = t / "r.json", t / "resumo.txt"
            cli_main(["report", str(game), "-o", str(out), "--resumo", str(res)])
            rep = json.loads(out.read_text(encoding="utf-8"))
            agg = rep["grps"][0]["aggregates"]
            self.assertEqual(list(agg["gr2"]["combinations"].values()), [1])
            self.assertEqual(agg["b3d"]["count"], 1)
            self.assertEqual(agg["b3d"]["texture_files"], ["body.tga"])
            self.assertEqual(agg["tga"]["plausible"], 1)
            self.assertEqual(agg["amx"]["publics"], ["main"])
            self.assertEqual(rep["rht"]["unknown_type_ids"], [5555])
            text = res.read_text(encoding="utf-8")
            self.assertIn("GR2 combinações", text)
            self.assertIn("publics", text)
            self.assertIn("tipos fora da tabela: [5555]", text)
            self.assertEqual(format_resumo(rep), text)


if __name__ == "__main__":
    unittest.main()
