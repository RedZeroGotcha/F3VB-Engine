"""Interface do jogo: inventario do pack Interface, arvores dos `.int` (layouts de tela, decodificados no v19: intfile.py) e imagens da UI em .f3tex.

v18: so sondava os `.int` (strings e bytes). v19: cada `.int` e lido pela gramatica completa (docs/specs/int.md) e o relatorio mostra a arvore de widgets
com retangulos, skins, imagens, fontes e textos (do .stf, se for achado). Se um `.int` nao fechar, a sonda antiga aparece no lugar.
"""
from __future__ import annotations

import re
import struct
from pathlib import Path

from .chunk import ChunkError, read_chunk
from .imagecodec import ImageError, decode_bmp, decode_tga

INT_TYPE = "INT_Interface"
IMAGE_TYPES = ("TGA_Image",)
MAX_DEPTH = 5


def _strings(data: bytes, min_len: int = 3, limit: int = 40) -> list:
    """Strings ASCII (>= min_len) e UTF-16LE plausiveis, na ordem em que aparecem."""
    out = []
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % min_len, data):
        out.append((m.start(), m.group().decode("ascii")))
    for m in re.finditer(rb"(?:[\x20-\x7e]\x00){%d,}" % min_len, data):
        out.append((m.start(), m.group().decode("utf-16le")))
    out.sort()
    return out[:limit]


def _tree(data: bytes, start: int, end: int, depth: int, lines: list) -> bool:
    """Tenta ler [start, end) como chunks consecutivos que fecham exatamente. Descreve cada chunk e desce quando o payload tambem e um container."""
    pos = start
    parsed = []
    while pos < end:
        try:
            c = read_chunk(data, pos, end)
        except ChunkError:
            return False
        parsed.append(c)
        pos += c.size
    if pos != end or not parsed:
        return False
    for c in parsed:
        ind = "  " * depth
        lines.append(f"{ind}{c.tag} v{c.version} tamanho {c.size} @{c.offset}")
        if depth < MAX_DEPTH and c.payload_size >= 12:
            sub: list = []
            if _tree(data, c.payload_offset, c.offset + c.size, depth + 1, sub):
                lines.extend(sub)
                continue
        pay = data[c.payload_offset:c.offset + c.size]
        lines.append(f"{ind}  payload {len(pay)} bytes: {pay[:24].hex(' ')}{' ...' if len(pay) > 24 else ''}")
    return True


def probe_int(data: bytes) -> list:
    lines = [f"tamanho {len(data)} bytes; primeiros 32 bytes: {data[:32].hex(' ')}"]
    tree: list = []
    if _tree(data, 0, len(data), 0, tree):
        lines.append("enquadramento tag/versao/tamanho: FECHA ate o ultimo byte")
        lines.extend(tree)
    else:
        lines.append("enquadramento tag/versao/tamanho: NAO fecha (formato diferente); estrutura abaixo e so de strings e bytes")
    st = _strings(data)
    if st:
        lines.append("strings (offset: texto): " + " | ".join(f"{o}: {t}" for o, t in st[:30]))
    return lines


def ui_report(ix, stf_lookup=None) -> str:
    """Relatorio dos recursos de interface do RETAIL. stf_lookup: funcao id 1-based -> texto (opcional)."""
    from .intfile import IntError, describe, read_int
    rows = [(pack, number, type_name, name) for t, ents in ix.entries.items() for (name, pack, number) in ents for type_name in (t,) if pack.lower() == "interface"]
    by_type: dict = {}
    for pack, number, type_name, name in rows:
        by_type.setdefault(type_name, []).append((name, pack, number))
    out = ["=== INTERFACE (pack Interface do RETAIL) ===", f"recursos: {len(rows)}"]
    for t, ents in sorted(by_type.items()):
        out.append(f"  {t}: {len(ents)}")
    for name, pack, number in sorted(by_type.get(INT_TYPE, [])):
        data = ix.read(pack, number)
        out.append("")
        out.append(f"--- {INT_TYPE} {name} ({len(data)} bytes) ---")
        try:
            out.extend("  " + ln for ln in describe(read_int(data), stf_lookup))
        except IntError as exc:
            out.append(f"  NAO DECODIFICOU ({exc}); sonda do v18:")
            out.extend("  " + ln for ln in probe_int(data))
    fnt = sorted(by_type.get("FNT", []))
    for name, pack, number in fnt[:10]:
        data = ix.read(pack, number)
        out.append("")
        out.append(f"--- FNT {name} ({len(data)} bytes): {data[:32].hex(' ')} ---")
    names = sorted(n for n, _p, _n in by_type.get("TGA_Image", []))
    out.append("")
    out.append(f"imagens (TGA_Image) da interface: {len(names)}; primeiras: {', '.join(names[:30])}")
    return "\n".join(out) + "\n"


def export_ui_images(ix, conv: Path, overwrite: bool = False) -> tuple:
    """Imagens do pack Interface (TGA e BMP) -> CONVERTED/ui/<nome>.f3tex. Devolve (convertidas, falhas)."""
    from .extractor import encode_f3tex, safe_name
    udir = conv / "ui"
    n = 0
    fails: list = []
    for t in IMAGE_TYPES:
        for name, pack, number in sorted(ix.entries.get(t, [])):
            if pack.lower() != "interface":
                continue
            dest = udir / f"{safe_name(name)}.f3tex"
            if dest.exists() and not overwrite:
                n += 1
                continue
            data = ix.read(pack, number)
            try:
                w, h, rgba = decode_bmp(data) if data[:2] == b"BM" else decode_tga(data)
                udir.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(encode_f3tex(w, h, rgba))
                n += 1
            except (ImageError, ValueError, struct.error) as exc:
                fails.append(f"{name}: {type(exc).__name__}: {exc}")
    return n, fails
