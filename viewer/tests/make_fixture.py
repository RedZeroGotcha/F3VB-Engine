"""Gera arquivos F3M sintéticos + valores esperados (skinning de referência em Python) para o teste do núcleo C++.
Uso: python make_fixture.py PASTA_DE_SAIDA"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "py"))
from f3vb.f3m import read_f3m, skin_positions, write_f3m   # noqa: E402


def quat_z(deg):
    a = math.radians(deg) / 2
    return (0.0, 0.0, math.sin(a), math.cos(a))


def quat_x(deg):
    a = math.radians(deg) / 2
    return (math.sin(a), 0.0, 0.0, math.cos(a))


def ibm_translation(tx, ty, tz):
    return (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -tx, -ty, -tz, 1)


def scene(with_mesh=True, with_skeleton=True, with_clips=True):
    bones = ["Root", "Bone01", "Bone02"]
    sc = {"name": "fixture", "textures": [], "materials": [], "meshes": [], "joints": [], "clips": [], "bone_names": bones}
    if with_mesh:
        sc["textures"] = [{"name": "tex", "w": 3, "h": 5, "rgba": bytes([200, 100, 50, 255]) * 15}]
        sc["materials"] = [{"name": "mat", "texture": 0, "color": (1, 1, 1, 1), "alpha_mode": 0, "double_sided": 0}]
        pos = [(-0.1, 0.0, 0.0), (0.1, 0.0, 0.0), (-0.1, 1.0, 0.0), (0.1, 1.0, 0.0), (0.1, 1.5, 0.0), (-0.1, 2.0, 0.0)]
        sc["meshes"] = [{"name": "m", "material": 0, "pos": pos, "nor": [(0, 0, 1)] * 6, "uv": [(0, 0)] * 6, "col": [(1, 1, 1, 1)] * 6,
                         "joints": [(0, 0, 0, 0), (0, 0, 0, 0), (1, 0, 0, 0), (1, 0, 0, 0), (1, 2, 0, 0), (2, 0, 0, 0)],
                         "weights": [(1, 0, 0, 0), (1, 0, 0, 0), (1, 0, 0, 0), (1, 0, 0, 0), (0.5, 0.5, 0, 0), (1, 0, 0, 0)],
                         "idx": [0, 1, 2, 1, 3, 2, 2, 3, 4, 2, 4, 5]}]
    if with_skeleton:
        sc["joints"] = [
            {"name": "Root", "parent": -1, "t": (0, 0, 0), "q": (0, 0, 0, 1), "s": (1, 1, 1), "ibm": ibm_translation(0, 0, 0)},
            {"name": "Bone01", "parent": 0, "t": (0, 1, 0), "q": (0, 0, 0, 1), "s": (1, 1, 1), "ibm": ibm_translation(0, 1, 0)},
            {"name": "Bone02", "parent": 1, "t": (0, 1, 0), "q": (0, 0, 0, 1), "s": (1, 1, 1), "ibm": ibm_translation(0, 2, 0)}]
    if with_clips:
        sc["clips"] = [{"name": "bend", "duration": 1.0, "channels": [
            ("Bone01", 1, [0.0, 0.5, 1.0], [quat_z(0), quat_z(45), quat_z(90)]),
            ("Bone02", 1, [0.0, 1.0], [quat_x(0), quat_x(90)]),
            ("Root", 0, [0.0, 1.0], [(0, 0, 0), (1, 0, 0)]),
            ("Bone02", 2, [0.0, 1.0], [(1, 1, 1), (1, 2, 1)]),
            ("OssoInexistente", 0, [0.0], [(0, 0, 0)])]}]
    return sc


def main(out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "model_full.f3m").write_bytes(write_f3m(scene()))
    (out / "model_only.f3m").write_bytes(write_f3m(scene(with_skeleton=False, with_clips=False)))
    (out / "addon.f3m").write_bytes(write_f3m(scene(with_mesh=False)))
    ref = read_f3m((out / "model_full.f3m").read_bytes())          # valores como float32 (iguais aos que o C++ lê)
    lines = []
    for clip_name, clip in [("-", None)] + [(c["name"], c) for c in ref["clips"]]:
        for t in ([0.0] if clip is None else [0.0, 0.25, 0.5, 0.75, 1.0, 1.5]):
            pts = skin_positions(ref, 0, clip, t)
            lines.append(f"E {clip_name} {t} {len(pts)} " + " ".join(f"{c:.7f}" for p in pts for c in p))
    (out / "expected.txt").write_text("\n".join(lines) + "\n")
    print(f"fixture em {out}: {len(lines)} casos")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "fixture")
