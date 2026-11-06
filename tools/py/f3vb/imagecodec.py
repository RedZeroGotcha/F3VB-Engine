"""Decodificadores puros (sem Pillow/numpy) de TGA e BMP e codificador PNG, para embutir texturas no GLB."""
from __future__ import annotations

import struct
import zlib


class ImageError(ValueError):
    pass


def _expand_rle(data: bytes, pos: int, pixels: int, bpp_bytes: int) -> bytes:
    out = bytearray()
    n = len(data)
    while len(out) < pixels * bpp_bytes:
        if pos >= n:
            raise ImageError("TGA RLE truncado")
        h = data[pos]
        pos += 1
        count = (h & 0x7F) + 1
        if h & 0x80:
            if pos + bpp_bytes > n:
                raise ImageError("TGA RLE truncado")
            out += data[pos:pos + bpp_bytes] * count
            pos += bpp_bytes
        else:
            end = pos + count * bpp_bytes
            if end > n:
                raise ImageError("TGA RLE truncado")
            out += data[pos:end]
            pos = end
    return bytes(out[:pixels * bpp_bytes])


def decode_tga(data: bytes) -> tuple[int, int, bytes]:
    """Devolve (largura, altura, RGBA8 linha a linha de CIMA para BAIXO)."""
    if len(data) < 18:
        raise ImageError("TGA menor que o cabeçalho")
    idlen, cmap_type, itype = data[0], data[1], data[2]
    cm_first, cm_len, cm_bits = struct.unpack_from("<HHB", data, 3)
    w, h, bpp, desc = struct.unpack_from("<HHBB", data, 12)
    if w == 0 or h == 0 or w > 16384 or h > 16384:
        raise ImageError(f"dimensões inválidas {w}x{h}")
    pos = 18 + idlen
    palette = None
    if cmap_type:
        cb = (cm_bits + 7) // 8
        pal_raw = data[pos:pos + cm_len * cb]
        pos += cm_len * cb
        palette = []
        for i in range(cm_len):
            c = pal_raw[i * cb:(i + 1) * cb]
            if cb == 3:
                palette.append((c[2], c[1], c[0], 255))
            elif cb == 4:
                palette.append((c[2], c[1], c[0], c[3]))
            elif cb == 2:
                v = c[0] | (c[1] << 8)
                palette.append((((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, 255))
            else:
                raise ImageError("paleta TGA não suportada")
    base = itype & 7
    rle = bool(itype & 8)
    if base not in (1, 2, 3):
        raise ImageError(f"tipo de TGA não suportado: {itype}")
    bb = (bpp + 7) // 8
    if base == 1 and palette is None:
        raise ImageError("TGA paletizado sem paleta")
    raw = _expand_rle(data, pos, w * h, bb) if rle else data[pos:pos + w * h * bb]
    if len(raw) < w * h * bb:
        raise ImageError("dados de pixel truncados")
    rgba = bytearray(w * h * 4)
    if base == 2:
        if bpp == 24:
            rgba[0::4], rgba[1::4], rgba[2::4] = raw[2::3], raw[1::3], raw[0::3]
            rgba[3::4] = b"\xff" * (w * h)
        elif bpp == 32:
            rgba[0::4], rgba[1::4], rgba[2::4], rgba[3::4] = raw[2::4], raw[1::4], raw[0::4], raw[3::4]
        elif bpp in (15, 16):
            for i in range(w * h):
                v = raw[2 * i] | (raw[2 * i + 1] << 8)
                rgba[4 * i:4 * i + 4] = bytes((((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, 255))
        else:
            raise ImageError(f"bpp {bpp} não suportado")
    elif base == 3:
        if bpp != 8:
            raise ImageError("TGA cinza só em 8 bpp")
        rgba[0::4] = rgba[1::4] = rgba[2::4] = raw[:w * h]
        rgba[3::4] = b"\xff" * (w * h)
    else:
        if bpp != 8:
            raise ImageError("TGA paletizado só em 8 bpp")
        for i in range(w * h):
            idx = raw[i] - cm_first
            rgba[4 * i:4 * i + 4] = bytes(palette[idx] if 0 <= idx < len(palette) else (0, 0, 0, 255))
    if not (desc & 0x20):                                   # origem embaixo -> inverter linhas
        row = w * 4
        rgba = bytearray(b"".join(bytes(rgba[r * row:(r + 1) * row]) for r in range(h - 1, -1, -1)))
    return w, h, bytes(rgba)


def decode_bmp(data: bytes) -> tuple[int, int, bytes]:
    if len(data) < 54 or data[:2] != b"BM":
        raise ImageError("não é BMP")
    off, dib = struct.unpack_from("<II", data, 10)
    w, h, planes, bpp, comp = struct.unpack_from("<iiHHI", data, 18)
    if comp not in (0, 3) or bpp not in (8, 24, 32) or w <= 0 or h == 0:
        raise ImageError(f"BMP não suportado (bpp={bpp}, compressão={comp})")
    top_down = h < 0
    h = abs(h)
    pal = []
    if bpp == 8:
        n = struct.unpack_from("<I", data, 46)[0] or 256
        po = 14 + dib
        pal = [(data[po + 4 * i + 2], data[po + 4 * i + 1], data[po + 4 * i], 255) for i in range(n)]
    stride = ((w * bpp + 31) // 32) * 4
    out = bytearray()
    rows = range(h) if top_down else range(h - 1, -1, -1)
    for r in rows:
        line = data[off + r * stride:off + r * stride + stride]
        if len(line) < stride:
            raise ImageError("BMP truncado")
        for x in range(w):
            if bpp == 24:
                out += bytes((line[3 * x + 2], line[3 * x + 1], line[3 * x], 255))
            elif bpp == 32:
                out += bytes((line[4 * x + 2], line[4 * x + 1], line[4 * x], line[4 * x + 3] or 255))
            else:
                out += bytes(pal[line[x]] if line[x] < len(pal) else (0, 0, 0, 255))
    return w, h, bytes(out)


def encode_png(w: int, h: int, rgba: bytes) -> bytes:
    if len(rgba) != w * h * 4:
        raise ImageError("tamanho RGBA inconsistente")
    # descarta o canal alfa se for todo 255 (arquivo menor)
    opaque = rgba[3::4].count(255) == w * h
    if opaque:
        rows = b"".join(b"\x00" + b"".join(rgba[(y * w + x) * 4:(y * w + x) * 4 + 3] for x in range(w)) for y in range(h)) \
            if w * h <= 4096 else None
        if rows is None:
            rgb = bytearray(w * h * 3)
            rgb[0::3], rgb[1::3], rgb[2::3] = rgba[0::4], rgba[1::4], rgba[2::4]
            rows = b"".join(b"\x00" + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))
        ctype, ch = 2, 3
    else:
        rows = b"".join(b"\x00" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
        ctype, ch = 6, 4

    def chunk(tag: bytes, body: bytes) -> bytes:
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, ctype, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))


def image_to_png(data: bytes) -> tuple[int, int, bytes]:
    """TGA ou BMP -> (largura, altura, bytes PNG)."""
    w, h, rgba = decode_bmp(data) if data[:2] == b"BM" else decode_tga(data)
    return w, h, encode_png(w, h, rgba)
