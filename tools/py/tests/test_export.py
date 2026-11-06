import struct
import tempfile
import unittest
from pathlib import Path

from f3vb.b3d import B3DError, read_b3d
from f3vb.cli import main as cli_main
from f3vb.export import export_b3d_to_glb, DEFAULT_MESH_SCALE
from f3vb.glb import read_accessor, read_glb, validate_glb
from f3vb.gr2export import Clip, Joint
from f3vb.imagecodec import decode_tga, encode_png, image_to_png
from tests.test_catalog import build_rht_real
from tests.test_extract import make_b3d, make_tga
from tests.test_readers import build_grp


class B3DReaderTests(unittest.TestCase):
    def test_reads_geometry_and_bbox_check(self):
        m = read_b3d(make_b3d(is_bones=True))
        (node, surf), = list(m.surfaces())
        self.assertEqual(len(surf.positions), 3)
        self.assertEqual(surf.bones_per_vertex, 4)
        self.assertEqual(surf.materials[0].textures, ["weaptex.tga"])
        self.assertEqual(surf.primitives[0]["indices"], [0, 1, 2, 2, 1, 0])

    def test_rejects_garbage(self):
        with self.assertRaises(B3DError):
            read_b3d(b"B3D 1.1\x00\x7f")
        with self.assertRaises(B3DError):
            read_b3d(b"XXXX")


class ExportTests(unittest.TestCase):
    def test_static_glb_is_valid(self):
        glb, rep = export_b3d_to_glb(make_b3d(), "teste")
        self.assertEqual(validate_glb(glb), [])
        js, bn = read_glb(glb)
        self.assertEqual(js["meshes"][0]["primitives"][0]["mode"], 4)
        self.assertEqual(rep["stats"]["triangles"], 2)
        self.assertTrue(any("weaptex" in w for w in rep["warnings"]))   # sem textura disponível

    def test_texture_is_embedded(self):
        glb, rep = export_b3d_to_glb(make_b3d(), "t", texture_loader=lambda stem: make_tga() if stem == "weaptex" else None)
        js, _ = read_glb(glb)
        self.assertEqual(rep["stats"]["textures"], 1)
        self.assertEqual(js["images"][0]["mimeType"], "image/png")
        self.assertEqual(validate_glb(glb), [])

    def test_z_flip_and_scale(self):
        raw = read_b3d(make_b3d()).roots[0].children[0].surfaces[0].positions
        for ms in (1.0, DEFAULT_MESH_SCALE):                   # v12: a escala da malha multiplica a do usuário
            glb, _ = export_b3d_to_glb(make_b3d(), "t", unit_scale=2.0, mesh_scale=ms)
            js, bn = read_glb(glb)
            pos = read_accessor(js, bn, js["meshes"][0]["primitives"][0]["attributes"]["POSITION"])
            for (x, y, z), (rx, ry, rz) in zip(pos, raw):
                self.assertAlmostEqual(x, rx * 2 * ms, places=5)
                self.assertAlmostEqual(z, -rz * 2 * ms, places=5)

    def test_skin_and_animation(self):
        joints = [Joint("Root", -1, (0, 0, 0), (0, 0, 0, 1), (1, 1, 1), tuple(float(i == j) for i in range(4) for j in range(4))),
                  Joint("Bip01", 0, (0, 1, 0), (0, 0, 0, 1), (1, 1, 1), tuple(float(i == j) for i in range(4) for j in range(4)))]
        clip = Clip("idle", 1.0, tracks={"Bip01": {"translation": ([0.0, 0.5, 1.0], [(0, 1, 0), (0, 1.2, 0), (0, 1, 0)]),
                                                   "rotation": ([0.0, 1.0], [(0, 0, 0, 1), (0, 0.7071, 0, 0.7071)])}})
        glb, rep = export_b3d_to_glb(make_b3d(is_bones=True), "m", joints=joints, clips=[clip])
        self.assertEqual(validate_glb(glb), [], rep["warnings"])
        js, _ = read_glb(glb)
        self.assertEqual(len(js["skins"][0]["joints"]), 2)
        self.assertEqual(len(js["animations"][0]["channels"]), 2)
        self.assertIn("JOINTS_0", js["meshes"][0]["primitives"][0]["attributes"])

    def test_cli_export_with_game_dir(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game = t / "demo" / "data"
            game.mkdir(parents=True)
            (game / "Critters.grp").write_bytes(build_grp([make_b3d(), make_tga()]))
            raw = build_rht_real([(0, 100, 0, "CR_Teste"), (1, 200, 0, "weaptex")], ["Critters"], None)
            (t / "demo" / "resource.rht").write_bytes(raw)
            out = t / "o.glb"
            cli_main(["export-list", "G3D_Model", str(t / "demo")])
            cli_main(["export-glb", "CR_Teste", str(t / "demo"), "--out", str(out)])
            self.assertEqual(validate_glb(out.read_bytes()), [])
            js, _ = read_glb(out.read_bytes())
            self.assertEqual(len(js["images"]), 1)


class ImageTests(unittest.TestCase):
    def test_tga_to_png_roundtrip(self):
        w, h, rgba = decode_tga(make_tga(4, 3, 24))
        self.assertEqual((w, h, len(rgba)), (4, 3, 48))
        w2, h2, png = image_to_png(make_tga(4, 3, 24))
        self.assertTrue(png.startswith(b"\x89PNG"))
        self.assertTrue(encode_png(2, 2, bytes([255, 0, 0, 128] * 4)).startswith(b"\x89PNG"))


if __name__ == "__main__":
    unittest.main()


class StfTests(unittest.TestCase):
    def test_parse_and_text(self):
        from f3vb.stf import StfError, parse_stf
        texts = ["Ola, viajante.", "Bem-vindo ao Vault 13"]
        head = b"STFMAGIC" + struct.pack("<I", 2)
        base = len(head) + 2 * 16
        blob, entries = b"", b""
        for i, t in enumerate(texts):
            raw = t.encode("cp1252")
            entries += struct.pack("<II8s", base + len(blob), len(raw), b"\0" * 8)
            blob += raw
        data = head + entries + blob
        stf = parse_stf(data)
        self.assertEqual(stf.count, 2)
        self.assertEqual(stf.text(data, 1), texts[1])
        with self.assertRaises(StfError):
            stf.text(data, 5)
        with self.assertRaises(StfError):
            parse_stf(b"STFMAGIC" + struct.pack("<I", 999))

    def test_utf16(self):
        from f3vb.stf import parse_stf
        raw = "Água".encode("utf-16-le")
        head = b"STFMAGIC" + struct.pack("<I", 1)
        data = head + struct.pack("<II8s", 12 + 16, len(raw), b"\0" * 8) + raw
        self.assertEqual(parse_stf(data).text(data, 0), "Água")
