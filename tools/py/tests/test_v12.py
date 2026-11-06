"""v12: escala malha B3D -> esqueleto, posicionamento inicial do GR2 e pose de bind reproduzindo a malha."""
import math
import unittest

from f3vb.b3d import read_b3d
from f3vb.export import DEFAULT_MESH_SCALE, Exporter, apply_initial_placement, detect_initial_placement, resolve_bone_aliases
from f3vb.extractor import anim_prefixes
from f3vb.f3m import read_f3m, skin_positions, write_f3m
from f3vb.gr2export import Clip, Joint
from tests.test_extract import make_b3d


def _rigid_inverse(w):
    r = [[w[i][j] for j in range(3)] for i in range(3)]
    t = [w[i][3] for i in range(3)]
    rt = [[r[j][i] for j in range(3)] for i in range(3)]
    ti = [-sum(rt[i][k] * t[k] for k in range(3)) for i in range(3)]
    return [rt[i] + [ti[i]] for i in range(3)] + [[0, 0, 0, 1]]


def _mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def _trans(t):
    return [[1, 0, 0, t[0]], [0, 1, 0, t[1]], [0, 0, 1, t[2]], [0, 0, 0, 1]]


def _joints(shift):
    """Raiz em (0,0.5,0) e um filho em (0,0.2,0.1); IBM = inverse(mundo) * trans(shift) (shift=None -> sem deslocamento)."""
    local = [((0.0, 0.5, 0.0), -1), ((0.0, 0.2, 0.1), 0)]
    world, out = [], []
    for i, (pos, parent) in enumerate(local):
        loc = _trans(pos)
        w = loc if parent < 0 else _mul(world[parent], loc)
        world.append(w)
        ibm_col = _rigid_inverse(w)
        if shift:
            ibm_col = _mul(ibm_col, _trans(shift))
        flat = tuple(ibm_col[c][r] for r in range(4) for c in range(4))     # inverse_world[r*4+c] = ibm_col[c][r]
        out.append(Joint(f"J{i}", parent, pos, (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0), flat))
    return out


class InitialPlacementTests(unittest.TestCase):
    def test_none_when_consistent(self):
        self.assertIsNone(detect_initial_placement(_joints(None)))

    def test_detects_constant_offset(self):
        # P = mundo * IBM = trans(shift)  =>  T = -shift aplicado à raiz
        p = detect_initial_placement(_joints((0.01, 0.0, 0.02)))
        self.assertIsNotNone(p)
        for got, want in zip(p, (-0.01, 0.0, -0.02)):
            self.assertAlmostEqual(got, want, places=6)

    def test_not_constant_is_ignored(self):
        js = _joints((0.01, 0, 0.02))
        bad = _joints((0.05, 0, 0.02))
        self.assertIsNone(detect_initial_placement([js[0], bad[1]]))

    def test_applies_to_roots_and_root_translation_keys(self):
        js = _joints((0.01, 0.0, 0.02))
        clip = Clip("c", 1.0, {"J0": {"translation": ([0.0, 1.0], [(0.0, 0.5, 0.0), (0.0, 0.6, 0.0)])},
                               "J1": {"translation": ([0.0], [(0.0, 0.2, 0.1)])}})
        nj, nc, place = apply_initial_placement(js, [clip])
        self.assertAlmostEqual(nj[0].position[0], -0.01, places=6)
        self.assertEqual(nj[1].position, js[1].position)                       # filho intacto
        self.assertAlmostEqual(nc[0].tracks["J0"]["translation"][1][1][2], -0.02, places=6)
        self.assertEqual(nc[0].tracks["J1"]["translation"][1][0], (0.0, 0.2, 0.1))

    def test_bind_pose_reproduces_mesh(self):
        """Com o deslocamento compensado, pose de bind skinada == malha (era deslocada em CR_FloatingEyeBot)."""
        js = _joints((0.01, 0.0, 0.02))
        model = read_b3d(make_b3d())
        sc = read_f3m(write_f3m(Exporter().scene(model, "t", joints=js, clips=[])))
        for mi, me in enumerate(sc["meshes"]):
            if me.get("joints") is None:
                continue
            bind = skin_positions(sc, mi, None, 0)
            for a, b in zip(bind, me["pos"]):
                self.assertLess(math.dist(a, b), 1e-5)


class MeshScaleTests(unittest.TestCase):
    def test_default_is_100_over_1024(self):
        self.assertAlmostEqual(DEFAULT_MESH_SCALE, 100.0 / 1024.0)

    def test_scale_applies_to_vertices_only_not_skeleton(self):
        model = read_b3d(make_b3d())
        raw = Exporter(mesh_scale=1.0).scene(model, "t")
        scaled = Exporter().scene(model, "t")
        for m1, m2 in zip(raw["meshes"], scaled["meshes"]):
            for a, b in zip(m1["pos"], m2["pos"]):
                for i in range(3):
                    self.assertAlmostEqual(b[i], a[i] * DEFAULT_MESH_SCALE, places=5)
        js = _joints(None)
        s1 = Exporter(mesh_scale=1.0).scene(model, "t", joints=js)
        s2 = Exporter().scene(model, "t", joints=js)
        self.assertEqual([j["t"] for j in s1["joints"]], [j["t"] for j in s2["joints"]])    # esqueleto não muda


class FailureSummaryTests(unittest.TestCase):
    def test_groups_by_cause_and_pack(self):
        from f3vb.extractor import failure_summary
        fl = ["B3D Critters/CR_A: token de nó desconhecido 0x00 em 129303", "B3D Critters/CR_B: token de nó desconhecido 0x00 em 55",
              "B3D Props/P1: numVerts absurdo (-855638016) na superfície em 269", "B3D Props/P2: numVerts absurdo (1157627904) na superfície em 11"]
        txt = "\n".join(failure_summary(fl))
        self.assertIn("2  B3D: token de nó desconhecido 0xN em N", txt)
        self.assertIn("2  B3D: numVerts absurdo (N) na superfície em N", txt)
        self.assertIn("2  B3D em Critters", txt)
        self.assertIn("2  B3D em Props", txt)


class BoneAliasTests(unittest.TestCase):
    def _joints(self, names):
        return [Joint(n, -1 if i == 0 else 0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0), tuple([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]))
                for i, n in enumerate(names)]

    def test_suffix_aliases_like_hum_skeleton(self):
        """Medido em HumFSkl: o B3D diz 'L Clavicle', o GR2 tem 'L Clavicle_1'."""
        js = self._joints(["Female_Collision", "Root", "L Clavicle_1", "R Fingers_1", "R Handle_1", "Head"])
        res, al = resolve_bone_aliases(["Root", "L Clavicle", "R Fingers", "R Handle", "Head", "Inexistente"], js)
        self.assertEqual(res, ["Root", "L Clavicle_1", "R Fingers_1", "R Handle_1", "Head", "Inexistente"])
        self.assertEqual(al, {"L Clavicle": "L Clavicle_1", "R Fingers": "R Fingers_1", "R Handle": "R Handle_1"})

    def test_exact_name_wins_and_case_and_strip(self):
        js = self._joints(["Root", "Arm", "Arm_1", "leg", "Hand"])
        res, al = resolve_bone_aliases(["Arm", "Leg", "Hand_2"], js)
        self.assertEqual(res[0], "Arm")                                  # exato vence "Arm_1"
        self.assertEqual(res[1], "leg")                                  # maiusculas
        self.assertEqual(res[2], "Hand")                                 # sufixo _N removido
        self.assertNotIn("Arm", al)

    def test_no_joints_is_identity(self):
        self.assertEqual(resolve_bone_aliases(["A", "B"], None), (["A", "B"], {}))

    def test_exporter_uses_aliases_in_f3m(self):
        model = read_b3d(make_b3d())
        names = [b.name for b in model.bones]
        js = self._joints(["Root"] + [n + "_1" for n in names] if names else ["Root"])
        sc = Exporter().scene(model, "t", joints=js, clips=[])
        self.assertEqual(sc["bone_names"], [n + "_1" for n in names])


class AnimFamilyTests(unittest.TestCase):
    def test_prefixes(self):
        self.assertEqual(anim_prefixes("CR_Dog"), ["CR_Dog_"])
        self.assertEqual(anim_prefixes("HumMNor"), ["HumMNor_", "HumM_"])
        self.assertEqual(anim_prefixes("HumFSkl"), ["HumFSkl_", "HumF_"])
        self.assertEqual(anim_prefixes("HumM"), ["HumM_"])


class PackSampleTests(unittest.TestCase):
    def test_pack_sample_collects_model_files(self):
        import tempfile, zipfile
        from pathlib import Path
        from f3vb.extractor import ExtractOptions, extract_all
        from f3vb.index import RetailIndex
        from f3vb.sample import write_sample_zip
        from tests.test_extractor import make_game
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game, _ = make_game(t)
            out = t / "oa"
            extract_all(game, out, ExtractOptions())
            ix = RetailIndex.from_csv(out)
            n, total, warns = write_sample_zip(ix, ["CR_Teste", "NaoExiste"], t / "s.zip", anims=3)
            names = zipfile.ZipFile(t / "s.zip").namelist()
            self.assertTrue(any(x.endswith("CR_Teste.b3d") for x in names))
            self.assertIn("LEIA-ME.txt", names)
            self.assertTrue(any("NaoExiste" in w for w in warns))
            self.assertGreaterEqual(n, 1)


class B3dMatCommandTests(unittest.TestCase):
    def test_b3dmat_reads_retail(self):
        import subprocess, sys, tempfile
        from pathlib import Path
        from f3vb.extractor import ExtractOptions, extract_all
        from tests.test_extractor import make_game
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            game, _ = make_game(t)
            out = t / "oa"
            extract_all(game, out, ExtractOptions())
            r = subprocess.run([sys.executable, "-m", "f3vb", "b3dmat", "CR_Teste", "--assets", str(out), "--out", str(t / "r.txt")],
                               capture_output=True, text=True, cwd=str(Path(__file__).resolve().parents[1]))
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("MODELO CR_Teste", r.stdout)
            self.assertIn("superficie #0", r.stdout)
            self.assertTrue((t / "r.txt").exists())


class NoAssetsGuardTests(unittest.TestCase):
    def _vsa(self):
        import importlib.util
        from pathlib import Path
        p = Path(__file__).resolve().parents[3] / "scripts" / "verificar_sem_assets.py"
        spec = importlib.util.spec_from_file_location("vsa", p)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod

    def test_project_itself_is_clean(self):
        self.assertEqual(self._vsa().varrer(), [])

    def test_detects_game_data(self):
        import tempfile
        from pathlib import Path
        vsa = self._vsa()
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            (t / "src").mkdir()
            (t / "src" / "ok.cpp").write_text("int x;")
            (t / "tools").mkdir()
            (t / "tools" / "modelo.f3m").write_bytes(b"F3M")
            (t / "original_assets" / "RETAIL").mkdir(parents=True)
            (t / "original_assets" / ".gitignore").write_text("*\n")            # arquivo de protecao: permitido
            (t / "original_assets" / "RETAIL" / "x.b3d").write_bytes(b"B3D ")
            (t / "private").mkdir()
            (t / "private" / "a.txt").write_text("x")
            achados = " | ".join(vsa.varrer(t))
            self.assertIn("modelo.f3m", achados)
            self.assertIn("x.b3d", achados)
            self.assertIn("a.txt", achados)
            self.assertNotIn("ok.cpp", achados)
            self.assertNotIn("original_assets/.gitignore", achados.replace("\\", "/"))


if __name__ == "__main__":
    unittest.main()
