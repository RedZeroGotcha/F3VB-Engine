"""Formato F3M ("F3VB Model"): contêiner simples, sem JSON e sem compressão, feito para o visualizador C++ ler fácil.

Little-endian. Um mesmo formato serve para: modelo (malhas + texturas + esqueleto opcional), esqueleto + clipes, e só clipes.
As malhas referenciam ossos por NOME (tabela `bones` da própria malha), os clipes também: assim animações e esqueletos
podem ser adicionados depois ao modelo, como o visualizador faz.

  char[4] "F3M1" ; u32 flags (1=modelo, 2=esqueleto, 4=clipes)
  u32 n_tex, n_mat, n_mesh, n_joint, n_clip ; str nome
  textura : str nome ; u32 w, h ; u8 rgba[w*h*4]
  material: str nome ; i32 textura(-1) ; f32 cor[4] ; u8 modo(0 opaco, 1 mistura, 2 aditivo) ; u8 dupla_face ; u8[2]
  malha   : str nome ; i32 material ; u32 nv, ni, nbones ; str bones[nbones] ; u8 tem_skin ; u8[3]
            f32 pos[nv*3], nrm[nv*3], uv[nv*2], cor[nv*4] ; (skin) u16 juntas[nv*4], f32 pesos[nv*4] ; u32 idx[ni]
  junta   : str nome ; i32 pai ; f32 t[3], q[4], s[3] ; f32 ibm[16] (coluna-major, como no glTF)
  clipe   : str nome ; f32 duracao ; u32 n_canais ; canal: str osso ; u8 caminho(0 T, 1 R, 2 S) ; u8[3] ; u32 n ; f32 tempos[n], valores[n*(3|4)]
  str = u16 tamanho + bytes (latin-1)
Espaço: mão-direita, Y para cima (já convertido pelo exportador).
"""
from __future__ import annotations

import math
import struct

MAGIC = b"F3M1"
FLAG_MODEL, FLAG_SKELETON, FLAG_CLIPS = 1, 2, 4


class F3MError(ValueError):
    pass


def _s(text: str) -> bytes:
    b = text.encode("latin-1", "replace")[:65535]
    return struct.pack("<H", len(b)) + b


def write_f3m(sc: dict) -> bytes:
    flags = (FLAG_MODEL if sc["meshes"] else 0) | (FLAG_SKELETON if sc["joints"] else 0) | (FLAG_CLIPS if sc["clips"] else 0)
    out = bytearray(MAGIC)
    out += struct.pack("<I5I", flags, len(sc["textures"]), len(sc["materials"]), len(sc["meshes"]), len(sc["joints"]), len(sc["clips"]))
    out += _s(sc.get("name", ""))
    for t in sc["textures"]:
        out += _s(t["name"]) + struct.pack("<II", t["w"], t["h"]) + t["rgba"]
    for m in sc["materials"]:
        tex = -1 if m["texture"] is None else m["texture"]
        out += _s(m["name"]) + struct.pack("<i4fBBxx", tex, *m["color"], m["alpha_mode"], m["double_sided"])
    for me in sc["meshes"]:
        nv, ni = len(me["pos"]), len(me["idx"])
        bones = sc["bone_names"] if me["joints"] is not None else []
        mat = -1 if me["material"] is None else me["material"]
        out += _s(me["name"]) + struct.pack("<iIII", mat, nv, ni, len(bones))
        for b in bones:
            out += _s(b)
        out += struct.pack("<Bxxx", 1 if me["joints"] is not None else 0)
        out += struct.pack(f"<{nv * 3}f", *[c for v in me["pos"] for c in v])
        out += struct.pack(f"<{nv * 3}f", *[c for v in me["nor"] for c in v])
        out += struct.pack(f"<{nv * 2}f", *[c for v in me["uv"] for c in v])
        out += struct.pack(f"<{nv * 4}f", *[c for v in me["col"] for c in v])
        if me["joints"] is not None:
            out += struct.pack(f"<{nv * 4}H", *[c for v in me["joints"] for c in v])
            out += struct.pack(f"<{nv * 4}f", *[c for v in me["weights"] for c in v])
        out += struct.pack(f"<{ni}I", *me["idx"])
    for j in sc["joints"]:
        out += _s(j["name"]) + struct.pack("<i3f4f3f16f", j["parent"], *j["t"], *j["q"], *j["s"], *j["ibm"])
    for c in sc["clips"]:
        out += _s(c["name"]) + struct.pack("<fI", c["duration"], len(c["channels"]))
        for bone, path, times, values in c["channels"]:
            n = len(times)
            out += _s(bone) + struct.pack("<BxxxI", path, n) + struct.pack(f"<{n}f", *times)
            out += struct.pack(f"<{n * (4 if path == 1 else 3)}f", *[x for v in values for x in v])
    return bytes(out)


class _R:
    def __init__(self, d: bytes):
        self.d, self.p = d, 0

    def take(self, n: int) -> bytes:
        if n < 0 or self.p + n > len(self.d):
            raise F3MError(f"arquivo truncado em {self.p}")
        b = self.d[self.p:self.p + n]
        self.p += n
        return b

    def unpack(self, fmt: str):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))

    def s(self) -> str:
        (n,) = self.unpack("<H")
        return self.take(n).decode("latin-1")

    def floats(self, n: int):
        return list(self.unpack(f"<{n}f")) if n else []


def read_f3m(data: bytes) -> dict:
    """Leitor de referência (usado nos testes e na validação cruzada com o C++)."""
    r = _R(data)
    if r.take(4) != MAGIC:
        raise F3MError("não é um F3M1")
    flags, nt, nm, nme, nj, nc = r.unpack("<I5I")
    sc = {"flags": flags, "name": r.s(), "textures": [], "materials": [], "meshes": [], "joints": [], "clips": []}
    for _ in range(nt):
        name = r.s(); w, h = r.unpack("<II")
        if w * h > 1 << 26:
            raise F3MError("textura grande demais")
        sc["textures"].append({"name": name, "w": w, "h": h, "rgba": r.take(w * h * 4)})
    for _ in range(nm):
        name = r.s(); tex, *col, mode, dbl = r.unpack("<i4fBBxx")
        sc["materials"].append({"name": name, "texture": None if tex < 0 else tex, "color": tuple(col), "alpha_mode": mode, "double_sided": dbl})
    for _ in range(nme):
        name = r.s(); mat, nv, ni, nb = r.unpack("<iIII")
        bones = [r.s() for _ in range(nb)]
        (skin,) = r.unpack("<Bxxx")
        f = r.floats
        pos = f(nv * 3); nor = f(nv * 3); uv = f(nv * 2); col = f(nv * 4)
        me = {"name": name, "material": None if mat < 0 else mat, "bones": bones,
              "pos": [tuple(pos[i * 3:i * 3 + 3]) for i in range(nv)], "nor": [tuple(nor[i * 3:i * 3 + 3]) for i in range(nv)],
              "uv": [tuple(uv[i * 2:i * 2 + 2]) for i in range(nv)], "col": [tuple(col[i * 4:i * 4 + 4]) for i in range(nv)],
              "joints": None, "weights": None}
        if skin:
            jn = r.unpack(f"<{nv * 4}H"); w = f(nv * 4)
            me["joints"] = [tuple(jn[i * 4:i * 4 + 4]) for i in range(nv)]
            me["weights"] = [tuple(w[i * 4:i * 4 + 4]) for i in range(nv)]
        me["idx"] = list(r.unpack(f"<{ni}I")) if ni else []
        sc["meshes"].append(me)
    for _ in range(nj):
        name = r.s(); parent, *rest = r.unpack("<i3f4f3f16f")
        sc["joints"].append({"name": name, "parent": parent, "t": tuple(rest[0:3]), "q": tuple(rest[3:7]), "s": tuple(rest[7:10]), "ibm": tuple(rest[10:26])})
    for _ in range(nc):
        name = r.s(); dur, nch = r.unpack("<fI")
        chans = []
        for _ in range(nch):
            bone = r.s(); path, n = r.unpack("<BxxxI")
            times = r.floats(n); k = 4 if path == 1 else 3
            vals = r.floats(n * k)
            chans.append((bone, path, times, [tuple(vals[i * k:(i + 1) * k]) for i in range(n)]))
        sc["clips"].append({"name": name, "duration": dur, "channels": chans})
    if r.p != len(data):
        raise F3MError(f"{len(data) - r.p} bytes sobrando")
    return sc


# ----------------------------------------------------------------------------- referência de skinning (puro Python)
def _quat_mat(q):
    x, y, z, w = q
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    x, y, z, w = x / n, y / n, z / n, w / n
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]


def _mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def _trs(t, q, s):
    r = _quat_mat(q)
    m = [[r[i][j] * s[j] for j in range(3)] + [t[i]] for i in range(3)] + [[0, 0, 0, 1]]
    return m


def _from_colmajor(m16):
    return [[m16[c * 4 + r] for c in range(4)] for r in range(4)]


def _slerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b = tuple(-x for x in b); d = -d
    if d > 0.9995:
        q = tuple(x + t * (y - x) for x, y in zip(a, b))
    else:
        th = math.acos(min(1.0, d)); s = math.sin(th)
        wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
        q = tuple(wa * x + wb * y for x, y in zip(a, b))
    n = math.sqrt(sum(x * x for x in q)) or 1.0
    return tuple(x / n for x in q)


def _sample(times, values, t, rot):
    if t <= times[0] or len(times) == 1:
        return values[0]
    if t >= times[-1]:
        return values[-1]
    for i in range(len(times) - 1):
        if times[i] <= t <= times[i + 1]:
            u = (t - times[i]) / (times[i + 1] - times[i]) if times[i + 1] > times[i] else 0.0
            a, b = values[i], values[i + 1]
            return _slerp(a, b, u) if rot else tuple(x + u * (y - x) for x, y in zip(a, b))
    return values[-1]


def pose_matrices(sc: dict, clip: dict | None, t: float):
    """Matrizes de mundo das juntas (listas 4x4 linha-a-linha) para o clipe em t (None = pose de bind)."""
    joints = sc["joints"]
    chan = {}
    for bone, path, times, values in (clip["channels"] if clip else []):
        chan[(bone, path)] = (times, values)
    world = []
    for j in joints:
        tr, q, s = j["t"], j["q"], j["s"]
        if (j["name"], 0) in chan:
            tr = _sample(*chan[(j["name"], 0)], t, False)
        if (j["name"], 1) in chan:
            q = _sample(*chan[(j["name"], 1)], t, True)
        if (j["name"], 2) in chan:
            s = _sample(*chan[(j["name"], 2)], t, False)
        local = _trs(tr, q, s)
        world.append(local if j["parent"] < 0 else _mul(world[j["parent"]], local))
    return world


def skin_positions(sc: dict, mesh_index: int, clip: dict | None, t: float):
    me = sc["meshes"][mesh_index]
    if me["joints"] is None or not sc["joints"]:
        return [tuple(p) for p in me["pos"]]
    name_to_joint = {j["name"]: i for i, j in enumerate(sc["joints"])}
    world = pose_matrices(sc, clip, t)
    jm = [_mul(world[i], _from_colmajor(sc["joints"][i]["ibm"])) for i in range(len(world))]
    out = []
    for v, p in enumerate(me["pos"]):
        acc = [0.0, 0.0, 0.0]
        for k in range(4):
            w = me["weights"][v][k]
            if w == 0:
                continue
            bone = me["bones"][me["joints"][v][k]] if me["joints"][v][k] < len(me["bones"]) else None
            ji = name_to_joint.get(bone)
            if ji is None:
                continue
            m = jm[ji]
            for r in range(3):
                acc[r] += w * (m[r][0] * p[0] + m[r][1] * p[1] + m[r][2] * p[2] + m[r][3])
        out.append(tuple(acc))
    return out
