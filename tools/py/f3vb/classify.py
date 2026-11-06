"""Classificação HEURÍSTICA do conteúdo de um lump (por magic). Os rótulos com '?'
são palpites fracos; servem para cruzar com os tipos do resource.rht."""
from __future__ import annotations

import struct

from .chunk import _tag_ok
from .knowledge import AMX_MAGICS, CHUNK_HEADER_SIZE


# Assinaturas de 16 bytes de arquivos Granny (constantes factuais, também presentes no
# blendergranny/MIT e na documentação pública do formato). Little e big-endian.
_GR2_WORDS = {
    "old": (0xCAB067B8, 0x0FB16DF8, 0x7E8C7284, 0x1E00195E),
    "32le": (0xC06CDE29, 0x2B53A4BA, 0xA5B7F525, 0xEEE266F6),
    "32be": (0xB595110E, 0x4BB5A56A, 0x502828EB, 0x04B37825),
    "64le": (0x5E499BE5, 0x141F636F, 0xA9EB131E, 0xC4EDBE90),
    "64be": (0xE3D49531, 0x624FDC20, 0x3AD036CC, 0x89FF82B1),
}
GR2_SIGNATURES = {}
for _name, _w in _GR2_WORDS.items():
    GR2_SIGNATURES[struct.pack("<4I", *_w)] = f"gr2 ({_name}, bytes LE)"
    GR2_SIGNATURES[struct.pack(">4I", *_w)] = f"gr2 ({_name}, bytes BE)"


def classify(data: bytes, total_size: int | None = None) -> str:
    """Classifica pelo começo do arquivo. `data` pode ser só o início (ex.: 64 bytes);
    passe `total_size` (tamanho real do lump) para validar tamanhos declarados (AMX, chunk)."""
    avail = len(data)
    total = avail if total_size is None else max(total_size, avail)
    if total == 0:
        return "vazio"
    if avail >= 16 and data[:16] in GR2_SIGNATURES:
        return "gr2"
    if data[:4] == b"B3D ":
        return "b3d"
    if data[:4] == b"8TRE":
        return "8tre"
    if avail >= 12 and data[:4] == b"RIFF" and data[8:12] == b"WAVE":
        return "wav"
    if avail >= 14 and data[:2] == b"BM":
        return "bmp?"
    if avail >= 8:
        size32 = struct.unpack_from("<i", data, 0)[0]
        magic16 = struct.unpack_from("<H", data, 4)[0]
        if magic16 in AMX_MAGICS and 0 < size32 <= total:
            return f"amx? ({AMX_MAGICS[magic16]})"
    if avail >= CHUNK_HEADER_SIZE and _tag_ok(data[:4]):
        _, version, csize = struct.unpack_from("<4sII", data, 0)
        if CHUNK_HEADER_SIZE <= csize <= total and version <= 1000:
            return f"chunk:{data[:4].decode('ascii')}"
    if avail >= 18 and data[1] in (0, 1) and data[2] in (1, 2, 3, 9, 10, 11) and data[16] in (8, 15, 16, 24, 32):
        w, h = struct.unpack_from("<HH", data, 12)
        if 0 < w <= 8192 and 0 < h <= 8192:
            return "tga?"
    sample = data[:512]
    if all(b in (9, 10, 13) or 32 <= b < 127 or b >= 128 for b in sample):
        try:
            sample.decode("utf-8")
            return "texto?"
        except UnicodeDecodeError:
            pass
    return "bin"
