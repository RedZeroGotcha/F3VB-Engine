"""Tabela de strings (.stf): textos localizados usados pelos diálogos (ids como 3113 vistos nos scripts).

Layout derivado do stf.pat (OpenJE/patterns); VERIFICADO no English.stf real (3.281 entradas; entrada 0 = 'Single Player').
v19: os ids de texto citados pelos .int (rotulos) sao 1-BASED: id N = entrada N-1, id 0 = sem texto (medido: rotulo do botao QuitBtn = id 16 -> entrada 15 'Quit';
aba Load = 530 -> 'Load/Save'; botoes da MessageBox 11/12 -> 'YES'/'NO'). Use text_id() para esses ids. Layout:
  u8 magic[8], u32 count, count x { u32 stringOffset, u32 stringLength, u8 constant[8] }
Hipóteses: id do texto = índice da entrada; o texto está em stringOffset (absoluto) com stringLength unidades;
a codificação é decidida pelos dados (UTF-16LE se os bytes alternam com zeros; senão cp1252).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass

ENTRY = struct.Struct("<II8s")


class StfError(ValueError):
    pass


@dataclass
class Stf:
    magic: bytes
    count: int
    entries: list

    def text(self, data: bytes, i: int) -> str:
        if not 0 <= i < self.count:
            raise StfError(f"id {i} fora de 0..{self.count - 1}")
        off, length, _ = self.entries[i]
        for width in (1, 2):                       # tenta length em bytes; se não couber, em unidades de 2 bytes
            raw = data[off:off + length * width]
            if len(raw) == length * width:
                break
        if len(raw) >= 2 and len(raw) % 2 == 0 and raw[1::2].count(0) >= len(raw) // 2 - 1 and b"\x00" in raw[1::2]:
            return raw.decode("utf-16-le", "replace").rstrip("\x00")
        return raw.decode("cp1252", "replace").rstrip("\x00")


    def text_id(self, data: bytes, text_id: int) -> str:
        """v19: id 1-based (como nos .int); "" para 0 ou fora da tabela."""
        if not 1 <= text_id <= self.count:
            return ""
        return self.text(data, text_id - 1)


def parse_stf(data: bytes) -> Stf:
    if len(data) < 12:
        raise StfError("arquivo menor que o cabeçalho (12 bytes)")
    magic = data[:8]
    count = struct.unpack_from("<I", data, 8)[0]
    if 12 + ENTRY.size * count > len(data):
        raise StfError(f"{count} entradas não cabem no arquivo ({len(data)} bytes)")
    entries = [ENTRY.unpack_from(data, 12 + ENTRY.size * i) for i in range(count)]
    return Stf(magic, count, entries)
