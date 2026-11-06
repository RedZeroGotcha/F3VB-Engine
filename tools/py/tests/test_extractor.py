import csv
import tempfile
import time
import unittest
from pathlib import Path

from f3vb.extractor import ExtractJob, ExtractOptions, convert_retail, extract_all, fmt_bytes, plan, safe_name
from f3vb.layout import ASSETS_DIRNAME, CONVERTER_VERSION, AssetsLayout, default_assets_dir
from tests.test_catalog import build_rht_real
from tests.test_extract import make_b3d, make_tga
from tests.test_analyze import make_amx
from tests.test_readers import build_grp


def make_game(t: Path):
    game = t / "demo" / "data"
    game.mkdir(parents=True)
    blobs = [make_b3d(), make_tga(), make_tga(8, 8, 32), b"hello" * 20]
    (game / "Critters.grp").write_bytes(build_grp(blobs))
    (game / "Scripts.grp").write_bytes(build_grp([make_amx()]))
    # nomes com caracteres problemáticos e duplicados
    raw = build_rht_real([(0, 100, 0, "CR_Teste"), (1, 200, 0, "weaptex"), (2, 200, 0, "weaptex"), (3, 800, 0, "a<b>:c"),
                          (0, 1200, 9, "00_MainMenu")], ["Critters", "Scripts"], None)
    (t / "demo" / "resource.rht").write_bytes(raw)
    return t / "demo", blobs


class ExtractorTests(unittest.TestCase):
    def test_safe_name(self):
        self.assertEqual(safe_name('a<b>:c'), "a_b__c")
        self.assertEqual(safe_name("CON"), "_CON")
        self.assertEqual(safe_name("  .. "), "sem_nome")
        self.assertEqual(safe_name("Male_Outfits{23}MusHermit"), "Male_Outfits{23}MusHermit")
        self.assertEqual(fmt_bytes(1536), "1.5 KB")

    def test_extract_real_size_and_index(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game, blobs = make_game(t)
            p = plan(game)
            self.assertEqual(p["files"], 5)
            out = t / "out"
            log = extract_all(game, out, ExtractOptions(raw=True))
            self.assertEqual(log["written"], 5)
            self.assertEqual(log["failures"], [])
            files = sorted(f for f in (out / "RETAIL").rglob("*") if f.is_file())
            self.assertEqual(len(files), 5)
            sizes = sorted(f.stat().st_size for f in files)
            self.assertEqual(sizes, sorted([len(b) for b in blobs] + [len(make_amx())]))
            r = out / "RETAIL"
            self.assertTrue((r / "Critters" / "G3D_Model" / "CR_Teste.b3d").exists())
            self.assertTrue((r / "Critters" / "TGA_Image" / "weaptex.tga").exists())
            self.assertTrue((r / "Critters" / "TGA_Image" / "weaptex_2.tga").exists())     # duplicado
            self.assertTrue((r / "Scripts" / "AMX" / "00_MainMenu.amx").exists())
            self.assertEqual((r / "Critters" / "G3D_Model" / "CR_Teste.b3d").read_bytes(), blobs[0])
            self.assertTrue((out / "CONVERTED").is_dir())
            self.assertEqual((out / ".gitignore").read_text().strip(), "*")
            self.assertTrue((out / "LEIA-ME.txt").exists())
            with open(out / "index.csv", encoding="utf-8") as fh:
                rows = list(csv.DictReader(fh))
            self.assertEqual(len(rows), 5)
            self.assertIn("EXTRACAO_RELATORIO", "".join(p.name for p in out.iterdir()).upper())

    def test_conversions_from_retail(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game, _ = make_game(t)
            out = t / "out"
            log = extract_all(game, out, ExtractOptions(png=True, glb=True, f3m=True))
            self.assertEqual(log["written"], 5)
            self.assertEqual(log["png"], 2)
            self.assertEqual(log["glb"], 1)
            self.assertEqual(log["f3m_models"], 1)
            self.assertTrue((out / "CONVERTED" / "glb" / "Critters" / "CR_Teste.glb").exists())
            self.assertTrue((out / "CONVERTED" / "f3m" / "models" / "CR_Teste.f3m").exists())
            self.assertTrue((out / "CONVERTED" / "png" / "Critters" / "weaptex.png").exists())
            self.assertEqual(AssetsLayout(out).converter_version(), CONVERTER_VERSION)

    def test_convert_retail_without_game(self):
        """CONVERTED é refeita só com RETAIL + index.csv: apagamos o jogo e a pasta CONVERTED e reconvertemos."""
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game, _ = make_game(t)
            out = t / "out"
            extract_all(game, out, ExtractOptions())
            import shutil
            shutil.rmtree(game)
            self.assertFalse(game.exists())
            log = convert_retail(out, ExtractOptions(png=True, f3m=True))
            self.assertEqual(log["png"], 2)
            self.assertEqual(log["f3m_models"], 1)
            self.assertEqual(log["written"], 0)
            self.assertTrue((out / "CONVERTED" / "f3m" / "models" / "CR_Teste.f3m").exists())
            # --no-raw em extract_all leva ao mesmo caminho
            shutil.rmtree(out / "CONVERTED")
            log = extract_all(game, out, ExtractOptions(raw=False, png=True))
            self.assertEqual(log["png"], 2)

    def test_convert_retail_needs_index(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(FileNotFoundError):
                convert_retail(Path(t) / "vazio", ExtractOptions(png=True))

    def test_stale_converted_detection(self):
        with tempfile.TemporaryDirectory() as t:
            lay = AssetsLayout(Path(t) / "oa").ensure()
            self.assertFalse(lay.converted_is_stale())                 # vazio
            (lay.converted / "f3m" / "models").mkdir(parents=True)
            (lay.converted / "f3m" / "models" / "x.f3m").write_bytes(b"F3M")
            self.assertTrue(lay.converted_is_stale())                  # como no v11: sem registro
            lay.write_converter_info()
            self.assertFalse(lay.converted_is_stale())

    def test_default_assets_dir(self):
        import os
        old = os.environ.pop("F3VB_ASSETS_DIR", None)
        try:
            self.assertEqual(default_assets_dir().name, ASSETS_DIRNAME)
            os.environ["F3VB_ASSETS_DIR"] = "X"
            self.assertEqual(default_assets_dir(), Path("X"))
        finally:
            os.environ.pop("F3VB_ASSETS_DIR", None)
            if old is not None:
                os.environ["F3VB_ASSETS_DIR"] = old

    def test_cancel_and_job(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game, _ = make_game(t)
            log = extract_all(game, t / "o2", ExtractOptions(), cancel=lambda: True)
            self.assertTrue(log["cancelled"])
            job = ExtractJob(game, t / "o3", ExtractOptions())
            job.start()
            end = time.time() + 20
            msgs = []
            while time.time() < end:
                try:
                    m = job.q.get(timeout=0.2)
                except Exception:
                    continue
                msgs.append(m[0])
                if m[0] == "done":
                    break
            self.assertIn("done", msgs)
            self.assertIn("progress", msgs)
            self.assertEqual(job.result["written"], 5)
            self.assertIsNone(job.error)


if __name__ == "__main__":
    unittest.main()
