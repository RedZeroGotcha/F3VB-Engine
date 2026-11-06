"""Leitor de `.map` (recurso tipo 1300 "MAP_Map"): descricao de um mapa = lista de chunks de topo.

Enquadramento (MEDIDO nos 38 arquivos reais): cada chunk = tag[4] + version u32 + size u32 (size inclui os 12 bytes do cabecalho);
os chunks de topo cobrem o arquivo inteiro, sem sobra. Strings = u16 comprimento + bytes (ASCII). Posicoes = f32 (x, y, z), y = altura.
Cada decodificador diz quantos bytes do payload NAO explicou (`sobra`): 0 = layout confirmado por inteiro.

Tags (contagem nos 38 arquivos): EMAP 37 (cabecalho), EME2 175 (entidade colocada; contem EEOV), ECAM 37 (camera), EMNP 37 (grafo de navegacao),
EMEF 35 (efeito/VFX), EMTR 28 + EBTR 23 / ESTR 3 / ETTR 2 (gatilhos), EMSD 23 (som), EMEP 13 (pontos de entrada), EPTH 9 (caminhos),
EMNO 5 (marcadores de mapa), 2MWT 2 (agua; so parcial).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

from .chunk import ChunkError, iter_chunks

_F = struct.Struct("<f")


class MapError(ValueError):
    pass


def _u16str(p: bytes, pos: int):
    if pos + 2 > len(p):
        raise MapError(f"string cortada em {pos}")
    n, = struct.unpack_from("<H", p, pos)
    if pos + 2 + n > len(p):
        raise MapError(f"string de {n} bytes ultrapassa o payload em {pos}")
    return p[pos + 2:pos + 2 + n].decode("latin-1"), pos + 2 + n


def _vec3(p: bytes, pos: int):
    return list(struct.unpack_from("<3f", p, pos)), pos + 12


def _plausible_string_at(p: bytes, pos: int):
    """String u16 plausivel em `pos` (comprimento >= 3, so ASCII imprimivel), ou None."""
    if pos + 2 > len(p):
        return None
    n, = struct.unpack_from("<H", p, pos)
    if n < 3 or pos + 2 + n > len(p):
        return None
    raw = p[pos + 2:pos + 2 + n]
    if all(32 <= b < 127 for b in raw):
        return raw.decode("ascii"), pos + 2 + n
    return None


# ------------------------------------------------------------------ decodificadores por tag
def parse_emap(p: bytes) -> dict:
    reserved, = struct.unpack_from("<I", p, 0)
    pos = 4
    tre, pos = _u16str(p, pos)
    rle, pos = _u16str(p, pos)
    ambient, pos = _u16str(p, pos)
    tail = p[pos:]
    d = {"reserved": reserved, "tre": tre, "rle": rle, "ambient_map": ambient, "sobra": 0}
    if len(tail) == 27:
        d["u16_a"] = struct.unpack_from("<H", tail, 0)[0]
        d["color_a"] = list(tail[2:6])            # hipotese: cor de fundo (R, G, B, A?)
        d["color_b"] = list(tail[6:10])           # hipotese: cor de nevoa/ambiente (R, G, B, A)
        d["flag"] = tail[10]
        d["u32_b"] = struct.unpack_from("<I", tail, 11)[0]
        d["floats"] = list(struct.unpack_from("<3f", tail, 15))   # 250, 500, 1.0 na maioria (hipotese: nevoa perto/longe, escala)
    else:
        d["tail_hex"] = tail.hex()
        d["sobra"] = len(tail)
    return d


def parse_ecam(p: bytes) -> dict:
    if len(p) != 16:
        return {"raw_hex": p.hex(), "sobra": len(p)}
    return {"floats": list(struct.unpack("<4f", p)), "sobra": 0}   # padrao (0, 1000, 1000, 0)


def parse_emnp(p: bytes) -> dict:
    count, = struct.unpack_from("<I", p, 0)
    if len(p) != 4 + 30 * count:
        return {"count": count, "nodes": [], "sobra": len(p) - 4}
    nodes = []
    for i in range(count):
        o = 4 + 30 * i
        nodes.append({"kind": p[o], "pos": list(struct.unpack_from("<3f", p, o + 1)), "reserved": list(struct.unpack_from("<3f", p, o + 13)),
                      "links": [b for b in p[o + 25:o + 30] if b != 0xFF]})
    return {"count": count, "nodes": nodes, "sobra": 0}


def parse_eeov(p: bytes, version: int) -> dict:
    """Sobrescritas da instancia: nome, 3 flags e uma sequencia de strings e numeros. Os numeros entre as strings ainda nao tem layout."""
    inst, pos = _u16str(p, 0)
    d = {"version": version, "instance": inst, "flags": list(p[pos:pos + 3]), "strings": [], "gaps": []}
    pos += 3
    gap_start = pos
    while pos < len(p):
        s = _plausible_string_at(p, pos)
        if s is None:
            pos += 1
            continue
        if pos > gap_start:
            d["gaps"].append({"at": gap_start, "hex": p[gap_start:pos].hex()})
        text, end = s
        ext = text.rsplit(".", 1)[-1].lower() if "." in text else ""
        kind = {"amx": "script", "dds": "skin", "tga": "skin", "veg": "effect", "psf": "sound", "amo": "inventory", "wea": "inventory",
                "arm": "inventory", "itm": "inventory", "use": "inventory", "con": "inventory"}.get(ext, "text")
        d["strings"].append({"at": pos, "kind": kind, "text": text})
        pos = gap_start = end
    if gap_start < len(p):
        d["gaps"].append({"at": gap_start, "hex": p[gap_start:].hex()})
    d["script"] = next((s["text"] for s in d["strings"] if s["kind"] == "script"), None)
    d["display_name"] = next((s["text"] for s in d["strings"] if s["kind"] == "text"), None)
    d["skin"] = next((s["text"] for s in d["strings"] if s["kind"] == "skin"), None)
    d["inventory"] = [s["text"] for s in d["strings"] if s["kind"] == "inventory"]
    d["effects"] = [s["text"] for s in d["strings"] if s["kind"] == "effect"]
    d["sobra"] = 0                       # o payload e totalmente percorrido (strings + lacunas); as lacunas sao os numeros nao decifrados
    return d


def parse_eme2(p: bytes) -> dict:
    name, pos = _u16str(p, 0)
    if pos + 25 + 12 > len(p):
        return {"prototype": name, "sobra": len(p) - pos}
    x, y, z, yaw = struct.unpack_from("<4f", p, pos)
    extra = list(struct.unpack_from("<2f", p, pos + 16))
    flag = p[pos + 24]
    pos += 25
    tag, ver, size = struct.unpack_from("<4sII", p, pos)
    d = {"prototype": name, "kind": name.rsplit(".", 1)[-1].upper(), "pos": [x, y, z], "yaw": yaw, "extra": extra, "flag": flag, "sobra": 0}
    if tag != b"EEOV" or pos + size != len(p):
        d["sobra"] = len(p) - pos
        d["raw_tail_hex"] = p[pos:].hex()
        return d
    d["eeov"] = parse_eeov(p[pos + 12:pos + size], ver)
    return d


def parse_emef(p: bytes) -> dict:
    name, pos = _u16str(p, 0)
    x, y, z, yaw = struct.unpack_from("<4f", p, pos)
    extra = list(struct.unpack_from("<2f", p, pos + 16))
    flag = p[pos + 24]
    effect, pos = _u16str(p, pos + 25)
    return {"instance": name, "pos": [x, y, z], "yaw": yaw, "extra": extra, "flag": flag, "effect": effect, "trailing": p[pos:].hex(), "sobra": 0 if len(p) - pos == 1 else len(p) - pos}


def parse_emep(p: bytes) -> dict:
    if len(p) == 17:                         # variante de 1 ponto: id, posicao e angulo (Test_Mesa_2)
        x, y, z = struct.unpack_from("<3f", p, 1)
        return {"id": p[0], "positions": [[x, y, z]], "angles": [struct.unpack_from("<f", p, 13)[0]], "sobra": 0}
    if len(p) != 97:
        return {"raw_hex": p.hex(), "sobra": len(p)}
    pts = [list(struct.unpack_from("<3f", p, 1 + 12 * i)) for i in range(6)]
    ang = list(struct.unpack_from("<6f", p, 73))
    return {"id": p[0], "positions": pts, "angles": ang, "sobra": 0}   # 6 posicoes de entrada (grupo) + 6 angulos (rad)


def parse_emno(p: bytes) -> dict:
    pos, o = _vec3(p, 0)
    icon, o = _u16str(p, o)
    tid = struct.unpack_from("<I", p, o)[0] if o + 4 <= len(p) else None
    return {"pos": pos, "icon": icon, "text_id": tid, "sobra": len(p) - o - 4 if tid is not None else len(p) - o}


def parse_emtr(p: bytes) -> dict:
    kind, n = struct.unpack_from("<II", p, 0)
    if len(p) != 8 + 12 * n:
        return {"kind": kind, "count": n, "points": [], "sobra": len(p) - 8}
    return {"kind": kind, "points": [list(struct.unpack_from("<3f", p, 8 + 12 * i)) for i in range(n)], "sobra": 0}


def parse_ebtr(p: bytes) -> dict:
    return {"id": struct.unpack_from("<I", p, 0)[0], "rest_hex": p[4:].hex(), "sobra": 0}


def parse_estr(p: bytes) -> dict:
    s, o = _u16str(p, 0)
    return {"script": s, "rest_hex": p[o:].hex(), "sobra": 0}


def parse_ettr(p: bytes) -> dict:
    s, o = _u16str(p, 0)
    return {"target_map": s, "entry_point": p[o] if o < len(p) else None, "flag": p[o + 1] if o + 1 < len(p) else None, "sobra": max(0, len(p) - o - 2)}


def parse_epth(p: bytes) -> dict:
    name, o = _u16str(p, 0)
    n, = struct.unpack_from("<I", p, o)
    o += 4
    if len(p) != o + 24 * n:
        return {"name": name, "count": n, "points": [], "sobra": len(p) - o}
    pts = []
    for i in range(n):
        x, y, z, a, b, c = struct.unpack_from("<6f", p, o + 24 * i)
        pts.append({"pos": [x, y, z], "yaw": a, "extra": [b, c]})
    return {"name": name, "points": pts, "sobra": 0}


def parse_emsd(p: bytes) -> dict:
    name, o = _u16str(p, 0)
    pos, o = _vec3(p, o)
    d = {"name": name, "pos": pos, "sound": None, "flags_hex": "", "sobra": 0}
    s = _plausible_string_at(p, o)
    if s:
        d["sound"], o = s
    d["flags_hex"] = p[o:].hex()
    return d


def parse_2mwt(p: bytes) -> dict:
    """Agua (MEDIDO em 2 mapas): nome + 140 bytes de parametros (floats ainda sem nome) + u32 N + N x {vec3, string de textura .TGA, f32 u, f32 v}.
    As posicoes formam uma grade de remendos a cada 10 unidades (y = -2 ou 0); u/v deslocam a textura (multiplos de 0,125)."""
    name, o = _u16str(p, 0)
    params = p[o:o + 140]
    o += 140
    count, = struct.unpack_from("<I", p, o)
    o += 4
    patches = []
    try:
        for _ in range(count):
            pos, o = _vec3(p, o)
            tex, o = _u16str(p, o)
            u, v = struct.unpack_from("<2f", p, o)
            o += 8
            patches.append({"pos": pos, "texture": tex, "uv": [u, v]})
    except (MapError, struct.error):
        return {"name": name, "params_hex": params.hex(), "patches": patches, "sobra": len(p) - o, "erro": "remendos fora do layout"}
    return {"name": name, "params_hex": params.hex(), "params_floats": list(struct.unpack("<35f", params)) if len(params) == 140 else [],
            "patches": patches, "sobra": len(p) - o}


_PARSERS = {"EMAP": parse_emap, "ECAM": parse_ecam, "EMNP": parse_emnp, "EME2": parse_eme2, "EMEF": parse_emef, "EMEP": parse_emep,
            "EMNO": parse_emno, "EMTR": parse_emtr, "EBTR": parse_ebtr, "ESTR": parse_estr, "ETTR": parse_ettr, "EPTH": parse_epth,
            "EMSD": parse_emsd, "2MWT": parse_2mwt}


@dataclass
class MapFile:
    chunks: list = field(default_factory=list)       # (tag, version, size, dict decodificado)
    header: dict = field(default_factory=dict)
    camera: dict = field(default_factory=dict)
    entities: list = field(default_factory=list)
    effects: list = field(default_factory=list)
    entry_points: list = field(default_factory=list)
    nav_nodes: list = field(default_factory=list)
    markers: list = field(default_factory=list)
    triggers: list = field(default_factory=list)     # {"region", "binding"|"script"|"travel"}
    paths: list = field(default_factory=list)
    sounds: list = field(default_factory=list)
    water: list = field(default_factory=list)
    unknown: list = field(default_factory=list)      # tags sem decodificador
    total_bytes: int = 0
    unexplained_bytes: int = 0                       # soma das sobras dos decodificadores

    def to_dict(self) -> dict:
        return {k: getattr(self, k) for k in ("header", "camera", "entities", "effects", "entry_points", "nav_nodes", "markers", "triggers",
                                              "paths", "sounds", "water", "unknown", "total_bytes", "unexplained_bytes")}

    def coverage(self) -> dict:
        tags: dict = {}
        for tag, _v, size, d in self.chunks:
            t = tags.setdefault(tag, {"chunks": 0, "bytes": 0, "sobra": 0})
            t["chunks"] += 1
            t["bytes"] += size
            t["sobra"] += d.get("sobra", size - 12) if isinstance(d, dict) else 0
        return tags


def read_map(data: bytes) -> MapFile:
    m = MapFile(total_bytes=len(data))
    try:
        chunks = list(iter_chunks(data))
    except ChunkError as exc:
        raise MapError(f"enquadramento de chunks invalido: {exc}") from exc
    group = None
    for c in chunks:
        payload = data[c.payload_offset:c.offset + c.size]
        parser = _PARSERS.get(c.tag)
        if parser is None:
            d = {"raw_hex": payload[:64].hex(), "sobra": len(payload)}
            m.unknown.append({"tag": c.tag, "version": c.version, "size": c.size})
        else:
            try:
                d = parser(payload)
            except (MapError, struct.error) as exc:
                d = {"erro": str(exc), "raw_hex": payload[:64].hex(), "sobra": len(payload)}
        m.chunks.append((c.tag, c.version, c.size, d))
        m.unexplained_bytes += d.get("sobra", 0)
        t = c.tag
        if t == "EMAP": m.header = d
        elif t == "ECAM": m.camera = d
        elif t == "EME2": m.entities.append(d)
        elif t == "EMEF": m.effects.append(d)
        elif t == "EMEP": m.entry_points.append(d)
        elif t == "EMNP": m.nav_nodes = d.get("nodes", [])
        elif t == "EMNO": m.markers.append(d)
        elif t == "EPTH": m.paths.append(d)
        elif t == "EMSD": m.sounds.append(d)
        elif t == "2MWT": m.water.append(d)
        elif t == "EMTR":
            group = {"region": d}
            m.triggers.append(group)
        elif t in ("EBTR", "ESTR", "ETTR") and group is not None and len(group) == 1:
            group[{"EBTR": "binding", "ESTR": "script", "ETTR": "travel"}[t]] = d
    return m


# ------------------------------------------------------------------ escala mundo -> celulas da grade
def estimate_cell_scale(m: MapFile, grid) -> tuple:
    """Escolhe (fator, acertos, total) entre 0.5, 1, 2, 4: o fator em que mais pontos (entidades, nos de navegacao, entradas)
    caem em celulas NAO vazias da grade. MEDIDO: Chiefs/Tutorial_Vault -> 1; Spelunking -> 2. HIPOTESE: depende do mapa (talvez do .8tre)."""
    pts = [e["pos"] for e in m.entities] + [n["pos"] for n in m.nav_nodes]
    for ep in m.entry_points:
        pts += [q for q in ep.get("positions", []) if q[0] or q[2]]
    if not pts or grid is None:
        return 1.0, 0, len(pts)
    best = (1.0, -1, len(pts))
    for s in (1.0, 0.5, 2.0, 4.0):
        ok = 0
        for x, _y, z in pts:
            c, r = int(x * s), int(z * s)
            if 0 <= c < grid.width and 0 <= r < grid.rows and grid.at(c, r) != 0x2F:
                ok += 1
        if ok > best[1]:
            best = (s, ok, len(pts))
    return best
