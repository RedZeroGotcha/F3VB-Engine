import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

from f3vb.catalog import build_catalog, catalog_summary, catalog_tsv, find_grps
from f3vb.cli import main as cli_main
from f3vb.rht import parse_rht
from tests.test_analyze import make_amx
from tests.test_extract import make_b3d, make_tga
from tests.test_readers import build_grp


def build_rht_real(entries, packs, names):
    """entries: (number, type, pack_value, name). Monta um .rht com região de packs e de nomes."""
    pack_blob = b"".join(p.encode() + b"\0" for p in packs)
    name_blob, offs = b"", {}
    for *_, nm in entries:
        offs[nm] = len(name_blob)
        name_blob += nm.encode() + b"\0"
    n = len(entries)
    off_packs = 20 + 20 * n
    off_res = off_packs + len(pack_blob)
    out = struct.pack("<5I", 1, 1, n, off_packs, off_res)
    for num, typ, pack, nm in entries:
        out += struct.pack("<5I", 1, num, typ, offs[nm], pack)
    return out + pack_blob + name_blob


class CatalogTests(unittest.TestCase):
    def _setup(self, t: Path):
        game = t / "demo" / "data"
        game.mkdir(parents=True)
        (game / "Tiles.grp").write_bytes(build_grp([make_tga(), make_b3d()]))
        (game / "Scripts.grp").write_bytes(build_grp([make_amx()]))
        # duplicata aninhada, mesmo tamanho
        (game / "Demo" / "data").mkdir(parents=True)
        (game / "Demo" / "data" / "Tiles.grp").write_bytes((game / "Tiles.grp").read_bytes())
        # pack "Tiles" em 0, "Scripts" em 6
        raw = build_rht_real([(0, 200, 0, "a.tga"), (1, 100, 0, "b.b3d"), (0, 1200, 6, "s.amx")],
                             ["Tiles", "Scripts"], None)
        (t / "demo" / "resource.rht").write_bytes(raw)
        return t / "demo", raw

    def test_pack_labels_and_names(self):
        raw = build_rht_real([(0, 200, 0, "a.tga"), (0, 1200, 6, "s.amx")], ["Tiles", "Scripts"], None)
        info = parse_rht(raw)
        self.assertEqual(info.pack_label(0), "Tiles")
        self.assertEqual(info.pack_label(6), "Scripts")
        self.assertEqual(info.resource_name(info.entries[1].offset), "s.amx")
        self.assertEqual(info.summary()["by_pack"], {"Tiles": 1, "Scripts": 1})

    def test_catalog_validates_hypothesis(self):
        with tempfile.TemporaryDirectory() as t:
            game, raw = self._setup(Path(t))
            info = parse_rht(raw)
            cat = build_catalog(game, info)
            self.assertEqual(len(cat["rows"]), 3)
            self.assertEqual(cat["agreement"]["TGA_Image"], {"tga": 1})
            self.assertEqual(cat["agreement"]["G3D_Model"], {"b3d": 1})
            self.assertEqual(cat["agreement"]["AMX"], {"amx": 1})
            self.assertTrue(all(r["count_match"] and r["numbers_are_permutation"] for r in cat["pack_report"].values()))
            self.assertEqual(cat["unreferenced_lumps"], 0)
            self.assertEqual(cat["names_present"], 3)
            self.assertEqual(cat["name_spacing"]["inconsistent"], 0)
            self.assertEqual(len(cat["duplicates_ignored"]), 1)
            self.assertIn("a.tga", catalog_tsv(cat))
            self.assertIn("concordância", catalog_summary(cat, info))

    def test_find_grps_prefers_shallow(self):
        with tempfile.TemporaryDirectory() as t:
            game, _ = self._setup(Path(t))
            found, dups = find_grps(game)
            self.assertEqual(found["tiles"], game / "data" / "Tiles.grp")
            self.assertEqual(len(dups), 1)

    def test_cli_catalog_zipdir_inspect(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game, _ = self._setup(t)
            out = t / "cat"
            cli_main(["catalog", str(game), "--out", str(out)])
            self.assertTrue((out / "catalogo.txt").exists() and (out / "catalogo_resumo.txt").exists())
            cli_main(["rht", str(game / "resource.rht"), "--entries", "3"])   # regressão: --entries dava exit 1
            amx = t / "s.amx"
            amx.write_bytes(make_amx())
            cli_main(["inspect", str(amx)])
            z = t / "o.zip"
            cli_main(["zipdir", str(out), str(z)])
            with zipfile.ZipFile(z) as zf:
                self.assertTrue(all("\\" not in n for n in zf.namelist()))
                self.assertIn("catalogo.txt", zf.namelist())


if __name__ == "__main__":
    unittest.main()


class B3dFailTests(unittest.TestCase):
    def test_report_groups_failures(self):
        import tempfile
        from pathlib import Path
        from f3vb.b3dfail import b3d_failures
        from tests.test_extract import make_b3d
        from tests.test_readers import build_grp
        good = make_b3d()
        bad = good[:-40]                       # truncado: não termina
        with tempfile.TemporaryDirectory() as t:
            p = Path(t) / "Props.grp"
            p.write_bytes(build_grp([good, bad, bad + b"\x00"]))
            text = b3d_failures(p)
        self.assertIn("3 arquivos, 1 decodificados por completo, 2 nao", text)
        self.assertIn("MOTIVO", text)
        self.assertIn("lump #1", text)
