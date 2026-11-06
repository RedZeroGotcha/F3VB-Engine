"""v19: import-loose (fontes e English.stf soltos na pasta da demo), com arquivos FALSOS gerados no teste."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from f3vb.loose import import_loose

HERE = Path(__file__).resolve().parent.parent


class ImportLooseTests(unittest.TestCase):
    def _demo(self, root: Path) -> Path:
        demo = root / "Demo"
        (demo / "fonts").mkdir(parents=True)                       # minusculo de proposito
        (demo / "fonts" / "FAKE_A.TTF").write_bytes(b"\x00\x01\x00\x00fake-a")
        (demo / "fonts" / "fake_b.ttf").write_bytes(b"\x00\x01\x00\x00fake-b")
        (demo / "fonts" / "fontlist.ini").write_text("[fonts]\n")
        (demo / "fonts" / "leia.txt").write_text("nao copiar")
        (demo / "english.STF").write_bytes(b"stf-falso")
        return demo

    def test_copy_and_idempotent(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            demo = self._demo(root)
            assets = root / "original_assets"
            r = import_loose(demo, assets)
            self.assertEqual(r["fonts"], 2)
            self.assertEqual(r["missing"], [])
            self.assertTrue((assets / "RETAIL" / "Fonts" / "FAKE_A.TTF").is_file())
            self.assertTrue((assets / "RETAIL" / "Fonts" / "fontlist.ini").is_file())
            self.assertFalse((assets / "RETAIL" / "Fonts" / "leia.txt").exists())
            self.assertEqual((assets / "RETAIL" / "English.stf").read_bytes(), b"stf-falso")
            self.assertEqual((assets / "CONVERTED" / "scripts" / "English.stf").read_bytes(), b"stf-falso")
            self.assertEqual(len(r["copied"]), 5)
            r2 = import_loose(demo, assets)
            self.assertEqual(r2["copied"], [])
            self.assertEqual(len(r2["skipped"]), 5)
            r3 = import_loose(demo, assets, overwrite=True)
            self.assertEqual(len(r3["copied"]), 5)

    def test_missing(self):
        with tempfile.TemporaryDirectory() as t:
            demo = Path(t) / "vazia"
            demo.mkdir()
            r = import_loose(demo, Path(t) / "oa")
            self.assertEqual(r["fonts"], 0)
            self.assertEqual(len(r["missing"]), 2)
            with self.assertRaises(FileNotFoundError):
                import_loose(Path(t) / "nao_existe", Path(t) / "oa")

    def test_cli(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t)
            demo = self._demo(root)
            p = subprocess.run([sys.executable, "-m", "f3vb", "import-loose", str(demo), "--out", str(root / "oa")], cwd=HERE, capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
            self.assertIn("2 fonte(s) TrueType", p.stdout)


if __name__ == "__main__":
    unittest.main()
