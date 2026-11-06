"""Construtor mínimo de GLB (glTF 2.0 binário) em Python puro, com malhas, materiais, texturas, skins e animações."""
from __future__ import annotations

import json
import math
import struct

FLOAT, USHORT, UINT, UBYTE = 5126, 5123, 5125, 5121
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963
_FMT = {FLOAT: "f", USHORT: "H", UINT: "I", UBYTE: "B"}
_SIZE = {FLOAT: 4, USHORT: 2, UINT: 4, UBYTE: 1}
_NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


class Glb:
    def __init__(self, generator: str = "F3VB exporter"):
        self.j: dict = {"asset": {"version": "2.0", "generator": generator}, "scene": 0,
                        "scenes": [{"nodes": []}], "nodes": [], "buffers": [{"byteLength": 0}]}
        self.bin = bytearray()
        self._img_cache: dict = {}

    # -- infraestrutura -------------------------------------------------
    def _list(self, key: str) -> list:
        return self.j.setdefault(key, [])

    def view(self, data: bytes, target: int | None = None) -> int:
        while len(self.bin) % 4:
            self.bin.append(0)
        v = {"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(data)}
        if target:
            v["target"] = target
        self.bin += data
        self._list("bufferViews").append(v)
        return len(self.j["bufferViews"]) - 1

    def accessor(self, values, ctype: int, atype: str, target: int | None = None, normalized: bool = False,
                 minmax: bool = False) -> int:
        n = _NCOMP[atype]
        flat = [x for v in values for x in (v if isinstance(v, (tuple, list)) else (v,))]
        if len(flat) % n:
            raise ValueError("número de componentes não bate com o tipo do accessor")
        count = len(flat) // n
        data = struct.pack(f"<{len(flat)}{_FMT[ctype]}", *flat)
        a = {"bufferView": self.view(data, target), "componentType": ctype, "count": count, "type": atype}
        if normalized:
            a["normalized"] = True
        if minmax and count:
            cols = [flat[i::n] for i in range(n)]
            a["min"] = [min(c) for c in cols]
            a["max"] = [max(c) for c in cols]
        self._list("accessors").append(a)
        return len(self.j["accessors"]) - 1

    # -- recursos -------------------------------------------------------
    def texture(self, png: bytes, key: str | None = None, name: str | None = None) -> int:
        if key and key in self._img_cache:
            return self._img_cache[key]
        img = {"bufferView": self.view(png), "mimeType": "image/png"}
        if name:
            img["name"] = name
        self._list("images").append(img)
        if "samplers" not in self.j:
            self.j["samplers"] = [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}]
        self._list("textures").append({"source": len(self.j["images"]) - 1, "sampler": 0})
        idx = len(self.j["textures"]) - 1
        if key:
            self._img_cache[key] = idx
        return idx

    def material(self, name: str, base_color=(1, 1, 1, 1), texture: int | None = None, alpha_mode: str = "OPAQUE",
                 double_sided: bool = False, extras: dict | None = None) -> int:
        pbr = {"baseColorFactor": [float(c) for c in base_color], "metallicFactor": 0.0, "roughnessFactor": 1.0}
        if texture is not None:
            pbr["baseColorTexture"] = {"index": texture, "texCoord": 0}
        m = {"name": name, "pbrMetallicRoughness": pbr, "alphaMode": alpha_mode, "doubleSided": double_sided}
        if extras:
            m["extras"] = extras
        self._list("materials").append(m)
        return len(self.j["materials"]) - 1

    def mesh(self, name: str, primitives: list, extras: dict | None = None) -> int:
        m = {"name": name, "primitives": primitives}
        if extras:
            m["extras"] = extras
        self._list("meshes").append(m)
        return len(self.j["meshes"]) - 1

    def node(self, name: str, *, mesh: int | None = None, skin: int | None = None, translation=None, rotation=None,
             scale=None, children: list | None = None, extras: dict | None = None, root: bool = False) -> int:
        n: dict = {"name": name}
        if mesh is not None:
            n["mesh"] = mesh
        if skin is not None:
            n["skin"] = skin
        if translation is not None:
            n["translation"] = [float(x) for x in translation]
        if rotation is not None:
            n["rotation"] = [float(x) for x in rotation]
        if scale is not None:
            n["scale"] = [float(x) for x in scale]
        if children:
            n["children"] = children
        if extras:
            n["extras"] = extras
        self.j["nodes"].append(n)
        idx = len(self.j["nodes"]) - 1
        if root:
            self.j["scenes"][0]["nodes"].append(idx)
        return idx

    def skin(self, name: str, joints: list, ibm: list, skeleton: int | None = None) -> int:
        s = {"name": name, "joints": joints, "inverseBindMatrices": self.accessor(ibm, FLOAT, "MAT4")}
        if skeleton is not None:
            s["skeleton"] = skeleton
        self._list("skins").append(s)
        return len(self.j["skins"]) - 1

    def animation(self, name: str, channels: list) -> int:
        """channels: [(node, 'translation'|'rotation'|'scale', times, values)]"""
        samplers, chans = [], []
        for node, path, times, values in channels:
            tin = self.accessor(times, FLOAT, "SCALAR", minmax=True)
            tout = self.accessor(values, FLOAT, "VEC4" if path == "rotation" else "VEC3")
            samplers.append({"input": tin, "output": tout, "interpolation": "LINEAR"})
            chans.append({"sampler": len(samplers) - 1, "target": {"node": node, "path": path}})
        self._list("animations").append({"name": name, "samplers": samplers, "channels": chans})
        return len(self.j["animations"]) - 1

    # -- saída ----------------------------------------------------------
    def to_bytes(self) -> bytes:
        self.j["buffers"][0]["byteLength"] = len(self.bin)
        body = json.dumps(self.j, separators=(",", ":"), ensure_ascii=True).encode()
        body += b" " * (-len(body) % 4)
        binary = bytes(self.bin) + b"\x00" * (-len(self.bin) % 4)
        total = 12 + 8 + len(body) + (8 + len(binary) if binary else 0)
        out = struct.pack("<4sII", b"glTF", 2, total) + struct.pack("<I4s", len(body), b"JSON") + body
        if binary:
            out += struct.pack("<I4s", len(binary), b"BIN\x00") + binary
        return out


# ---- utilitários de matemática (4x4 em COLUNA-major, como o glTF) -----------------------------------------
def quat_normalize(q):
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    return tuple(c / n for c in q)


def read_glb(data: bytes) -> tuple[dict, bytes]:
    """Leitor mínimo, usado nos testes: devolve (json, bin)."""
    magic, ver, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or ver != 2 or length != len(data):
        raise ValueError("cabeçalho GLB inválido")
    pos, js, bn = 12, None, b""
    while pos < length:
        clen, ctype = struct.unpack_from("<I4s", data, pos)
        chunk = data[pos + 8:pos + 8 + clen]
        if ctype == b"JSON":
            js = json.loads(chunk.decode())
        elif ctype == b"BIN\x00":
            bn = chunk
        pos += 8 + clen
    return js, bn


def read_accessor(js: dict, bn: bytes, idx: int) -> list:
    a = js["accessors"][idx]
    v = js["bufferViews"][a["bufferView"]]
    n = _NCOMP[a["type"]]
    fmt = _FMT[a["componentType"]]
    raw = struct.unpack_from(f"<{a['count'] * n}{fmt}", bn, v["byteOffset"] + a.get("byteOffset", 0))
    return [tuple(raw[i * n:(i + 1) * n]) if n > 1 else raw[i] for i in range(a["count"])]


def validate_glb(data: bytes) -> list[str]:
    """Checagens estruturais básicas (sem depender de validador externo). Devolve a lista de problemas."""
    problems: list[str] = []
    try:
        js, bn = read_glb(data)
    except Exception as exc:
        return [f"GLB ilegível: {exc}"]
    if js.get("asset", {}).get("version") != "2.0":
        problems.append("asset.version != 2.0")
    nb = len(js.get("bufferViews", []))
    for i, v in enumerate(js.get("bufferViews", [])):
        if v["byteOffset"] + v["byteLength"] > len(bn):
            problems.append(f"bufferView {i} fora do buffer")
        if v["byteOffset"] % 4:
            problems.append(f"bufferView {i} desalinhado")
    for i, a in enumerate(js.get("accessors", [])):
        if not 0 <= a["bufferView"] < nb:
            problems.append(f"accessor {i}: bufferView inválido")
            continue
        v = js["bufferViews"][a["bufferView"]]
        need = a["count"] * _NCOMP[a["type"]] * _SIZE[a["componentType"]]
        if a.get("byteOffset", 0) + need > v["byteLength"]:
            problems.append(f"accessor {i}: leitura passa do bufferView")
        if a["count"] == 0:
            problems.append(f"accessor {i}: count 0")
        if "min" in a and len(a["min"]) != _NCOMP[a["type"]]:
            problems.append(f"accessor {i}: min/max com tamanho errado")
    nn = len(js.get("nodes", []))
    for i, n in enumerate(js.get("nodes", [])):
        for c in n.get("children", []):
            if not 0 <= c < nn:
                problems.append(f"node {i}: filho {c} inválido")
        if "mesh" in n and not 0 <= n["mesh"] < len(js.get("meshes", [])):
            problems.append(f"node {i}: mesh inválido")
        if "skin" in n and not 0 <= n["skin"] < len(js.get("skins", [])):
            problems.append(f"node {i}: skin inválido")
        if "rotation" in n and abs(math.sqrt(sum(c * c for c in n["rotation"])) - 1) > 1e-3:
            problems.append(f"node {i}: quaternion não normalizado")
    for mi, m in enumerate(js.get("meshes", [])):
        for pi, p in enumerate(m["primitives"]):
            for k, ai in p["attributes"].items():
                if not 0 <= ai < len(js.get("accessors", [])):
                    problems.append(f"mesh {mi} prim {pi}: atributo {k} inválido")
            if "indices" in p:
                idx = read_accessor(js, bn, p["indices"])
                nvert = js["accessors"][p["attributes"]["POSITION"]]["count"]
                if idx and max(idx) >= nvert:
                    problems.append(f"mesh {mi} prim {pi}: índice {max(idx)} >= {nvert} vértices")
                if len(idx) % 3:
                    problems.append(f"mesh {mi} prim {pi}: índices não múltiplos de 3")
            if "material" in p and not 0 <= p["material"] < len(js.get("materials", [])):
                problems.append(f"mesh {mi} prim {pi}: material inválido")
    for si, s in enumerate(js.get("skins", [])):
        ibm = js["accessors"][s["inverseBindMatrices"]]
        if ibm["count"] != len(s["joints"]):
            problems.append(f"skin {si}: {ibm['count']} IBMs para {len(s['joints'])} joints")
        for jn in s["joints"]:
            if not 0 <= jn < nn:
                problems.append(f"skin {si}: joint {jn} inválido")
    for ai, an in enumerate(js.get("animations", [])):
        for c in an["channels"]:
            s = an["samplers"][c["sampler"]]
            tin = read_accessor(js, bn, s["input"])
            if any(b <= a for a, b in zip(tin, tin[1:])):
                problems.append(f"animação {ai}: tempos não crescentes")
            if js["accessors"][s["output"]]["count"] != len(tin):
                problems.append(f"animação {ai}: saídas != entradas")
    return problems
