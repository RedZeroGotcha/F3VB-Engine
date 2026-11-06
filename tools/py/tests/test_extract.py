import json
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from f3vb.classify import classify
from f3vb.extract import extract, sample
from f3vb.gr2tool import _VENDOR, gr2_header, summarize_gr2
from f3vb.probes import probe_b3d, probe_bmp, probe_tga
from tests.test_readers import build_grp  # noqa: E402


def make_tga(w=4, h=3, bpp=24):
    hdr = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, bpp, 0)
    return hdr + b"\x10" * (w * h * (bpp // 8))


def s16(t: str) -> bytes:
    return struct.pack("<H", len(t)) + t.encode("latin-1")


def make_b3d(is_bones: bool = False, textured: bool = True):
    """B3D sintético (família de vértice de 44 bytes), no layout observado em arquivos reais."""
    d = b"B3D 1.1\x00"
    d += bytes([0x03]) + struct.pack("<f", 1.0)
    d += bytes([0x05]) + s16("Z_UP")
    d += bytes([0x07]) + s16("mtl0") + s16("Body") + struct.pack("<18f", *range(18)) + s16("OPAQUE") + s16("PHONG") + struct.pack("<ii", 1, 2)
    d += bytes([0x08]) + b"\x00" + s16("tex0") + s16("body.tga") + struct.pack("<II", 128, 64)
    d += bytes([0x0E]) + s16("Bip01") + struct.pack("<H", 0) + b"\x01" + struct.pack("<3f", 0, 1, 2)
    d += bytes([0x0A]) + bytes([0x0C]) + s16("Scene Root")
    d += bytes([0x0A]) + bytes([0x0C]) + s16("Mesh01")
    verts = [(0.0, 0.0, 0.0), (1.0, 2.0, 3.0), (1.0, 0.0, 2.0)]
    d += bytes([0x0F]) + bytes([0, 0, 1]) + struct.pack("<ii", len(verts), 44)
    for i, (x, y, z) in enumerate(verts):
        d += struct.pack("<3f3f4B2f2i", x, y, z, 0.0, 0.0, 1.0, 255, 255, 255, 255, float(i), 0.5, 0, 0)
    d += struct.pack("<6f", 0.0, 0.0, 0.0, 1.0, 2.0, 3.0)                 # caixa envolvente = min/max das posicoes
    if is_bones:
        d += bytes([1]) + struct.pack("<i", 4)
        for k in range(4):                                           # pares (osso, peso) POR SLOT: slot k de todos os vertices
            for _v in verts:
                d += struct.pack("<if", 0 if k == 0 else -1, 1.0 if k == 0 else 0.0)
    else:
        d += bytes([0])
    d += struct.pack("<i", 1)                                    # numMaterials
    d += s16("Default") + s16("BASE_2X") + struct.pack("<17f", *range(17)) + s16("OPAQUE") + s16("SILENT")
    words = [257] + [0] * 14 + [1 if textured else 0]
    d += struct.pack("<16i", *words)
    if textured:
        d += s16("weaptex.tga")
    d += struct.pack("<I", 0)                                    # extra
    d += s16("Default") + struct.pack("<ii", 6, 2) + struct.pack("<6H", 0, 1, 2, 2, 1, 0)
    d += bytes([0x0B, 0x0B, 0x01])
    return d


def make_gr2():
    sys.path.insert(0, str(_VENDOR))
    from io_scene_gr2.gr2.export import ExportMesh, ExportScene, ExportVertex
    from io_scene_gr2.gr2.write import write_raw_gr2
    scene = ExportScene(meshes=(ExportMesh(
        name="Tri", vertices=(ExportVertex((0, 0, 0), (0, 0, 1), (0, 1), ()), ExportVertex((1, 0, 0), (0, 0, 1), (1, 1), ()),
                              ExportVertex((0, 1, 0), (0, 0, 1), (0, 0), ())),
        indices=(0, 1, 2), material_names=("Body",), material_indices=(0,),
        material_texture_files=("t.dds",), material_texture_sizes=((8, 8),)),), skeletons=())
    with tempfile.TemporaryDirectory() as d:
        p = write_raw_gr2(Path(d) / "t.gr2", scene)
        return Path(p).read_bytes()


class ProbeTests(unittest.TestCase):
    def test_tga(self):
        p = probe_tga(make_tga())
        self.assertEqual((p["width"], p["height"], p["bpp"]), (4, 3, 24))
        self.assertTrue(p["plausible"] and p["size_ok"])
        self.assertFalse(probe_tga(make_tga()[:-5])["size_ok"])

    def test_bmp(self):
        d = b"BM" + struct.pack("<IHHII", 70, 0, 0, 54, 40) + struct.pack("<iiHHI", 4, 3, 1, 24, 0) + b"\0" * 40
        p = probe_bmp(d)
        self.assertEqual((p["width"], p["height"], p["bpp"]), (4, 3, 24))

    def test_b3d(self):
        p = probe_b3d(make_b3d())
        self.assertEqual(p["globals"]["coordinate_system"], "Z_UP")
        self.assertEqual(p["textures"][0]["file"], "body.tga")
        self.assertEqual(p["materials"][0]["name"], "Body")
        self.assertEqual(p["bone_count"], 1)
        self.assertEqual(p["node_names"], ["Scene Root", "Mesh01"])
        sf = p["surfaces"][0]
        self.assertEqual((sf["vertices"], sf["family"], sf["complete"]), (3, "44", True))
        self.assertEqual(sf["textures"], ["weaptex.tga"])
        self.assertEqual(sf["index_counts"], [6])
        self.assertIsNone(p["stopped"])
        self.assertEqual(p["ended_at"], len(make_b3d()))

    def test_b3d_variants(self):
        p = probe_b3d(make_b3d(is_bones=True, textured=False))
        sf = p["surfaces"][0]
        self.assertTrue(sf["complete"] and sf["bone_weights"] and sf["bones_per_vertex"] == 4)
        self.assertEqual(sf["textures"], [None])
        self.assertIsNone(p["stopped"])

    def test_b3d_64_family_terrain(self):
        d = make_b3d()
        i = d.index(bytes([0x0F]) + bytes([0, 0, 1]))
        nv = 4
        tail = (struct.pack("<II", 0, 0) + s16("LM_32_Floor") + struct.pack("<i", 1) + s16("Ext_Grass.tga")
                + struct.pack("<I", 0) + bytes([0xC9]) + struct.pack("<i", 6)
                + struct.pack("<6i", 0, 1, 2, 2, 1, 3) + b"\x01" * 6 + bytes([0x0B, 0x0B, 0x01]))
        d64 = d[:i] + bytes([0x0F]) + bytes([0, 0, 0]) + struct.pack("<i", nv) + b"\x00" * (nv * 64) + tail
        p = probe_b3d(d64)
        sf = p["surfaces"][0]
        self.assertEqual((sf["family"], sf["complete"], sf["vertices"]), ("64", True, nv))
        self.assertEqual((sf["lightmap"], sf["textures"], sf["index_counts"]), ("LM_32_Floor", ["Ext_Grass.tga"], [6]))
        self.assertIsNone(p["stopped"])
        self.assertEqual(p["ended_at"], len(d64))

    def test_b3d_64_family_bad_index_is_reported(self):
        d = make_b3d()
        i = d.index(bytes([0x0F]) + bytes([0, 0, 1]))
        tail = (struct.pack("<II", 0, 0) + s16("LM") + struct.pack("<i", 1) + s16("t.tga") + struct.pack("<I", 0)
                + bytes([0xC9]) + struct.pack("<i", 3) + struct.pack("<3i", 0, 1, 99) + b"\x01" * 3 + bytes([0x0B, 0x0B, 0x01]))
        d64 = d[:i] + bytes([0x0F]) + bytes([0, 0, 0]) + struct.pack("<i", 4) + b"\x00" * 256 + tail
        p = probe_b3d(d64)
        self.assertIn("fora do intervalo", p["stopped"]["reason"])

    def test_b3d_truncated_and_bad(self):
        p = probe_b3d(make_b3d()[:40])
        self.assertIsNotNone(p["stopped"])
        self.assertIn("B3D", probe_b3d(b"XXXX1234")["stopped"]["reason"])


class Gr2Tests(unittest.TestCase):
    def test_header_and_summary(self):
        data = make_gr2()
        self.assertEqual(classify(data[:64]), "gr2")
        h = gr2_header(data)
        self.assertEqual(h["compressions"], ["none"])
        self.assertGreaterEqual(h["section_count"], 1)
        s = summarize_gr2(data)           # nenhuma etapa pode levantar exceção
        self.assertIn("header", s)
        self.assertIsInstance(s["errors"], list)

    def test_not_gr2(self):
        s = summarize_gr2(b"\x00" * 100)
        self.assertEqual(s["errors"][0]["stage"], "header")


class ExtractTests(unittest.TestCase):
    def _grp(self, d: Path) -> Path:
        blobs = [make_gr2(), make_tga(), make_b3d(), b"hello world text" * 80, make_tga(16, 16, 32)]
        p = d / "a.grp"
        p.write_bytes(build_grp(blobs))
        return p

    def test_extract_filters(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            grp = self._grp(t)
            m = extract(grp, t / "o", classes=["b3d", "gr2"])
            self.assertEqual(sorted(e["class"] for e in m["files"]), ["b3d", "gr2"])
            self.assertTrue((t / "o" / "manifest.json").exists())
            m2 = extract(grp, t / "o2", indices=[1])
            self.assertEqual(m2["count"], 1)
            self.assertEqual(m2["files"][0]["probe"]["width"], 4)

    def test_sample_zip(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            grp = self._grp(t)
            m = sample(grp, t / "sample", classes=("gr2", "b3d", "tga"), per_class=1, min_bytes=16)
            self.assertEqual(m["count"], 3)
            self.assertEqual(m["not_found"], [])
            with zipfile.ZipFile(m["zip"]) as z:
                names = set(z.namelist())
                self.assertIn("manifest.json", names)
                self.assertIn("manifest.txt", names)
                self.assertIn("LEIA-ME.txt", names)
                self.assertEqual(len([n for n in names if n.startswith("hexdump_") and n.endswith(".txt")]), 3)
                self.assertEqual(len([n for n in names if n.endswith((".gr2", ".b3d", ".tga"))]), 3)
                man = json.loads(z.read("manifest.json"))
                self.assertTrue(all(len(e["sha256"]) == 64 for e in man["files"]))


class TextOnlyTests(unittest.TestCase):
    def test_text_only_zip_has_no_binaries(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            blobs = [make_gr2(), make_tga(), make_b3d()]
            grp = t / "a.grp"
            grp.write_bytes(build_grp(blobs))
            m = sample(grp, t / "s", per_class=1, min_bytes=16, text_only=True)
            with zipfile.ZipFile(m["zip"]) as z:
                names = z.namelist()
                self.assertTrue(all(n.endswith(".txt") for n in names), names)
                self.assertIn("manifest.txt", names)
                txt = z.read("manifest.txt").decode("utf-8")
                self.assertIn("sha256=", txt)
                self.assertIn("body.tga", txt)


if __name__ == "__main__":
    unittest.main()
