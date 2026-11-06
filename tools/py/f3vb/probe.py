"""Sondagem de um .grp SEM assumir o layout da tabela de lumps.

Serve para o M0: ver o início do arquivo, contar magics conhecidos e chunks
plausíveis. Contagens são APROXIMADAS (varredura crua, com falsos positivos).
"""
from __future__ import annotations

import mmap
from pathlib import Path

from .chunk import plausible_chunk_at
from .knowledge import CHUNK_TAGS, MAGICS


def hexdump(data: bytes, base: int = 0, width: int = 16) -> str:
    lines = []
    for i in range(0, len(data), width):
        row = data[i:i + width]
        hx = " ".join(f"{b:02x}" for b in row).ljust(width * 3 - 1)
        asc = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        lines.append(f"{base + i:08x}  {hx}  |{asc}|")
    return "\n".join(lines)


def _find_all(mm, needle: bytes, limit: int | None = None):
    pos = mm.find(needle)
    n = 0
    while pos != -1:
        yield pos
        n += 1
        if limit is not None and n >= limit:
            return
        pos = mm.find(needle, pos + 1)


def probe_file(path: str | Path, head_bytes: int = 128, sample: int = 3) -> dict:
    p = Path(path)
    size = p.stat().st_size
    if size == 0:
        return {"path": str(p), "size": 0, "head_hex": "", "magics": {}, "chunks": {}}
    with p.open("rb") as fh, mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ) as mm:
        head = mm[:head_bytes]
        magics = {}
        for magic, label in MAGICS.items():
            offs = list(_find_all(mm, magic))
            magics[label] = {"count": len(offs), "first_offsets": offs[:sample]}
        chunks = {}
        for group, tags in CHUNK_TAGS.items():
            for tag in tags:
                tb = tag.encode("ascii")
                offs = [o for o in _find_all(mm, tb) if plausible_chunk_at(mm, o, tb)]
                if offs:
                    chunks[tag] = {"group": group, "plausible": len(offs),
                                   "first_offsets": offs[:sample]}
    return {
        "path": str(p),
        "size": size,
        "head_hex": hexdump(head),
        "head_ascii_16": "".join(chr(b) if 32 <= b < 127 else "." for b in head[:16]),
        "magics": magics,
        "chunks": chunks,
    }
