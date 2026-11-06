"""Leitor completo de .b3d (modelos da Jefferson Engine): geometria, materiais, ossos e pesos.

Layouts OBSERVADOS e validados em arquivos reais (ver docs/DECISAO_ABORDAGEM.md):
 - vértice de 44 bytes: pos 3f, normal 3f, cor RGBA 4B, uv 2f, 2 x s32 (zeros); os 6 floats depois dos vértices
   são a caixa envolvente exata das posições (min xyz, max xyz) -> validação automática.
 - vértice de 64 bytes (terreno): pos 3f, normal 3f, cor RGBA 4f, s32, uv0 2f, uv1 (lightmap) 2f, 1 f de preenchimento.
   Neste caso o cabeçalho da superfície NÃO tem o campo sizeOfVerts: os vértices começam logo após numVerts.
 - pesos de osso: bonesPerVertex x numVerts pares (s32 osso, f32 peso), organizados POR SLOT (slot-major):
   influência k do vértice v = par[k * numVerts + v]; osso -1 = vazio. O índice refere-se à lista de ossos do topo.
Quase tudo é HIPÓTESE fora das amostras testadas; erros de layout viram B3DError com offset.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field


class B3DError(ValueError):
    pass


class _R:
    def __init__(self, d: bytes, p: int = 0):
        self.d, self.p = d, p

    def need(self, n: int):
        if n < 0 or self.p + n > len(self.d):
            raise B3DError(f"fim do arquivo em {self.p} (precisava de {n} bytes)")

    def u8(self):
        self.need(1); v = self.d[self.p]; self.p += 1; return v

    def u16(self):
        self.need(2); v = struct.unpack_from("<H", self.d, self.p)[0]; self.p += 2; return v

    def u32(self):
        self.need(4); v = struct.unpack_from("<I", self.d, self.p)[0]; self.p += 4; return v

    def s32(self):
        self.need(4); v = struct.unpack_from("<i", self.d, self.p)[0]; self.p += 4; return v

    def f32(self):
        self.need(4); v = struct.unpack_from("<f", self.d, self.p)[0]; self.p += 4; return v

    def floats(self, n: int):
        self.need(4 * n); v = struct.unpack_from(f"<{n}f", self.d, self.p); self.p += 4 * n; return list(v)

    def str16(self, limit: int = 4096) -> str:
        n = self.u16()
        if n > limit:
            raise B3DError(f"string de {n} bytes em {self.p - 2}")
        self.need(n)
        s = self.d[self.p:self.p + n].decode("latin-1"); self.p += n
        return s


@dataclass
class Material:
    name: str = ""
    id: str = ""
    floats: list = field(default_factory=list)      # D3D: difusa4, ambiente4, especular4, emissiva4, brilho
    blend: str = ""
    type: str = ""
    textures: list = field(default_factory=list)


@dataclass
class Surface:
    family: str                                     # "44" | "64"
    positions: list = field(default_factory=list)
    normals: list = field(default_factory=list)
    colors: list = field(default_factory=list)      # RGBA 0..1
    uv0: list = field(default_factory=list)
    uv1: list = field(default_factory=list)         # lightmap (só família 64)
    bbox: list = field(default_factory=list)
    influences: list = field(default_factory=list)  # por vértice: [(osso, peso), ...]
    bones_per_vertex: int = 0
    materials: list = field(default_factory=list)
    material_names: list = field(default_factory=list)
    lightmap: str | None = None
    primitives: list = field(default_factory=list)  # [{"material": i, "indices": [...]}]


@dataclass
class Node:
    name: str = ""
    transform: dict | None = None
    surfaces: list = field(default_factory=list)
    children: list = field(default_factory=list)
    zones: list = field(default_factory=list)


@dataclass
class Bone:
    name: str
    unk: int
    flag: int
    translation: list | None = None
    rotation: list | None = None
    extra: float | None = None


@dataclass
class Model:
    signature: str = ""
    globals: dict = field(default_factory=dict)
    materials: list = field(default_factory=list)
    textures: list = field(default_factory=list)
    bones: list = field(default_factory=list)
    roots: list = field(default_factory=list)

    def surfaces(self):
        def walk(n):
            for s in n.surfaces:
                yield n, s
            for c in n.children:
                yield from walk(c)
        for r in self.roots:
            yield from walk(r)


def _material_top(r: _R) -> Material:
    m = Material(id=r.str16(), name=r.str16(), floats=r.floats(18))
    m.blend, m.type = r.str16(), r.str16()
    r.s32(); r.s32()
    return m


def _material44(r: _R) -> Material:
    m = Material(name=r.str16(), id=r.str16(), floats=r.floats(17))
    m.blend, m.type = r.str16(), r.str16()
    words = [r.s32() for _ in range(16)]
    n_tex = words[15]
    if not 0 <= n_tex <= 8:
        raise B3DError(f"contagem de texturas absurda ({n_tex}) em {r.p}")
    m.textures = [r.str16() for _ in range(n_tex)]
    r.u32()
    return m


def _surface(r: _R) -> Surface:
    start = r.p
    r.u8(); r.u8()
    vflag = r.u8()
    nv = r.s32()
    if not 0 <= nv <= 2_000_000:
        raise B3DError(f"numVerts absurdo ({nv}) na superfície em {start}")
    if vflag == 1:
        r.s32()                                      # sizeOfVerts (= 44)
        r.need(nv * 44)
        s = Surface("44")
        raw = struct.unpack_from(f"<{'3f3f4B2f2i' * nv}", r.d, r.p) if nv else ()
        for v in range(nv):
            x = raw[v * 14:(v + 1) * 14]
            s.positions.append(tuple(x[0:3])); s.normals.append(tuple(x[3:6]))
            s.colors.append(tuple(c / 255.0 for c in x[6:10])); s.uv0.append((x[10], x[11]))
        r.p += nv * 44
        s.bbox = r.floats(6)
        if nv and any(abs(a - b) > 1e-4 for a, b in zip(s.bbox, [min(p[i] for p in s.positions) for i in range(3)]
                                                         + [max(p[i] for p in s.positions) for i in range(3)])):
            raise B3DError(f"caixa envolvente não confere com as posições (superfície em {start})")
        is_bones = r.u8()
        if is_bones == 1:
            bpv = r.s32()
            if not 0 <= bpv <= 16:
                raise B3DError(f"bonesPerVertex absurdo ({bpv})")
            r.need(nv * bpv * 8)
            pairs = struct.unpack_from(f"<{'if' * (nv * bpv)}", r.d, r.p) if nv * bpv else ()
            r.p += nv * bpv * 8
            s.bones_per_vertex = bpv
            for v in range(nv):
                infl = []
                for k in range(bpv):
                    bi, bw = pairs[2 * (k * nv + v)], pairs[2 * (k * nv + v) + 1]
                    if bi >= 0 and bw > 0:
                        infl.append((bi, bw))
                s.influences.append(infl)
        nm = r.s32()
        if not 0 <= nm <= 64:
            raise B3DError(f"numMaterials absurdo ({nm})")
        s.materials = [_material44(r) for _ in range(nm)]
        s.material_names = [r.str16() for _ in range(nm)]
        mat_list = [r.s32() for _ in range(nm)]
        face_counts = [r.s32() for _ in range(nm)]
        for mi, n_idx in enumerate(mat_list):
            if n_idx < 0 or n_idx % 3:
                raise B3DError(f"lista de índices inválida ({n_idx})")
            r.need(2 * n_idx)
            idx = list(struct.unpack_from(f"<{n_idx}H", r.d, r.p)) if n_idx else []
            r.p += 2 * n_idx
            if idx and max(idx) >= max(nv, 1):
                raise B3DError("índice de vértice fora do intervalo")
            s.primitives.append({"material": mi, "indices": idx, "faces": face_counts[mi]})
        return s
    # família 64 (terreno): sem sizeOfVerts
    r.need(nv * 64)
    s = Surface("64")
    raw = struct.unpack_from(f"<{'3f3f4fi2f2ff' * nv}", r.d, r.p) if nv else ()
    for v in range(nv):
        x = raw[v * 16:(v + 1) * 16]
        s.positions.append(tuple(x[0:3])); s.normals.append(tuple(x[3:6])); s.colors.append(tuple(x[6:10]))
        s.uv0.append((x[11], x[12])); s.uv1.append((x[13], x[14]))
    r.p += nv * 64
    r.u32()
    r.u32()
    s.lightmap = r.str16()
    n_tex = r.s32()
    if not 0 <= n_tex <= 8:
        raise B3DError(f"família 64: contagem de texturas absurda ({n_tex})")
    tex = [r.str16() for _ in range(n_tex)]
    s.materials = [Material(name=s.lightmap, id="", blend="OPAQUE", type="", textures=tex)]
    r.u32(); r.u8()
    kk = r.s32()
    if kk < 0 or kk % 3 or r.p + kk * 5 > len(r.d):
        raise B3DError(f"família 64: kk={kk} inválido")
    idx = list(struct.unpack_from(f"<{kk}i", r.d, r.p)) if kk else []
    r.p += kk * 4
    if idx and (min(idx) < 0 or max(idx) >= max(nv, 1)):
        raise B3DError("família 64: índice de vértice fora do intervalo")
    r.p += kk
    s.primitives.append({"material": 0, "indices": idx, "faces": kk // 3})
    return s


def _node(r: _R, depth: int = 0) -> Node:
    n = Node()
    while True:
        if r.p >= len(r.d):
            raise B3DError("arquivo terminou dentro de um nó")
        t = r.u8()
        if t == 0x0B:
            return n
        if t == 0x0C:
            n.name = r.str16()
        elif t == 0x0D:
            r.str16()
            fl = r.u8()
            tr = {"flags": fl}
            if fl & 1:
                tr["translation"] = r.floats(3)
            if fl & 2:
                tr["rotation"] = r.floats(4)
            if fl & 4:
                tr["scale"] = r.floats(3)
            n.transform = tr
        elif t in (0x0F, 0x19):
            n.surfaces.append(_surface(r))
        elif t == 0x1B:
            n.zones = [r.str16() for _ in range(r.s32())]
        elif t in (0x0A, 0x13):
            if depth > 32:
                raise B3DError("nós aninhados demais")
            n.children.append(_node(r, depth + 1))
        else:
            raise B3DError(f"token de nó desconhecido {t:#04x} em {r.p - 1}")


def read_b3d(data: bytes) -> Model:
    if data[:4] != b"B3D ":
        raise B3DError("não começa com 'B3D '")
    m = Model(signature=data[:8].decode("latin-1", "replace"))
    r = _R(data, 8)
    while r.p < len(data):
        t = r.u8()
        if t == 0x01:
            return m
        if t == 0x03:
            m.globals["global_scale"] = r.f32()
        elif t == 0x04:
            m.globals["coordinate_scale"] = r.f32()
        elif t == 0x05:
            m.globals["coordinate_system"] = r.str16()
        elif t == 0x06:
            m.globals["token06"] = [r.s32(), r.s32(), r.s32()]
        elif t == 0x16:
            m.globals["token16"] = r.s32()
        elif t == 0x1C:
            m.globals["version_flag"] = r.u8()
        elif t == 0x35:
            m.globals["legacy_v35"] = r.u8()
        elif t in (0x07, 0x0F, 0x10):
            m.materials.append(_material_top(r))
        elif t in (0x08, 0x09):
            r.u8()
            name, fname = r.str16(), r.str16()
            m.textures.append({"name": name, "file": fname, "width": r.u32(), "height": r.u32()})
        elif t in (0x0E, 0x18):
            b = Bone(name=r.str16(), unk=r.u16(), flag=r.u8())
            if b.flag:
                b.translation = r.floats(3)
            if b.flag == 3:
                b.rotation = r.floats(4)
            if b.flag == 2:
                b.extra = r.f32()
            m.bones.append(b)
        elif t in (0x0A, 0x13):
            m.roots.append(_node(r))
        else:
            raise B3DError(f"token de topo desconhecido {t:#04x} em {r.p - 1}")
    raise B3DError("arquivo terminou sem o token final 0x01")
