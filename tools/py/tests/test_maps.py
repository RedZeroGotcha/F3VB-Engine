"""v14: leitores de .rle (grade de tiles) e .map (chunks de mapa), relatorio, JSON, PNG e integracao com convert-retail. Dados SINTETICOS."""
import csv
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from f3vb.mapfile import MapError, estimate_cell_scale, read_map
from f3vb.mapreport import analyze_map
from f3vb.rle import EMPTY, RleError, RleGrid, read_rle, rle_to_png, write_rle

TOOLS_PY = Path(__file__).resolve().parents[1]


def s16(t: str) -> bytes:
    b = t.encode("latin-1")
    return struct.pack("<H", len(b)) + b


def chunk(tag: str, ver: int, payload: bytes) -> bytes:
    return tag.encode() + struct.pack("<II", ver, 12 + len(payload)) + payload


def vec(x, y, z) -> bytes:
    return struct.pack("<3f", x, y, z)


def make_emap(rle="T.rle") -> bytes:
    tail = struct.pack("<H", 0) + bytes([10, 20, 30, 0]) + bytes([0, 0, 0, 128]) + bytes([1]) + struct.pack("<I", 0) + struct.pack("<3f", 250.0, 500.0, 1.0)
    return chunk("EMAP", 5, struct.pack("<I", 0) + s16("T.8") + s16(rle) + s16("T_AM.dds") + tail)


def make_eeov(inst="Npc_000", script="Npc.amx", inv=("Ammo.AMO", "Gun.WEA")) -> bytes:
    p = s16(inst) + bytes([1, 1, 1]) + bytes([2]) + bytes(7) + s16("") + s16(script) + struct.pack("<IB", 0, 1) + struct.pack("<f", 1.0)
    p += s16("Skin_Test.dds") + bytes([0, 1, 0]) + struct.pack("<I", len(inv)) + b"".join(s16(i) for i in inv)
    return chunk("EEOV", 2, p)


def make_eme2(proto="Critters_Npc.CRT", pos=(10.0, 0.0, 20.0)) -> bytes:
    p = s16(proto) + struct.pack("<4f", pos[0], pos[1], pos[2], 1.5) + bytes(8) + bytes([1]) + make_eeov()
    return chunk("EME2", 0, p)


def make_map(rle="T.rle") -> bytes:
    out = make_emap(rle)
    out += chunk("ECAM", 0, struct.pack("<4f", 0, 1000, 1000, 0))
    out += make_eme2()
    out += chunk("EMEF", 0, s16("Dust_000") + struct.pack("<4f", 5.0, 0.0, 6.0, 0.0) + bytes(8) + bytes([1]) + s16("FX_Dust.veg") + bytes([0]))
    out += chunk("EMEP", 0, bytes([1]) + b"".join(vec(10 + i, 0, 20 + i) for i in range(6)) + struct.pack("<6f", *[3.14] * 6))
    out += chunk("EMTR", 0, struct.pack("<II", 6, 4) + b"".join(vec(*p) for p in ((8, 9, 18), (12, 9, 18), (12, 9, 22), (8, 9, 22))))
    out += chunk("ETTR", 1, s16("Other_Map") + bytes([2, 1]))
    out += chunk("EMTR", 0, struct.pack("<II", 6, 3) + b"".join(vec(*p) for p in ((1, 9, 1), (3, 9, 1), (3, 9, 3))))
    out += chunk("ESTR", 0, s16("Door.amx") + bytes(4))
    out += chunk("EPTH", 0, s16("Waypoint01") + struct.pack("<I", 2) + b"".join(struct.pack("<6f", 10 + i, 0, 20, 0.5, 0, 0) for i in range(2)))
    out += chunk("EMSD", 0, s16("Amb") + vec(4, 0, 4) + s16("AMB_Wind.psf") + bytes([1, 1]))
    out += chunk("EMNO", 0, vec(7, 0, 7) + s16("Icon.dds") + struct.pack("<I", 1906))
    nodes = [(1, (10.0, 2.0, 20.0), [1, 0xFF, 0xFF, 0xFF, 0xFF]), (1, (12.0, 2.0, 20.0), [0, 0xFF, 0xFF, 0xFF, 0xFF])]
    out += chunk("EMNP", 0, struct.pack("<I", 2) + b"".join(bytes([k]) + vec(*p) + vec(0, 0, 0) + bytes(l) for k, p, l in nodes))
    out += chunk("2MWT", 1, s16("EFF_Water") + bytes(140) + struct.pack("<I", 1) + vec(0, -2, 0) + s16("W_0.TGA") + struct.pack("<2f", 0.25, 0.875))
    return out


def make_rle(width=30, rows=26) -> bytes:
    cells = bytearray([EMPTY] * (width * rows))
    for r in range(4, rows - 3):
        for c in range(3, width - 3):
            cells[r * width + c] = 5
    cells[10 * width + 10] = 27
    return write_rle(RleGrid(width, rows, width, EMPTY, bytes(cells)))


class RleTests(unittest.TestCase):
    def test_roundtrip_and_layout(self):
        data = make_rle()
        g = read_rle(data)
        self.assertEqual((g.width, g.rows, g.dim_c, g.fill), (30, 26, 30, 0x2F))
        self.assertEqual(struct.unpack_from("<3I", data, 0), (30, 27, 30))            # b = linhas + 1
        self.assertEqual(write_rle(g), data)
        self.assertEqual(g.at(10, 10), 27)
        self.assertTrue(g.is_empty(0, 0))
        self.assertEqual(g.bbox(), (3, 4, 26, 22))
        self.assertEqual(sum(g.histogram().values()), 30 * 26)

    def test_runs_never_cross_rows(self):
        g = read_rle(make_rle())
        pos = 13
        data = make_rle()
        acc = 0
        while pos < len(data):
            n, = struct.unpack_from("<I", data, pos)
            self.assertLessEqual(acc % g.width + n, g.width)
            acc += n
            pos += 5

    def test_errors(self):
        good = make_rle()
        with self.assertRaises(RleError):
            read_rle(good[:10])                                                        # cabecalho curto
        with self.assertRaises(RleError):
            read_rle(good[:-3])                                                        # run incompleto
        with self.assertRaises(RleError):
            read_rle(good[:-5])                                                        # celulas a menos
        bad = bytearray(good)
        struct.pack_into("<I", bad, 13, 31)                                            # run maior que a linha
        with self.assertRaises(RleError):
            read_rle(bytes(bad))
        with self.assertRaises(RleError):
            read_rle(struct.pack("<3I", 0, 5, 0) + b"/")

    def test_png(self):
        png = rle_to_png(read_rle(make_rle()))
        self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")


class MapTests(unittest.TestCase):
    def test_all_chunks_decode_without_leftover(self):
        m = read_map(make_map())
        self.assertEqual(m.unexplained_bytes, 0)
        self.assertEqual(m.unknown, [])
        self.assertEqual([c[0] for c in m.chunks][:3], ["EMAP", "ECAM", "EME2"])
        h = m.header
        self.assertEqual((h["tre"], h["rle"], h["ambient_map"]), ("T.8", "T.rle", "T_AM.dds"))
        self.assertEqual(h["color_a"], [10, 20, 30, 0])
        self.assertEqual(h["color_b"], [0, 0, 0, 128])
        self.assertEqual(h["floats"], [250.0, 500.0, 1.0])
        self.assertEqual(m.camera["floats"], [0.0, 1000.0, 1000.0, 0.0])

    def test_entities_and_eeov(self):
        e = read_map(make_map()).entities[0]
        self.assertEqual((e["prototype"], e["kind"]), ("Critters_Npc.CRT", "CRT"))
        self.assertEqual(e["pos"], [10.0, 0.0, 20.0])
        self.assertAlmostEqual(e["yaw"], 1.5)
        ev = e["eeov"]
        self.assertEqual(ev["instance"], "Npc_000")
        self.assertEqual(ev["flags"], [1, 1, 1])
        self.assertEqual(ev["script"], "Npc.amx")
        self.assertEqual(ev["skin"], "Skin_Test.dds")
        self.assertEqual(ev["inventory"], ["Ammo.AMO", "Gun.WEA"])

    def test_other_chunks(self):
        m = read_map(make_map())
        self.assertEqual(m.effects[0]["effect"], "FX_Dust.veg")
        ep = m.entry_points[0]
        self.assertEqual(ep["id"], 1)
        self.assertEqual(len(ep["positions"]), 6)
        self.assertEqual(ep["positions"][2], [12.0, 0.0, 22.0])
        self.assertEqual(len(m.nav_nodes), 2)
        self.assertEqual(m.nav_nodes[0]["links"], [1])
        self.assertEqual(m.nav_nodes[1]["pos"], [12.0, 2.0, 20.0])
        self.assertEqual(len(m.triggers), 2)
        self.assertEqual(len(m.triggers[0]["region"]["points"]), 4)
        self.assertEqual(m.triggers[0]["travel"]["target_map"], "Other_Map")
        self.assertEqual(m.triggers[0]["travel"]["entry_point"], 2)
        self.assertEqual(m.triggers[1]["script"]["script"], "Door.amx")
        self.assertEqual(m.paths[0]["name"], "Waypoint01")
        self.assertEqual(len(m.paths[0]["points"]), 2)
        self.assertEqual(m.sounds[0]["sound"], "AMB_Wind.psf")
        self.assertEqual(m.markers[0]["text_id"], 1906)
        self.assertEqual(m.markers[0]["icon"], "Icon.dds")
        self.assertEqual(m.water[0]["patches"][0]["texture"], "W_0.TGA")

    def test_framing_error_and_unknown_tag(self):
        with self.assertRaises(MapError):
            read_map(make_map()[:-3])
        m = read_map(make_emap() + chunk("ZZZZ", 0, b"\x01\x02\x03"))
        self.assertEqual(m.unknown[0]["tag"], "ZZZZ")
        self.assertGreater(m.unexplained_bytes, 0)

    def test_cell_scale_estimate(self):
        """Grade 60x60 com um bloco cheio so em colunas/linhas 40..59: apenas um fator acerta o bloco."""
        cells = bytearray([EMPTY] * 3600)
        for r in range(40, 60):
            for c in range(40, 60):
                cells[r * 60 + c] = 5
        g = RleGrid(60, 60, 60, EMPTY, bytes(cells))

        class M:                                                   # so o que estimate_cell_scale le
            def __init__(self, pts):
                self.entities = [{"pos": p} for p in pts]
                self.nav_nodes = []
                self.entry_points = []
        self.assertEqual(estimate_cell_scale(M([[45, 0, 45], [50, 0, 52], [58, 0, 41]]), g)[:2], (1.0, 3))
        self.assertEqual(estimate_cell_scale(M([[22, 0, 22], [25, 0, 26], [29, 0, 20]]), g)[:2], (2.0, 3))
        self.assertEqual(estimate_cell_scale(M([[11, 0, 11], [12, 0, 13], [14, 0, 10]]), g)[:2], (4.0, 3))
        self.assertEqual(estimate_cell_scale(M([]), g), (1.0, 0, 0))
        self.assertEqual(estimate_cell_scale(read_map(make_map()), None)[0], 1.0)

    def test_report_json_png(self):
        m, g, info, txt, js, png, _f3 = analyze_map("Teste", make_map(), make_rle())
        self.assertIn("=== MAPA Teste ===", txt)
        self.assertIn("bytes nao explicados pelos decodificadores: 0", txt)
        self.assertIn("viagem para 'Other_Map'", txt)
        self.assertIn("Critters_Npc.CRT", txt)
        d = json.loads(js)
        self.assertEqual(d["name"], "Teste")
        self.assertEqual(d["grid"]["width"], 30)
        self.assertEqual(len(d["entities"]), 1)
        self.assertEqual(d["entities"][0]["eeov"]["inventory"][1], "Gun.WEA")
        self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
        # sem .rle: relatorio e JSON continuam, sem imagem
        _m, g2, _i, txt2, js2, png2, _f32 = analyze_map("Sem", make_map(), None)
        self.assertIsNone(g2)
        self.assertIsNone(png2)
        self.assertIn("nao encontrada", txt2)


class MapsCommandsTests(unittest.TestCase):
    def _retail(self, t: Path) -> Path:
        root = t / "oa"
        (root / "RETAIL" / "Maps" / "MAP_Map").mkdir(parents=True)
        (root / "RETAIL" / "Maps" / "RLE").mkdir(parents=True)
        (root / "RETAIL" / "Maps" / "MAP_Map" / "00_T.map").write_bytes(make_map())
        (root / "RETAIL" / "Maps" / "RLE" / "T.rle").write_bytes(make_rle())
        with open(root / "index.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["pack", "numero", "tipo", "nome", "caminho", "tamanho"])
            w.writerow(["Maps", 0, "MAP_Map", "00_T", "Maps/MAP_Map/00_T.map", len(make_map())])
            w.writerow(["Maps", 1, "RLE", "T", "Maps/RLE/T.rle", len(make_rle())])
            w.writerow(["Scripts", 0, "AMX", "MainMenu", "Scripts/AMX/MainMenu.amx", 4])
            w.writerow(["Scripts", 1, "SMA_Script", "00 Frank", "Scripts/SMA_Script/00 Frank.amx", 4])
            w.writerow(["Scripts", 2, "SMA_Script", "MainMenu", "Scripts/SMA_Script/MainMenu_2.amx", 4])        # nome repetido em outro tipo
        for rel, data in (("Scripts/AMX/MainMenu.amx", b"AAAA"), ("Scripts/SMA_Script/00 Frank.amx", b"BBBB"), ("Scripts/SMA_Script/MainMenu_2.amx", b"CCCC")):
            (root / "RETAIL" / rel).parent.mkdir(parents=True, exist_ok=True)
            (root / "RETAIL" / rel).write_bytes(data)
        return root

    def _run(self, *args):
        return subprocess.run([sys.executable, "-m", "f3vb", *args], capture_output=True, text=True, cwd=str(TOOLS_PY))

    def test_export_textures_command(self):
        """v18: CONVERTED/textures/<nome>.f3tex a partir dos nomes citados pelos .f3map; o que nao existe no RETAIL vai para o resumo."""
        import struct as _st
        from f3vb.mapexport import build_f3map
        from f3vb.mapfile import read_map
        from f3vb.tre import read_tre
        from tests.test_tre import make_tre
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            root = self._retail(t)
            maps = root / "CONVERTED" / "maps"
            maps.mkdir(parents=True)
            (maps / "T.f3map").write_bytes(build_f3map("T", read_map(make_map()), None, read_tre(make_tre())))      # cita a.tga e lm_0.ctx
            tga = _st.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 32, 0x28) + bytes([10, 20, 30, 255, 40, 50, 60, 255, 70, 80, 90, 255, 1, 2, 3, 128])
            (root / "RETAIL" / "Tiles" / "TGA_Image").mkdir(parents=True)
            (root / "RETAIL" / "Tiles" / "TGA_Image" / "a.tga").write_bytes(tga)
            with open(root / "index.csv", "a", newline="", encoding="utf-8") as f:
                csv.writer(f).writerow(["Tiles", 0, "TGA_Image", "a", "Tiles/TGA_Image/a.tga", len(tga)])             # lm_0 NAO existe no RETAIL
            r = self._run("export-textures", "--out", str(root))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("texturas em CONVERTED/textures: 1", r.stdout)
            self.assertIn("faltando: lm_0 (lightmap)", r.stdout)
            data = (root / "CONVERTED" / "textures" / "a.f3tex").read_bytes()
            self.assertEqual(data[:4], b"F3TX")
            self.assertEqual(_st.unpack_from("<II", data, 4), (2, 2))
            self.assertEqual(len(data), 12 + 16)
            self.assertEqual(data[12:16], bytes([30, 20, 10, 255]))                                                  # TGA guarda BGRA; o .f3tex guarda RGBA, linha de cima primeiro
            self.assertIn("faltando: lm_0 (lightmap)", (root / "CONVERTED" / "textures" / "textures_summary.txt").read_text(encoding="utf-8"))
            self.assertFalse((root / "CONVERTED" / "textures" / "lm_0.f3tex").exists())
            r = self._run("export-textures", "--out", str(root))                                                      # de novo: nao refaz o que existe
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_ui_info_command(self):
        """v18: relatorio do pack Interface (sonda do .int com chunks) + imagens da UI (TGA e BMP) em CONVERTED/ui/*.f3tex."""
        import struct as _st
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            root = self._retail(t)

            def chunk(tag: bytes, payload: bytes, ver: int = 1) -> bytes:
                return tag + _st.pack("<II", ver, 12 + len(payload)) + payload
            inner = chunk(b"BTTN", b"Start\x00\x01\x02\x03")
            int_data = chunk(b"WIND", chunk(b"INFO", b"\x01\x00\x00\x00") + inner)
            tga = _st.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, 2, 1, 32, 0x28) + bytes([1, 2, 3, 255, 4, 5, 6, 255])
            bmp_px = bytes([10, 20, 30, 40, 50, 60, 0, 0])                                         # 2 pixels BGR + enchimento ate 4 bytes (linha unica)
            bmp = b"BM" + _st.pack("<IHHI", 14 + 40 + len(bmp_px), 0, 0, 54) + _st.pack("<IiiHHIIiiII", 40, 2, 1, 1, 24, 0, len(bmp_px), 0, 0, 0, 0) + bmp_px
            (root / "RETAIL" / "Interface").mkdir(parents=True)
            rows = [("Interface", 0, "INT_Interface", "mainmenu", "Interface/mainmenu.int", int_data), ("Interface", 1, "TGA_Image", "button", "Interface/button.tga", tga),
                    ("Interface", 2, "TGA_Image", "frame", "Interface/frame.bmp", bmp)]
            with open(root / "index.csv", "a", newline="", encoding="utf-8") as f:
                for pack, num, typ, name, rel, data in rows:
                    (root / "RETAIL" / rel).write_bytes(data)
                    csv.writer(f).writerow([pack, num, typ, name, rel, len(data)])
            r = self._run("ui-info", "--out", str(root))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            rep = (root / "CONVERTED" / "ui" / "ui_report.txt").read_text(encoding="utf-8")
            self.assertIn("INT_Interface: 1", rep)
            self.assertIn("enquadramento tag/versao/tamanho: FECHA", rep)
            self.assertIn("WIND v1", rep)
            self.assertIn("BTTN v1", rep)
            self.assertIn("Start", rep)
            self.assertIn("imagens da interface em CONVERTED/ui: 2", r.stdout)
            d = (root / "CONVERTED" / "ui" / "button.f3tex").read_bytes()
            self.assertEqual((d[:4], _st.unpack_from("<II", d, 4), d[12:20]), (b"F3TX", (2, 1), bytes([3, 2, 1, 255, 6, 5, 4, 255])))
            self.assertTrue((root / "CONVERTED" / "ui" / "frame.f3tex").exists())

    def test_map_info_and_rle_info(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            root = self._retail(t)
            r = self._run("map-info", "_T", "--assets", str(root), "--out", str(t / "m.txt"), "--json", str(t / "m.json"), "--png", str(t / "m.png"))
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("=== MAPA _T ===", r.stdout)
            self.assertTrue((t / "m.txt").exists() and (t / "m.json").exists() and (t / "m.png").exists())
            r = self._run("rle-info", "T", "--assets", str(root), "--png", str(t / "g.png"))
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("30 colunas x 26 linhas", r.stdout)
            r = self._run("map-info", "naoexiste", "--assets", str(root))
            self.assertNotEqual(r.returncode, 0)

    def test_map_info_on_loose_files(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            (t / "a.map").write_bytes(make_map())
            (t / "a.rle").write_bytes(make_rle())
            r = self._run("map-info", str(t / "a.map"), "--rle", str(t / "a.rle"))
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("grade (.rle): 30 colunas", r.stdout)

    def test_convert_retail_maps(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            root = self._retail(t)
            r = self._run("convert-retail", "--maps", "--out", str(root))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            out = root / "CONVERTED" / "maps"
            self.assertTrue((out / "00_T.txt").exists() and (out / "00_T.json").exists() and (out / "00_T.png").exists())
            self.assertIn("00_T", (out / "maps_summary.txt").read_text(encoding="utf-8"))
            self.assertIn("mapas: 1", (root / "EXTRACAO_RELATORIO.txt").read_text(encoding="utf-8"))
            sc = root / "CONVERTED" / "scripts"                          # v17: scripts ao lado dos mapas
            self.assertEqual(sorted(p.name for p in sc.iterdir()), ["00 Frank.amx", "MainMenu.amx", "MainMenu_2.amx"])
            self.assertEqual((sc / "00 Frank.amx").read_bytes(), b"BBBB")
            self.assertIn("scripts: 3", (root / "EXTRACAO_RELATORIO.txt").read_text(encoding="utf-8"))

    def test_export_scripts_command(self):
        with tempfile.TemporaryDirectory() as t:
            root = self._retail(Path(t))
            r = self._run("export-scripts", "--out", str(root))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("scripts gravados em CONVERTED/scripts: 3", r.stdout)
            self.assertTrue((root / "CONVERTED" / "scripts" / "MainMenu.amx").exists())
            self.assertFalse((root / "CONVERTED" / "maps").exists())      # so os scripts


if __name__ == "__main__":
    unittest.main()
