"""Leitor de resource.rht (layout do rht.pat, CONFIRMADO na demo real: v1.1, 7044 entradas).

    u32 vMajor, vMinor, nEntries, offsetPacks, offsetResources      (20 bytes)
    nEntries x { u32 one, number, type, offset, pack }              (20 bytes cada)
    [offsetPacks .. offsetResources)  nomes dos .grp, texto terminado em NUL
    [offsetResources .. fim)          nomes dos recursos, texto terminado em NUL

Semântica observada na demo (ver docs/specs/rht.md):
  one     = sempre 1
  pack    = deslocamento (em bytes) DENTRO da região de packs: o nome do .grp ("Tiles", "Critters"...)
  number  = índice do lump dentro desse .grp
  type    = id do tipo de recurso
  offset  = (hipótese, validada por `catalog`) deslocamento do nome do recurso na região de recursos
Atenção: o enum PackOffset do rht.pat tem RÓTULOS errados (PSF, INT...): só os valores numéricos servem.
"""
from __future__ import annotations

import struct
from collections import Counter
from dataclasses import dataclass, field

from .knowledge import PACK_INVALID, RESOURCE_EXT, RESOURCE_TYPES

_HEADER = struct.Struct("<5I")
_ENTRY = struct.Struct("<5I")
HEADER_SIZE = _HEADER.size
ENTRY_SIZE = _ENTRY.size


class RhtError(ValueError):
    pass


@dataclass(frozen=True)
class RhtEntry:
    index: int
    one: int
    number: int
    type: int
    offset: int
    pack: int

    @property
    def type_name(self) -> str:
        return RESOURCE_TYPES.get(self.type, f"DESCONHECIDO({self.type})")

    @property
    def ext(self) -> str | None:
        return RESOURCE_EXT.get(self.type)



@dataclass
class RhtInfo:
    v_major: int
    v_minor: int
    n_entries: int
    offset_packs: int
    offset_resources: int
    file_size: int
    entries: list[RhtEntry]
    pack_region: bytes = b""
    resource_region: bytes = b""
    resource_region_size: int = 0
    issues: list[str] = field(default_factory=list)

    @property
    def entries_end(self) -> int:
        return HEADER_SIZE + ENTRY_SIZE * self.n_entries

    def pack_strings(self) -> list[tuple[int, str]]:
        """Divide a região de packs em strings terminadas em NUL: (offset relativo, texto)."""
        out, start = [], 0
        for i, b in enumerate(self.pack_region):
            if b == 0:
                if i > start:
                    out.append((start, self.pack_region[start:i].decode("latin-1")))
                start = i + 1
        if start < len(self.pack_region):
            out.append((start, self.pack_region[start:].decode("latin-1")))
        return out

    @staticmethod
    def _cstr(region: bytes, start: int, limit: int = 4096) -> str | None:
        if not (0 <= start < len(region)):
            return None
        end = region.find(b"\0", start, start + limit)
        if end == -1:
            return None
        return region[start:end].decode("latin-1")

    def pack_label(self, value: int) -> str:
        """Nome do .grp para um valor de `pack` (string NUL-terminada em pack_region[value:])."""
        if value == PACK_INVALID:
            return "INVALID"
        name = self._cstr(self.pack_region, value)
        return name if name else f"DESCONHECIDO({value})"

    def resource_name(self, offset: int) -> str | None:
        """Nome do recurso (hipótese: string NUL-terminada em resource_region[offset:])."""
        return self._cstr(self.resource_region, offset)

    def summary(self) -> dict:
        ent = self.entries
        types = Counter(e.type for e in ent)
        packs = Counter(e.pack for e in ent)
        return {
            "version": [self.v_major, self.v_minor],
            "entries": self.n_entries,
            "offset_packs": self.offset_packs,
            "offset_resources": self.offset_resources,
            "entries_end": self.entries_end,
            "file_size": self.file_size,
            "by_type": {RESOURCE_TYPES.get(t, f"DESCONHECIDO({t})"): {"type_id": t, "count": c}
                        for t, c in sorted(types.items())},
            "unknown_type_ids": sorted(t for t in types if t not in RESOURCE_TYPES),
            "by_pack": {self.pack_label(p): c for p, c in sorted(packs.items())},
            "unknown_pack_values": sorted(p for p in packs if self.pack_label(p).startswith("DESCONHECIDO")),
            "field_one_values": dict(Counter(e.one for e in ent).most_common(8)),
            "number_unique": len({e.number for e in ent}),
            "number_range": [min((e.number for e in ent), default=0), max((e.number for e in ent), default=0)],
            "offset_range": [min((e.offset for e in ent), default=0), max((e.offset for e in ent), default=0)],
            "pack_region_strings": self.pack_strings()[:40],
            "issues": self.issues,
        }


def parse_rht(buf) -> RhtInfo:
    size = len(buf)
    if size < HEADER_SIZE:
        raise RhtError(f"arquivo com {size} bytes, menor que o cabeçalho ({HEADER_SIZE})")
    v_major, v_minor, n, off_packs, off_res = _HEADER.unpack_from(buf, 0)
    end = HEADER_SIZE + ENTRY_SIZE * n
    if end > size:
        raise RhtError(f"tabela de entradas (nEntries={n}) não cabe no arquivo ({end} > {size})")
    entries = [RhtEntry(i, *_ENTRY.unpack_from(buf, HEADER_SIZE + ENTRY_SIZE * i)) for i in range(n)]
    info = RhtInfo(v_major, v_minor, n, off_packs, off_res, size, entries)

    if off_packs > size:
        info.issues.append(f"offsetPacks ({off_packs}) fora do arquivo")
    if off_res > size:
        info.issues.append(f"offsetResources ({off_res}) fora do arquivo")
    if off_packs <= size:
        stop = off_res if off_packs <= off_res <= size else size
        info.pack_region = bytes(buf[off_packs:stop])
    if off_res <= size:
        info.resource_region_size = size - off_res
        info.resource_region = bytes(buf[off_res:])
    for e in entries:
        if e.type not in RESOURCE_TYPES:
            info.issues.append(f"entrada {e.index}: tipo desconhecido {e.type}")
            break
    return info


def read_rht(path) -> RhtInfo:
    with open(path, "rb") as fh:
        return parse_rht(fh.read())
