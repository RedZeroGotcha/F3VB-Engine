"""Enquadramento de chunk: tag[4] + version(u32) + size(u32, inclui o cabeçalho)."""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Iterator

from .knowledge import CHUNK_HEADER_SIZE

_HEADER = struct.Struct("<4sII")


class ChunkError(ValueError):
    pass


@dataclass(frozen=True)
class Chunk:
    tag: str
    version: int
    size: int      # total, inclui o cabeçalho de 12 bytes
    offset: int    # posição do cabeçalho no buffer

    @property
    def payload_offset(self) -> int:
        return self.offset + CHUNK_HEADER_SIZE

    @property
    def payload_size(self) -> int:
        return self.size - CHUNK_HEADER_SIZE


def _tag_ok(raw: bytes) -> bool:
    return all(32 <= b < 127 for b in raw)


def read_chunk(buf, pos: int = 0, end: int | None = None) -> Chunk:
    """Lê um cabeçalho em `pos` e valida contra `end`. Levanta ChunkError."""
    end = len(buf) if end is None else end
    if pos < 0 or pos + CHUNK_HEADER_SIZE > end:
        raise ChunkError(f"cabeçalho incompleto em {pos}")
    raw, version, size = _HEADER.unpack_from(buf, pos)
    if not _tag_ok(raw):
        raise ChunkError(f"tag não imprimível em {pos}: {raw!r}")
    if size < CHUNK_HEADER_SIZE:
        raise ChunkError(f"size {size} menor que o cabeçalho em {pos}")
    if pos + size > end:
        raise ChunkError(f"chunk em {pos} ultrapassa o buffer (size={size})")
    return Chunk(raw.decode("ascii"), version, size, pos)


def iter_chunks(buf, start: int = 0, end: int | None = None) -> Iterator[Chunk]:
    """Percorre chunks consecutivos. Para no primeiro erro de enquadramento."""
    end = len(buf) if end is None else end
    pos = start
    while pos + CHUNK_HEADER_SIZE <= end:
        chunk = read_chunk(buf, pos, end)
        yield chunk
        pos += chunk.size
    if pos != end:
        raise ChunkError(f"sobraram {end - pos} bytes após o último chunk")


def plausible_chunk_at(buf, pos: int, tag: bytes, max_version: int = 1000,
                       max_size: int = 256 * 1024 * 1024) -> bool:
    """Filtro barato para varredura sem layout: version e size dentro do razoável."""
    n = len(buf)
    if pos < 0 or pos + CHUNK_HEADER_SIZE > n:
        return False
    raw, version, size = _HEADER.unpack_from(buf, pos)
    return (raw == tag and version <= max_version
            and CHUNK_HEADER_SIZE <= size <= max_size and pos + size <= n)
