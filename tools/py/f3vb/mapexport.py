"""Formato `.f3map`: o mapa decodificado (.map + .rle + .8tre) num arquivo unico para o F3VBMapViewer.

Container de chunks, little-endian: "F3MP" + u32 versao (1, 2 ou 3; o v18 escreve 3 e le as tres) + { tag[4], u32 tamanho do payload, payload }... Tags desconhecidas sao ignoradas.
Strings = u16 comprimento + bytes. Posicoes (x, y, z) = f32 x 3, no MESMO espaco do mundo da geometria do .8tre.
  INFO  str nome, str tre, str rle, f32 origem_x, f32 origem_z, f32 tamanho_da_celula, u32 flags
  GRID  u32 largura, u32 linhas, u8[largura*linhas]            celula = (mundo - origem) / tamanho_da_celula
  GEOM  u32 N blocos; N x { u32 nv, f32[3nv] xyz, f32[3nv] normais, u8[4nv] RGBA [, v3: f32[2nv] uv da textura difusa, f32[2nv] uv do lightmap] };  u32 L listas;
          v1: L x { u32 bloco, u32 ntris, u32[3*ntris] }
          v2/v3: L x { u32 bloco, u32 material, u32 pagina, u32 textura, u32 ntris, u32[3*ntris] }     (uma lista por combinacao bloco/material/pagina/textura)
              material = indice 0-based em MATL; pagina = 1-based na lista ".ctx" de TEXN (0 = nenhuma); textura = 1-based na lista ".tga" de TEXN (0 = nenhuma)
  MATL  (v2) u32 n; n x { u32 id, str nome, u8[4] RGBA, str mistura, str superficie, u32 flags }
  TEXN  (v2) u32 n; n x str      nomes na ordem do .8tre: ".tga" = texturas difusas, ".ctx" = paginas de lightmap (arquivo "<nome>.tga")
  ENTS  u32 n; n x { str tipo, str prototipo, str instancia, str nome, str script, str pele, f32[3] pos, f32 yaw }
  NAVN  u32 n; n x { u8 tipo, f32[3] pos, u8 nlinks, u8[nlinks] }
  ENTR  u32 n; n x { u8 id, u8 np, f32[3*np] pos, f32[np] angulos }
  TRIG  u32 n; n x { u8 tipo (0 regiao, 1 script, 2 viagem, 3 vinculo), str alvo, i32 ponto_de_entrada, u32 np, f32[3*np] }
  PATH  u32 n; n x { str nome, u32 np, f32[3*np] }
  SOND  u32 n; n x { str nome, f32[3] pos, str som }
  MARK  u32 n; n x { f32[3] pos, str icone, u32 id_de_texto }
  EFFT  u32 n; n x { str nome, f32[3] pos, str efeito }
  WATR  u32 n; n x { f32[3] pos, str textura }
  LGHT  u32 n; n x { u32 tipo, f32[3] cor, f32[3] pos, f32[3] dir }
  CAMR  (v19) f32[4] do ECAM do .map: limites da camera (xmin, zmax, xmax, zmin) - padrao (0, 1000, 1000, 0) = sem limite. Hipotese forte (medida em 3 mapas).
  ENVI  (v19) u8[4] cor A, u8[4] cor B, f32[3] do cabecalho do .map (hipotese: cor de fundo/nevoa e nevoa perto/longe, ex. 250/500)
O arquivo e derivado de conteudo do jogo: fica em original_assets/CONVERTED/maps e nunca vai para o projeto.
"""
from __future__ import annotations

import struct

CELL_SIZE_DEFAULT = 0.5


class F3MapError(ValueError):
    pass


def _s(t: str) -> bytes:
    b = (t or "").encode("latin-1", "replace")[:65535]
    return struct.pack("<H", len(b)) + b


def _chunk(tag: str, payload: bytes) -> bytes:
    return tag.encode() + struct.pack("<I", len(payload)) + payload


def grid_origin(tre) -> tuple:
    """Origem do mundo da grade .rle: canto minimo (x, z) da geometria do .8tre (regra medida: 186 de 197 pontos em celulas ocupadas)."""
    b = tre.bounds() if tre is not None else None
    return (b[0], b[2]) if b else (0.0, 0.0)


def world_to_cell(pos, origin, cell=CELL_SIZE_DEFAULT) -> tuple:
    return ((pos[0] - origin[0]) / cell, (pos[2] - origin[1]) / cell)


def build_f3map(name: str, m, grid, tre, version: int = 3) -> bytes:
    """m: MapFile; grid: RleGrid|None; tre: TreFile|None; version 3 (padrao) = v2 + UVs por vertice (difusa e lightmap); 2 = listas com material/pagina/textura + MATL/TEXN (v16); 1 = formato do v15."""
    if version not in (1, 2, 3):
        raise F3MapError(f"versao {version} nao suportada")
    origin = grid_origin(tre)
    cell = tre.level_info[2] if tre is not None and len(tre.level_info) == 4 and tre.level_info[2] > 0 else CELL_SIZE_DEFAULT
    out = [b"F3MP", struct.pack("<I", version)]
    h = m.header
    out.append(_chunk("INFO", _s(name) + _s(h.get("tre", "")) + _s(h.get("rle", "")) + struct.pack("<3fI", origin[0], origin[1], cell, 0)))
    if grid is not None:
        out.append(_chunk("GRID", struct.pack("<II", grid.width, grid.rows) + grid.cells))
    if tre is not None and tre.blocks:
        tris, _stats = tre.triangles_ex()
        p = [struct.pack("<I", len(tre.blocks))]
        for b in tre.blocks:
            nrm = b.normals if b.normals else (0.0,) * (3 * b.count)
            col = b.colors if b.colors else b"\xff" * (4 * b.count)
            p += [struct.pack("<I", b.count), struct.pack("<%df" % (3 * b.count), *b.xyz), struct.pack("<%df" % (3 * b.count), *nrm), col]
            if version >= 3:
                uv = b.uv if b.uv else (0.0,) * (2 * b.count)
                lm = b.lightmap_uv if b.lightmap_uv else (0.0,) * (2 * b.count)
                p += [struct.pack("<%df" % (2 * b.count), *uv), struct.pack("<%df" % (2 * b.count), *lm)]
        per: dict = {}
        for blk, t, mat, pg, tx in tris:
            per.setdefault((blk, mat, pg, tx) if version >= 2 else (blk,), []).extend(t)
        p.append(struct.pack("<I", len(per)))
        for key, idx in sorted(per.items()):
            p += [struct.pack("<%dI" % (len(key) + 1), *key, len(idx) // 3), struct.pack("<%dI" % len(idx), *idx)]
        out.append(_chunk("GEOM", b"".join(p)))
        if version >= 2:
            if tre.materials:
                p = [struct.pack("<I", len(tre.materials))]
                for mt in tre.materials:
                    p += [struct.pack("<I", mt["id"]), _s(mt["name"]), bytes(mt["color"][:4]).ljust(4, b"\0"), _s(mt["blend"]), _s(mt["surface"]), struct.pack("<I", mt["flags"])]
                out.append(_chunk("MATL", b"".join(p)))
            if tre.textures:
                out.append(_chunk("TEXN", struct.pack("<I", len(tre.textures)) + b"".join(_s(x) for x in tre.textures)))
    if m.entities:
        p = [struct.pack("<I", len(m.entities))]
        for e in m.entities:
            ev = e.get("eeov") or {}
            p += [_s(e["kind"]), _s(e["prototype"]), _s(ev.get("instance", "")), _s(ev.get("display_name") or ""), _s(ev.get("script") or ""),
                  _s(ev.get("skin") or ""), struct.pack("<3ff", *e["pos"], e["yaw"])]
        out.append(_chunk("ENTS", b"".join(p)))
    if m.nav_nodes:
        p = [struct.pack("<I", len(m.nav_nodes))]
        for n in m.nav_nodes:
            p += [struct.pack("<B3fB", n["kind"], *n["pos"], len(n["links"])), bytes(n["links"])]
        out.append(_chunk("NAVN", b"".join(p)))
    if m.entry_points:
        p = [struct.pack("<I", len(m.entry_points))]
        for e in m.entry_points:
            pos, ang = e.get("positions", []), e.get("angles", [])
            p += [struct.pack("<BB", e["id"], len(pos))] + [struct.pack("<3f", *q) for q in pos] + [struct.pack("<f", a) for a in ang]
        out.append(_chunk("ENTR", b"".join(p)))
    if m.triggers:
        p = [struct.pack("<I", len(m.triggers))]
        for t in m.triggers:
            pts = t["region"].get("points", [])
            kind, target, entry = 0, "", -1
            if "travel" in t:
                kind, target, entry = 2, t["travel"].get("target_map", ""), t["travel"].get("entry_point") or 0
            elif "script" in t:
                kind, target = 1, t["script"].get("script", "")
            elif "binding" in t:
                kind, entry = 3, t["binding"].get("id", 0)
            p += [struct.pack("<B", kind), _s(target), struct.pack("<iI", entry, len(pts))] + [struct.pack("<3f", *q) for q in pts]
        out.append(_chunk("TRIG", b"".join(p)))
    if m.paths:
        p = [struct.pack("<I", len(m.paths))]
        for pa in m.paths:
            pts = pa.get("points", [])
            p += [_s(pa["name"]), struct.pack("<I", len(pts))] + [struct.pack("<3f", *q["pos"]) for q in pts]
        out.append(_chunk("PATH", b"".join(p)))
    if m.sounds:
        p = [struct.pack("<I", len(m.sounds))]
        for s in m.sounds:
            p += [_s(s["name"]), struct.pack("<3f", *s["pos"]), _s(s.get("sound") or "")]
        out.append(_chunk("SOND", b"".join(p)))
    if m.markers:
        p = [struct.pack("<I", len(m.markers))]
        for k in m.markers:
            p += [struct.pack("<3f", *k["pos"]), _s(k["icon"]), struct.pack("<I", k["text_id"] or 0)]
        out.append(_chunk("MARK", b"".join(p)))
    if m.effects:
        p = [struct.pack("<I", len(m.effects))]
        for e in m.effects:
            p += [_s(e["instance"]), struct.pack("<3f", *e["pos"]), _s(e["effect"])]
        out.append(_chunk("EFFT", b"".join(p)))
    patches = [q for w in m.water for q in w.get("patches", [])]
    if patches:
        p = [struct.pack("<I", len(patches))]
        for q in patches:
            p += [struct.pack("<3f", *q["pos"]), _s(q["texture"])]
        out.append(_chunk("WATR", b"".join(p)))
    cam = (m.camera or {}).get("floats")
    if cam and len(cam) == 4:
        out.append(_chunk("CAMR", struct.pack("<4f", *cam)))
    if "color_a" in h and "floats" in h:
        ca, cb, fl = list(h.get("color_a", [0] * 4))[:4], list(h.get("color_b", [0] * 4))[:4], list(h.get("floats", [0.0] * 3))[:3]
        out.append(_chunk("ENVI", bytes(ca + [0] * (4 - len(ca))) + bytes(cb + [0] * (4 - len(cb))) + struct.pack("<3f", *(fl + [0.0] * (3 - len(fl))))))
    if tre is not None and tre.lights:
        p = [struct.pack("<I", len(tre.lights))]
        for l in tre.lights:
            p.append(struct.pack("<I9f", l["type"], *l["color"], *l["pos"], *l["dir"]))
        out.append(_chunk("LGHT", b"".join(p)))
    return b"".join(out)


class _R:
    def __init__(self, b):
        self.b, self.p = b, 0

    def take(self, n):
        if n < 0 or self.p + n > len(self.b):
            raise F3MapError("leitura alem do chunk")
        v = self.b[self.p:self.p + n]
        self.p += n
        return v

    def u8(self): return self.take(1)[0]
    def u32(self): return struct.unpack("<I", self.take(4))[0]
    def i32(self): return struct.unpack("<i", self.take(4))[0]
    def f32(self): return struct.unpack("<f", self.take(4))[0]
    def vec(self): return list(struct.unpack("<3f", self.take(12)))
    def s(self):
        n, = struct.unpack("<H", self.take(2))
        return self.take(n).decode("latin-1")


def read_f3map(data: bytes) -> dict:
    """Leitor de referencia (testes e conferencia). Devolve um dict com as mesmas secoes."""
    if data[:4] != b"F3MP":
        raise F3MapError("assinatura F3MP ausente")
    if len(data) < 8 or struct.unpack_from("<I", data, 4)[0] not in (1, 2, 3):
        raise F3MapError("versao nao suportada")
    ver = struct.unpack_from("<I", data, 4)[0]
    pos, d = 8, {"version": ver}
    while pos + 8 <= len(data):
        tag = data[pos:pos + 4].decode("latin-1")
        n, = struct.unpack_from("<I", data, pos + 4)
        if pos + 8 + n > len(data):
            raise F3MapError(f"chunk {tag} ultrapassa o arquivo")
        r = _R(data[pos + 8:pos + 8 + n])
        pos += 8 + n
        if tag == "INFO":
            d["info"] = {"name": r.s(), "tre": r.s(), "rle": r.s(), "origin": [r.f32(), r.f32()], "cell": r.f32(), "flags": r.u32()}
        elif tag == "GRID":
            w, h = r.u32(), r.u32()
            d["grid"] = {"width": w, "rows": h, "cells": r.take(w * h)}
        elif tag == "GEOM":
            blocks = []
            for _ in range(r.u32()):
                nv = r.u32()
                blk = {"n": nv, "xyz": struct.unpack("<%df" % (3 * nv), r.take(12 * nv)), "nrm": struct.unpack("<%df" % (3 * nv), r.take(12 * nv)), "rgba": r.take(4 * nv)}
                if ver >= 3:
                    blk["uv"] = struct.unpack("<%df" % (2 * nv), r.take(8 * nv))
                    blk["lmuv"] = struct.unpack("<%df" % (2 * nv), r.take(8 * nv))
                blocks.append(blk)
            lists = []
            for _ in range(r.u32()):
                blk = r.u32()
                mat, pg, tx = (r.u32(), r.u32(), r.u32()) if ver >= 2 else (0, 0, 0)
                nt = r.u32()
                lists.append({"block": blk, "material": mat, "page": pg, "texture": tx, "indices": struct.unpack("<%dI" % (3 * nt), r.take(12 * nt))})
            d["geom"] = {"blocks": blocks, "lists": lists}
        elif tag == "MATL":
            d["materials"] = []
            for _ in range(r.u32()):
                i = r.u32(); nm = r.s(); col = list(r.take(4)); bl = r.s(); sf = r.s()
                d["materials"].append({"id": i, "name": nm, "color": col, "blend": bl, "surface": sf, "flags": r.u32()})
        elif tag == "TEXN":
            d["textures"] = [r.s() for _ in range(r.u32())]
        elif tag == "ENTS":
            d["ents"] = [{"kind": r.s(), "prototype": r.s(), "instance": r.s(), "name": r.s(), "script": r.s(), "skin": r.s(), "pos": r.vec(), "yaw": r.f32()} for _ in range(r.u32())]
        elif tag == "NAVN":
            nodes = []
            for _ in range(r.u32()):
                k = r.u8(); p = r.vec(); nl = r.u8()
                nodes.append({"kind": k, "pos": p, "links": list(r.take(nl))})
            d["nav"] = nodes
        elif tag == "ENTR":
            es = []
            for _ in range(r.u32()):
                i, npos = r.u8(), r.u8()
                pts = [r.vec() for _ in range(npos)]
                es.append({"id": i, "positions": pts, "angles": [r.f32() for _ in range(npos)]})
            d["entry"] = es
        elif tag == "TRIG":
            ts = []
            for _ in range(r.u32()):
                k = r.u8(); tgt = r.s(); en = r.i32(); np_ = r.u32()
                ts.append({"kind": k, "target": tgt, "entry": en, "points": [r.vec() for _ in range(np_)]})
            d["triggers"] = ts
        elif tag == "PATH":
            ps = []
            for _ in range(r.u32()):
                nm = r.s(); np_ = r.u32(); ps.append({"name": nm, "points": [r.vec() for _ in range(np_)]})
            d["paths"] = ps
        elif tag == "SOND":
            d["sounds"] = [{"name": r.s(), "pos": r.vec(), "sound": r.s()} for _ in range(r.u32())]
        elif tag == "MARK":
            d["markers"] = [{"pos": r.vec(), "icon": r.s(), "text_id": r.u32()} for _ in range(r.u32())]
        elif tag == "EFFT":
            d["effects"] = [{"name": r.s(), "pos": r.vec(), "effect": r.s()} for _ in range(r.u32())]
        elif tag == "WATR":
            d["water"] = [{"pos": r.vec(), "texture": r.s()} for _ in range(r.u32())]
        elif tag == "LGHT":
            ls = []
            for _ in range(r.u32()):
                t_ = r.u32(); v = [r.f32() for _ in range(9)]
                ls.append({"type": t_, "color": v[0:3], "pos": v[3:6], "dir": v[6:9]})
            d["lights"] = ls
        elif tag == "CAMR":
            d["camera"] = [r.f32() for _ in range(4)]
        elif tag == "ENVI":
            d["env"] = {"color_a": [r.u8() for _ in range(4)], "color_b": [r.u8() for _ in range(4)], "floats": [r.f32() for _ in range(3)]}
    return d
