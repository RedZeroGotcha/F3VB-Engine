"""Sondas de cabeçalho para TGA, BMP e B3D. Só leem; nunca modificam nada.

B3D: layout derivado de b3d.pat (OpenJE/patterns), a confirmar em arquivo real. A sonda
percorre o fluxo de tokens de topo enquanto o layout for linear e PARA ao primeiro
ponto duvidoso, dizendo onde e por quê.
"""
from __future__ import annotations

import struct

TGA_TYPES = {0: "sem imagem", 1: "paleta", 2: "truecolor", 3: "cinza",
             9: "paleta RLE", 10: "truecolor RLE", 11: "cinza RLE"}


def probe_tga(data: bytes) -> dict:
    if len(data) < 18:
        return {"format": "tga", "error": f"{len(data)} bytes, menor que o cabeçalho"}
    idlen, cmaptype, imgtype = data[0], data[1], data[2]
    cm_first, cm_len, cm_bits = struct.unpack_from("<HHB", data, 3)
    x0, y0, w, h, bpp, desc = struct.unpack_from("<HHHHBB", data, 8)
    info = {
        "format": "tga", "type": imgtype, "type_name": TGA_TYPES.get(imgtype, "desconhecido"),
        "width": w, "height": h, "bpp": bpp, "alpha_bits": desc & 0x0F,
        "origin": "topo" if desc & 0x20 else "base", "id_length": idlen,
        "colormap_type": cmaptype, "rle": imgtype in (9, 10, 11),
        "has_tga2_footer": len(data) >= 26 and data[-18:-2] == b"TRUEVISION-XFILE",
        "size": len(data),
    }
    if imgtype in (1, 2, 3):
        cmap = cm_len * ((cm_bits + 7) // 8) if cmaptype else 0
        expected = 18 + idlen + cmap + w * h * ((bpp + 7) // 8)
        info["expected_min_size"] = expected
        info["size_ok"] = len(data) >= expected
    plausible = imgtype in TGA_TYPES and 0 < w <= 8192 and 0 < h <= 8192 and bpp in (8, 15, 16, 24, 32)
    info["plausible"] = plausible
    return info


def probe_bmp(data: bytes) -> dict:
    if len(data) < 30 or data[:2] != b"BM":
        return {"format": "bmp", "error": "não parece BMP"}
    fsize, _, _, off, dib = struct.unpack_from("<IHHII", data, 2)
    w, h = struct.unpack_from("<ii", data, 18)
    planes, bpp, comp = struct.unpack_from("<HHI", data, 26)
    return {"format": "bmp", "declared_size": fsize, "actual_size": len(data), "data_offset": off,
            "dib_size": dib, "width": w, "height": h, "bpp": bpp, "compression": comp}


class _R:
    def __init__(self, data: bytes, pos: int = 0):
        self.d, self.p = data, pos

    def need(self, n: int):
        if self.p + n > len(self.d):
            raise ValueError(f"fim do arquivo em {self.p} (precisava de {n} bytes)")

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
            raise ValueError(f"string de {n} bytes em {self.p - 2} (limite {limit})")
        self.need(n)
        s = self.d[self.p:self.p + n].decode("latin-1"); self.p += n
        return s


def _plausible_name(d: bytes, p: int) -> bool:
    if p + 2 > len(d):
        return False
    n = struct.unpack_from("<H", d, p)[0]
    return 2 <= n <= 160 and p + 2 + n <= len(d) and all(32 <= c < 127 for c in d[p + 2:p + 2 + n])


def _material44(r: "_R") -> dict:
    """Material da família de vértice de 44 bytes. Layout OBSERVADO em arquivos reais (1 material):
    str16 nome, str16 id, 17 floats, str16 blend, str16 tipo, 16 x s32 (o último = quantidade de nomes de
    textura que seguem), as str16 das texturas, e 1 x u32 extra."""
    m = {"name": r.str16(), "id": r.str16()}
    m["floats"] = r.floats(17)
    m["blend"], m["type"] = r.str16(), r.str16()
    words = [r.s32() for _ in range(16)]
    m["flags_word0"] = words[0]
    n_tex = words[15]
    if not 0 <= n_tex <= 8:
        raise ValueError(f"material: contagem de texturas absurda ({n_tex})")
    textures = [r.str16() for _ in range(n_tex)]
    m["texture"] = textures[0] if textures else None
    m["textures"] = textures
    m["extra_u32"] = r.u32()
    return m


def _probe_node(r: "_R", out: dict, depth: int = 0) -> bool:
    """Percorre um SceneNode. Superfícies de 44 bytes/vértice: lidas por completo (validado em arquivos
    reais com 1 material). Superfícies de 64 bytes (terreno com lightmap): só o cabeçalho; a cauda tem outro
    layout (nome do lightmap + textura + triplas de s32 + bytes 0x01) ainda não decodificado.
    Devolve True se o nó terminou (0x0B); False se parou numa superfície não decodificada."""
    while r.p < len(r.d):
        t = r.u8()
        if t == 0x0B:
            return True
        if t == 0x0C:
            out["node_names"].append(r.str16())
        elif t == 0x0D:
            r.str16()
            fl = r.u8()
            if fl & 1:
                r.floats(3)
            if fl & 2:
                r.floats(4)
            if fl & 4:
                r.floats(3)
        elif t in (0x0F, 0x19):
            r.u8(); r.u8()
            vflag = r.u8()
            nv = r.s32()
            size_of_verts = r.s32() if vflag == 1 else None     # a família de 64 bytes não tem este campo
            vsize = 44 if vflag == 1 else 64
            if nv < 0 or nv > 2_000_000 or r.p + nv * vsize > len(r.d):
                raise ValueError(f"superfície com {nv} vértices não cabe no arquivo (vértice de {vsize} bytes)")
            sf = {"vertices": nv, "vertex_bytes": vsize, "size_of_verts_field": size_of_verts,
                  "complete": False, "family": "44" if vflag == 1 else "64"}
            out["surfaces"].append(sf)
            if vflag != 1:
                # Família de 64 bytes (terreno com lightmap). Layout OBSERVADO (1 arquivo distinto de 1041 bytes,
                # 9 vértices, 8 triângulos): vértices; u32 0; str16 lightmap; s32 n; n x str16 textura; u32 0;
                # u8 (0xC9); s32 kk; kk x s32 índices; kk x u8 (=1). Confirmar em escala com o relatório v9.
                r.p += nv * vsize
                r.u32()
                sf["unknown3"] = r.u32()
                sf["lightmap"] = r.str16()
                n_tex = r.s32()
                if not 0 <= n_tex <= 8:
                    raise ValueError(f"família 64: contagem de texturas absurda ({n_tex})")
                sf["textures"] = [r.str16() for _ in range(n_tex)]
                sf["pre_kk"] = r.u32()
                sf["flag_byte"] = r.u8()
                kk = r.s32()
                if kk < 0 or kk % 3 or r.p + kk * 5 > len(r.d):
                    raise ValueError(f"família 64: kk={kk} inválido")
                idx = r.floats(0) if kk == 0 else struct.unpack_from(f"<{kk}i", r.d, r.p)
                r.p += kk * 4
                if idx and (min(idx) < 0 or max(idx) >= max(nv, 1)):
                    raise ValueError("família 64: índice de vértice fora do intervalo")
                r.p += kk                                   # kk bytes (todos 0x01 na amostra)
                sf.update(complete=True, index_counts=[kk], face_counts=[kk // 3])
                continue
            r.p += nv * vsize
            r.floats(6)
            is_bones = r.u8()
            sf["bone_weights"] = bool(is_bones)
            if is_bones == 1:
                bpv = r.s32()
                if bpv < 0 or bpv > 16 or r.p + nv * bpv * 8 > len(r.d):
                    raise ValueError("dados de pesos de osso fora do arquivo")
                sf["bones_per_vertex"] = bpv
                r.p += nv * bpv * 8
            nm = r.s32()
            if not 0 <= nm <= 64:
                raise ValueError(f"numMaterials absurdo: {nm}")
            sf["materials"] = nm
            mats = [_material44(r) for _ in range(nm)]
            names = [r.str16() for _ in range(nm)]
            mat_list = [r.s32() for _ in range(nm)]
            vert_counts = [r.s32() for _ in range(nm)]
            for n_idx in mat_list:
                if n_idx < 0 or r.p + 2 * n_idx > len(r.d):
                    raise ValueError("lista de índices fora do arquivo")
                r.p += 2 * n_idx
            sf.update(complete=True, material_names=names, textures=[m.get("texture") for m in mats], material_ids=[m["id"] for m in mats],
                      index_counts=mat_list, face_counts=vert_counts,
                      blend=[m["blend"] for m in mats], surface_type=[m["type"] for m in mats])
        elif t == 0x1B:
            for _ in range(r.s32()):
                r.str16()
        elif t in (0x0A, 0x13):
            if depth > 16 or not _probe_node(r, out, depth + 1):
                return False
        else:
            raise ValueError(f"token de nó desconhecido {t:#04x} em {r.p - 1}")
    return False


def probe_b3d(data: bytes, max_items: int = 40) -> dict:
    out: dict = {"format": "b3d", "size": len(data), "signature": data[:8].decode("latin-1", "replace"),
                 "globals": {}, "materials": [], "textures": [], "bones": [],
                 "tokens_seen": [], "stopped": None}
    if data[:4] != b"B3D ":
        out["stopped"] = {"offset": 0, "reason": "não começa com 'B3D '"}
        return out
    r = _R(data, 8)
    while r.p < len(data):
        at = r.p
        tag = r.u8()
        out["tokens_seen"].append(f"{tag:#04x}@{at}") if len(out["tokens_seen"]) < 60 else None
        try:
            if tag == 0x01:
                out["ended_at"] = r.p
                break
            elif tag == 0x03:
                out["globals"]["global_scale"] = r.f32()
            elif tag == 0x04:
                out["globals"]["coordinate_scale"] = r.f32()
            elif tag == 0x05:
                out["globals"]["coordinate_system"] = r.str16()
            elif tag == 0x06:
                out["globals"]["token06"] = [r.s32(), r.s32(), r.s32()]
            elif tag == 0x16:
                out["globals"]["token16"] = r.s32()
            elif tag == 0x1C:
                out["globals"]["version_flag"] = r.u8()
            elif tag == 0x35:
                out["globals"]["legacy_v35"] = r.u8()
            elif tag in (0x07, 0x0F, 0x10):
                mid, name = r.str16(), r.str16()
                f = r.floats(18)
                blend, mtype = r.str16(), r.str16()
                f1, f2 = r.s32(), r.s32()
                if len(out["materials"]) < max_items:
                    out["materials"].append({"id": mid, "name": name, "diffuse": f[4:8], "shininess": f[16],
                                             "alpha": f[17], "blend_state": blend, "material_type": mtype,
                                             "flags": [f1, f2]})
            elif tag in (0x08, 0x09):
                unk = r.u8()
                name, fname = r.str16(), r.str16()
                w, h = r.u32(), r.u32()
                if len(out["textures"]) < max_items:
                    out["textures"].append({"name": name, "file": fname, "width": w, "height": h, "unk": unk})
            elif tag in (0x0E, 0x18):
                name = r.str16()
                unk = r.u16()
                flag = r.u8()
                b = {"name": name, "flag": flag, "unk": unk}
                if flag != 0:
                    b["translation"] = r.floats(3)
                if flag == 3:
                    b["rotation"] = r.floats(4)
                if flag == 2:
                    b["unknown_float"] = r.f32()
                out.setdefault("bone_count", 0)
                out["bone_count"] += 1
                if len(out["bones"]) < max_items:
                    out["bones"].append(b)
            elif tag in (0x0A, 0x13):
                out["first_node_offset"] = at
                out.setdefault("node_names", [])
                out.setdefault("surfaces", [])
                complete = _probe_node(r, out)
                if complete:                                   # raiz fechou: o próximo token deve ser 0x01
                    if r.p < len(data) and data[r.p] == 0x01:
                        out["ended_at"] = r.p + 1
                        out["stopped"] = None
                    else:
                        out["stopped"] = {"offset": r.p, "tag": "", "reason": "arquivo não terminou em 0x01"}
                else:
                    fam = next((x["family"] for x in reversed(out["surfaces"]) if not x["complete"]), "?")
                    out["stopped"] = {"offset": r.p, "tag": f"{tag:#04x}",
                                      "reason": f"superfície da família de {fam} bytes ainda não decodificada"}
                break
            else:
                out["stopped"] = {"offset": at, "tag": f"{tag:#04x}", "reason": "token desconhecido para a sonda"}
                break
        except ValueError as exc:
            out["stopped"] = {"offset": at, "tag": f"{tag:#04x}", "reason": str(exc)}
            break
    out["parsed_until"] = r.p
    return out


def probe_amx(data: bytes, max_names: int = 400) -> dict:
    """Sonda de Pawn/Small (AMX): CONFIRMADO nos 119 scripts da demo (file_version 6, magic 0xF1E0 = células de 32 bits).

    Cabeçalho (file_version < 7: 52 bytes): i32 size, u16 magic, i8 file_version, i8 amx_version, i16 flags,
    i16 defsize, i32 cod, dat, hea, stp, cip, publics, natives, libraries, pubvars, tags. A partir da
    versão 7 há mais um campo (nametable). Com defsize 40 os nomes ficam embutidos em cada entrada.
    """
    if len(data) < 52:
        return {"format": "amx", "error": f"{len(data)} bytes, menor que o cabeçalho (52)"}
    size, magic, fver, aver, flags, defsize = struct.unpack_from("<iHbbhh", data, 0)
    cod, dat, hea, stp, cip, publics, natives, libraries, pubvars, tags = struct.unpack_from("<10i", data, 12)
    nametable = struct.unpack_from("<i", data, 52)[0] if fver >= 7 and len(data) >= 56 else None
    info = {"format": "amx", "size_declared": size, "size_actual": len(data), "magic": f"{magic:#06x}",
            "file_version": fver, "amx_version": aver, "flags": flags, "compact": bool(flags & 0x04),
            "debug_flag": bool(flags & 0x02), "defsize": defsize, "cod": cod, "dat": dat, "hea": hea, "stp": stp,
            "cip": cip, "publics_offset": publics, "natives_offset": natives, "nametable": nametable,
            "code_bytes": dat - cod, "data_bytes": hea - dat, "stack_heap_bytes": stp - hea,
            "publics": [], "natives": []}
    if magic not in (0xF1E0, 0xF1E1, 0xF1E2) or defsize <= 0:
        info["error"] = "magic ou defsize inválido: não parece Pawn/AMX"
        return info

    def name_at(entry: int) -> str | None:
        try:
            if defsize == 8 and nametable is not None:
                ofs = struct.unpack_from("<I", data, entry + 4)[0]
                if ofs >= len(data):
                    return None
                end = data.index(b"\0", ofs, min(len(data), ofs + 64))
                return data[ofs:end].decode("latin-1")
            raw = data[entry + 4:entry + defsize]
            return raw.split(b"\0", 1)[0].decode("latin-1")
        except ValueError:
            return None

    def table(start: int, stop: int, key: str) -> None:
        if not (0 < start <= stop <= len(data)):
            return
        n = (stop - start) // defsize
        info[f"{key}_count"] = n
        for i in range(min(n, max_names)):
            nm = name_at(start + i * defsize)
            if nm is not None:
                info[key].append(nm)

    table(publics, natives, "publics")
    table(natives, libraries, "natives")
    return info
