"""Leitor de .grp (layout derivado de grp.pat; a confirmar em arquivo real).

    u32 vMajor, u32 vMinor, u32 nFiles
    nFiles x { u32 offset, u32 length }     (tabela logo após o cabeçalho)
    dados dos lumps em `offset` (absoluto desde o início do arquivo)

Little-endian. O .grp NÃO guarda nomes: nome/tipo vêm do resource.rht.
"""
from __future__ import annotations

import mmap
import struct
from dataclasses import dataclass, field
from pathlib import Path

_HEADER = struct.Struct("<III")
_LUMP = struct.Struct("<II")
HEADER_SIZE = _HEADER.size
LUMP_ENTRY_SIZE = _LUMP.size


class GrpError(ValueError):
    pass


@dataclass(frozen=True)
class Lump:
    index: int
    offset: int
    length: int

    @property
    def end(self) -> int:
        return self.offset + self.length


@dataclass
class GrpInfo:
    v_major: int
    v_minor: int
    n_files: int
    file_size: int
    lumps: list[Lump]
    issues: list[str] = field(default_factory=list)

    @property
    def table_end(self) -> int:
        return HEADER_SIZE + LUMP_ENTRY_SIZE * self.n_files


def parse_grp(buf) -> GrpInfo:
    size = len(buf)
    if size < HEADER_SIZE:
        raise GrpError(f"arquivo com {size} bytes, menor que o cabeçalho ({HEADER_SIZE})")
    v_major, v_minor, n_files = _HEADER.unpack_from(buf, 0)
    table_end = HEADER_SIZE + LUMP_ENTRY_SIZE * n_files
    if table_end > size:
        raise GrpError(
            f"tabela de lumps (nFiles={n_files}) não cabe no arquivo ({table_end} > {size}); "
            "provavelmente não é um .grp neste layout")
    lumps = [Lump(i, *_LUMP.unpack_from(buf, HEADER_SIZE + LUMP_ENTRY_SIZE * i))
             for i in range(n_files)]
    info = GrpInfo(v_major, v_minor, n_files, size, lumps)

    for lp in lumps:
        if lp.end > size:
            info.issues.append(f"lump {lp.index}: fora do arquivo (offset={lp.offset}, length={lp.length})")
        elif lp.length and lp.offset < table_end:
            info.issues.append(f"lump {lp.index}: começa dentro da tabela (offset={lp.offset})")
    ordered = sorted((lp for lp in lumps if lp.length and lp.end <= size), key=lambda l: l.offset)
    for a, b in zip(ordered, ordered[1:]):
        if b.offset < a.end:
            info.issues.append(f"lumps {a.index} e {b.index} se sobrepõem")
    return info


class GrpFile:
    """Abre um .grp com mmap (somente leitura). Use como context manager."""

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self._fh = self.path.open("rb")
        try:
            self._mm = mmap.mmap(self._fh.fileno(), 0, access=mmap.ACCESS_READ)
        except ValueError as exc:  # arquivo vazio
            self._fh.close()
            raise GrpError(f"{self.path}: arquivo vazio") from exc
        try:
            self.info = parse_grp(self._mm)
        except GrpError:
            self.close()
            raise

    def read(self, lump: Lump | int) -> bytes:
        lp = self.info.lumps[lump] if isinstance(lump, int) else lump
        if lp.end > self.info.file_size:
            raise GrpError(f"lump {lp.index} fora do arquivo")
        return self._mm[lp.offset:lp.end]

    def head(self, lump: Lump | int, n: int = 64) -> bytes:
        lp = self.info.lumps[lump] if isinstance(lump, int) else lump
        end = min(lp.end, self.info.file_size, lp.offset + n)
        return self._mm[lp.offset:end]

    def close(self) -> None:
        try:
            self._mm.close()
        finally:
            self._fh.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
