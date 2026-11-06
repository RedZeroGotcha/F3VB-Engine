"""Leitor/escritor de `.rle` (recurso tipo 1700 "RLE"; um por mapa): a GRADE DE TILES do mapa, comprimida por runs.

Formato (MEDIDO em 38 arquivos reais; re-codificar a grade reproduz os 38 arquivos byte a byte):
    u32 a, u32 b, u32 c      a = largura em celulas, b = linhas + 1, c = a (igual a `a` nos 38 arquivos; significado ainda desconhecido)
    u8  fill                 sempre 0x2F ('/'), o valor "vazio"
    N x { u32 contagem, u8 valor }   runs, linha por linha; nenhum run atravessa o fim de uma linha
A grade tem `a` colunas e `b - 1` linhas (a soma das contagens e exatamente a * (b - 1) em todos os arquivos).
`valor` = id do tile (0..44; 47 = vazio). Hipotese: id = 11 * nivel + tipo (so aparecem 0, 11, 22, 33, 44 + tipo 0..10).
O desenho dos mapas grandes e em losango (isometrico), com cantos vazios.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass

from .imagecodec import encode_png

EMPTY = 0x2F


class RleError(ValueError):
    pass


@dataclass
class RleGrid:
    width: int           # a
    rows: int            # b - 1
    dim_c: int           # c (igual a `a` em todos os arquivos medidos)
    fill: int
    cells: bytes         # rows * width, linha por linha

    def at(self, col: int, row: int) -> int:
        return self.cells[row * self.width + col]

    def is_empty(self, col: int, row: int) -> bool:
        return self.at(col, row) == EMPTY

    def histogram(self) -> dict:
        h: dict = {}
        for v in self.cells:
            h[v] = h.get(v, 0) + 1
        return dict(sorted(h.items()))

    def bbox(self):
        """(col_min, row_min, col_max, row_max) das celulas nao vazias, ou None."""
        cmin = rmin = 1 << 30
        cmax = rmax = -1
        for r in range(self.rows):
            row = self.cells[r * self.width:(r + 1) * self.width]
            for c, v in enumerate(row):
                if v != EMPTY:
                    cmin, cmax = min(cmin, c), max(cmax, c)
                    rmin, rmax = min(rmin, r), max(rmax, r)
        return None if cmax < 0 else (cmin, rmin, cmax, rmax)


def read_rle(data: bytes) -> RleGrid:
    if len(data) < 13:
        raise RleError("arquivo menor que o cabecalho (13 bytes)")
    a, b, c = struct.unpack_from("<3I", data, 0)
    fill = data[12]
    if a == 0 or b < 1 or a > 1 << 16 or b > 1 << 16:
        raise RleError(f"dimensoes absurdas ({a}, {b}, {c})")
    rows = b - 1
    total = a * rows
    out = bytearray()
    pos = 13
    while pos < len(data):
        if pos + 5 > len(data):
            raise RleError(f"run incompleto em {pos}")
        n, = struct.unpack_from("<I", data, pos)
        v = data[pos + 4]
        if n == 0 or len(out) + n > total:
            raise RleError(f"run de {n} em {pos} estoura a grade ({len(out)}/{total})")
        if (len(out) % a) + n > a:
            raise RleError(f"run em {pos} atravessa o fim da linha")
        out.extend(bytes([v]) * n)
        pos += 5
    if len(out) != total:
        raise RleError(f"celulas {len(out)} != {a} x {rows} = {total}")
    return RleGrid(a, rows, c, fill, bytes(out))


def write_rle(g: RleGrid) -> bytes:
    out = bytearray(struct.pack("<3I", g.width, g.rows + 1, g.dim_c))
    out.append(g.fill)
    for r in range(g.rows):
        row = g.cells[r * g.width:(r + 1) * g.width]
        i = 0
        while i < g.width:
            j = i
            while j < g.width and row[j] == row[i]:
                j += 1
            out += struct.pack("<IB", j - i, row[i])
            i = j
    return bytes(out)


def tile_color(v: int) -> tuple:
    """Cor de depuracao por id de tile: matiz pelo tipo (v % 11), brilho pelo nivel (v // 11). Vazio = quase preto."""
    if v == EMPTY:
        return (12, 14, 20)
    hue = (v % 11) / 11.0
    lvl = v // 11
    s, l = 0.55, 0.30 + 0.11 * min(lvl, 4)
    # HSL -> RGB
    q = l * (1 + s) if l < 0.5 else l + s - l * s
    p = 2 * l - q

    def f(t):
        t %= 1.0
        if t < 1 / 6: return p + (q - p) * 6 * t
        if t < 1 / 2: return q
        if t < 2 / 3: return p + (q - p) * (2 / 3 - t) * 6
        return p
    return tuple(int(255 * f(hue + d)) for d in (1 / 3, 0, -1 / 3))


def rle_to_rgba(g: RleGrid) -> bytes:
    pal = {v: tile_color(v) for v in set(g.cells)}
    out = bytearray()
    for v in g.cells:
        r, gg, b = pal[v]
        out += bytes((r, gg, b, 255))
    return bytes(out)


def rle_to_png(g: RleGrid) -> bytes:
    return encode_png(g.width, g.rows, rle_to_rgba(g))
