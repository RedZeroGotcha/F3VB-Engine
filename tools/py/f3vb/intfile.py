"""Formato `.int` (recurso INT_Interface): o layout de uma tela da interface. DECODIFICADO no v19 (25 de 25 arquivos reais lidos ate o ultimo byte).

Spec completa: docs/specs/int.md. Resumo (little-endian; str = u32 comprimento + bytes, SEM terminador):
    "GUI V1.x" (8 bytes; x = 1, 2 ou 3)   str nome_do_arquivo   u8 ?   u32 N_raizes   N x objeto
    objeto = u32 tipo  prefixo(tipo)  corpo  9 x fragmento  i32 (-1)  u32 N_filhos  N_filhos x objeto  sufixo(tipo)
      tipo: 0 janela, 1 botao, 2 imagem, 3 rotulo (texto), 4 campo de edicao
      prefixo do botao: u32 ?  [V1.3: u8 ?, 3 x str sons (apertar, soltar, soltar)]
      corpo: i32[4] retangulo ABSOLUTO na tela de 1024x768 (x0, y0, x1, y1), str nome, u8 a, u8 b, u8 c, str skin (.ini),
             u8[4] cor (rgb + byte), i32[4] retangulo relativo ao pai, u8 ?, u32 interativo (1 em janelas/botoes/campos, 0 em imagens/rotulos: hipotese; NAO e visibilidade),
             [V1.2+: u32 ?, i32 id_a, u32 ?, i32 id_dica, str textura_da_dica]
      fragmento (9, na ordem upperleft, uppermiddle, upperright, left, middle, right, lowerleft, lowermiddle, lowerright):
             u32 flags (0 esticar, 1 repetir em x, 2 repetir em y, 3 repetir em x e y), i32 largura, i32 altura, i32 ?, i32 ?,
             u32 ? (as vezes lixo de memoria), str textura, f32 ?, u8[4] cor RGBA, i32[4] retangulo ABSOLUTO
      sufixo do rotulo/campo (depois dos filhos!): u32 id_do_texto (1-based no .stf; 0 = sem texto), str fonte, u32 ?, u32 tamanho, u32 ?,
             u8[4] cor RGBA do texto, 22 bytes ?, u32 alinhamento (0 esquerda, 1 centro, 2 direita: hipotese)
O que cada campo "?" significa ainda nao se sabe; o leitor guarda os valores crus (raw) para estudo.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field

TYPE_NAMES = {0: "janela", 1: "botao", 2: "imagem", 3: "rotulo", 4: "edicao"}
FRAGMENT_NAMES = ("upperleft", "uppermiddle", "upperright", "left", "middle", "right", "lowerleft", "lowermiddle", "lowerright")
MAX_STR = 4096
MAX_CHILDREN = 4096
MAX_DEPTH = 32


class IntError(ValueError):
    pass


@dataclass
class Fragment:
    flags: int
    width: int
    height: int
    unk_a: int
    unk_b: int
    unk_c: int
    texture: str
    unk_f: float
    color: tuple          # (r, g, b, a)
    rect: tuple           # absoluto, tela 1024x768


@dataclass
class Widget:
    kind: int
    name: str
    rect: tuple           # (x0, y0, x1, y1) absoluto na tela de 1024x768
    rel: tuple            # retangulo relativo ao pai
    skin: str
    color: tuple          # 4 bytes crus depois da skin
    flags3: tuple         # os 3 bytes depois do nome
    interactive: int
    tip_a: int = -1
    tip_text: int = -1
    tip_texture: str = ""
    fragments: list = field(default_factory=list)
    children: list = field(default_factory=list)
    # botao
    sounds: tuple = ()
    button_unk: int = 0
    # rotulo / edicao
    text_id: int = 0      # 1-based no .stf (0 = sem texto)
    font: str = ""
    font_size: int = 0
    text_color: tuple = ()
    align: int = 0
    raw: dict = field(default_factory=dict)

    @property
    def type_name(self) -> str:
        return TYPE_NAMES.get(self.kind, f"tipo{self.kind}")

    def walk(self, depth: int = 0):
        yield depth, self
        for c in self.children:
            yield from c.walk(depth + 1)


@dataclass
class IntFile:
    version: int          # 11, 12 ou 13
    name: str
    roots: list

    def walk(self):
        for r in self.roots:
            yield from r.walk(0)

    def count(self) -> int:
        return sum(1 for _ in self.walk())

    def find(self, name: str):
        low = name.lower()
        return next((w for _d, w in self.walk() if w.name.lower() == low), None)


class _R:
    def __init__(self, data: bytes):
        self.d, self.o = data, 0

    def need(self, n: int) -> None:
        if self.o + n > len(self.d):
            raise IntError(f"fim do arquivo em {self.o} (precisava de {n} bytes)")

    def u8(self) -> int:
        self.need(1)
        v = self.d[self.o]
        self.o += 1
        return v

    def u32(self) -> int:
        self.need(4)
        v = struct.unpack_from("<I", self.d, self.o)[0]
        self.o += 4
        return v

    def i32(self) -> int:
        self.need(4)
        v = struct.unpack_from("<i", self.d, self.o)[0]
        self.o += 4
        return v

    def f32(self) -> float:
        self.need(4)
        v = struct.unpack_from("<f", self.d, self.o)[0]
        self.o += 4
        return v

    def raw(self, n: int) -> bytes:
        self.need(n)
        v = self.d[self.o:self.o + n]
        self.o += n
        return v

    def rect(self) -> tuple:
        self.need(16)
        v = struct.unpack_from("<4i", self.d, self.o)
        self.o += 16
        return v

    def s(self) -> str:
        n = self.u32()
        if n > MAX_STR:
            raise IntError(f"string de {n} bytes em {self.o - 4} (absurda)")
        return self.raw(n).decode("latin-1")


def _fragment(r: _R) -> Fragment:
    fl, w, h, a, b = (r.i32() for _ in range(5))
    c = r.u32()
    tex = r.s()
    f = r.f32()
    col = tuple(r.raw(4))
    return Fragment(fl, w, h, a, b, c, tex, f, col, r.rect())


def _widget(r: _R, kind: int, ver: int, depth: int) -> Widget:
    if depth > MAX_DEPTH:
        raise IntError("arvore funda demais")
    if kind not in TYPE_NAMES:
        raise IntError(f"tipo de objeto desconhecido {kind} em {r.o - 4}")
    sounds: tuple = ()
    bunk = 0
    raw: dict = {}
    if kind == 1:
        bunk = r.u32()
        if ver >= 13:
            raw["button_u8"] = r.u8()
            sounds = (r.s(), r.s(), r.s())
    rect = r.rect()
    name = r.s()
    f3 = (r.u8(), r.u8(), r.u8())
    skin = r.s()
    color = tuple(r.raw(4))
    rel = r.rect()
    raw["body_u8"] = r.u8()
    interactive = r.u32()
    w = Widget(kind, name, rect, rel, skin, color, f3, interactive, sounds=sounds, button_unk=bunk, raw=raw)
    if ver >= 12:
        raw["h1"] = r.u32()
        w.tip_a = r.i32()
        raw["h2"] = r.u32()
        w.tip_text = r.i32()
        w.tip_texture = r.s()
    w.fragments = [_fragment(r) for _ in range(9)]
    raw["end"] = r.i32()
    nch = r.u32()
    if nch > MAX_CHILDREN:
        raise IntError(f"{nch} filhos em {r.o - 4} (absurdo)")
    for _ in range(nch):
        w.children.append(_widget(r, r.u32(), ver, depth + 1))
    if kind in (3, 4):
        w.text_id = r.u32()
        w.font = r.s()
        raw["font_a"] = r.u32()
        w.font_size = r.u32()
        raw["font_b"] = r.u32()
        w.text_color = tuple(r.raw(4))
        raw["label_tail"] = r.raw(22).hex()
        w.align = r.u32()
    return w


def read_int(data: bytes) -> IntFile:
    if len(data) < 12 or data[:5] != b"GUI V" or data[6:7] != b".":
        raise IntError("nao e um .int (assinatura 'GUI V1.x' ausente)")
    try:
        ver = int(data[5:6]) * 10 + int(data[7:8])
    except ValueError as exc:
        raise IntError(f"versao ilegivel {data[:8]!r}") from exc
    if ver not in (11, 12, 13):
        raise IntError(f"versao {data[:8]!r} nao suportada (conhecidas: V1.1, V1.2, V1.3)")
    r = _R(data)
    r.o = 8
    name = r.s()
    r.u8()
    nroots = r.u32()
    if nroots > MAX_CHILDREN:
        raise IntError(f"{nroots} raizes (absurdo)")
    roots = [_widget(r, r.u32(), ver, 0) for _ in range(nroots)]
    if r.o != len(data):
        raise IntError(f"sobraram {len(data) - r.o} bytes depois da arvore (leitura terminou em {r.o})")
    return IntFile(ver, name, roots)


# ------------------------------------------------------------------------------------------- escritor (testes sinteticos)
def _ws(t: str) -> bytes:
    b = t.encode("latin-1")
    return struct.pack("<I", len(b)) + b


def build_widget(w: Widget, ver: int = 13) -> bytes:
    out = [struct.pack("<I", w.kind)]
    if w.kind == 1:
        out.append(struct.pack("<I", w.button_unk))
        if ver >= 13:
            snd = tuple(w.sounds) + ("",) * (3 - len(w.sounds))
            out.append(struct.pack("<B", w.raw.get("button_u8", 0)) + b"".join(_ws(x) for x in snd[:3]))
    out += [struct.pack("<4i", *w.rect), _ws(w.name), bytes(w.flags3), _ws(w.skin), bytes(w.color), struct.pack("<4i", *w.rel),
            struct.pack("<BI", w.raw.get("body_u8", 0), w.interactive)]
    if ver >= 12:
        out += [struct.pack("<IiIi", w.raw.get("h1", 0), w.tip_a, w.raw.get("h2", 0), w.tip_text), _ws(w.tip_texture)]
    frags = list(w.fragments) + [Fragment(0, 8, 8, 0, 0, 0, "", 0.0, (128, 128, 128, 255), w.rect)] * (9 - len(w.fragments))
    for f in frags[:9]:
        out += [struct.pack("<5iI", f.flags, f.width, f.height, f.unk_a, f.unk_b, f.unk_c), _ws(f.texture), struct.pack("<f", f.unk_f), bytes(f.color),
                struct.pack("<4i", *f.rect)]
    out.append(struct.pack("<iI", w.raw.get("end", -1), len(w.children)))
    out += [build_widget(c, ver) for c in w.children]
    if w.kind in (3, 4):
        tail = bytes.fromhex(w.raw.get("label_tail", "00" * 22))
        out += [struct.pack("<I", w.text_id), _ws(w.font), struct.pack("<III", w.raw.get("font_a", 0), w.font_size, w.raw.get("font_b", 1)),
                bytes(w.text_color or (255, 255, 255, 255)), tail[:22].ljust(22, b"\0"), struct.pack("<I", w.align)]
    return b"".join(out)


def build_int(name: str, roots: list, ver: int = 13) -> bytes:
    return b"GUI V1.%d" % (ver % 10) + _ws(name) + b"\0" + struct.pack("<I", len(roots)) + b"".join(build_widget(w, ver) for w in roots)


# ------------------------------------------------------------------------------------------- relatorio
def describe(f: IntFile, stf=None, max_lines: int = 100000) -> list:
    """Arvore legivel. stf: funcao id(1-based) -> texto, opcional."""
    lines = [f"{f.name}: GUI V1.{f.version % 10}, {len(f.roots)} raiz(es), {f.count()} objetos"]
    for depth, w in f.walk():
        if len(lines) >= max_lines:
            lines.append("  ...")
            break
        x0, y0, x1, y1 = w.rect
        tex = [fr.texture for fr in w.fragments if fr.texture and not fr.texture.lower().startswith(("zz", "quickslot", "main-", "defaultinterface", "f3_green", "areamap"))]
        extra = ""
        if w.kind in (3, 4):
            txt = stf(w.text_id) if (stf and w.text_id) else ""
            extra = f" texto#{w.text_id}{(' ' + repr(txt)) if txt else ''} fonte '{w.font}' {w.font_size} cor {bytes(w.text_color).hex()} alinh {w.align}"
        if w.kind == 1 and w.sounds:
            extra += f" sons {'/'.join(s for s in w.sounds if s)}"
        if w.tip_texture or w.tip_text not in (-1, 0):
            extra += f" dica#{w.tip_text} {w.tip_texture}"
        vis = ""
        lines.append(f"{'  ' * (depth + 1)}{w.type_name} {w.name} [{x0},{y0} {x1 - x0}x{y1 - y0}] skin={w.skin}{vis}{(' img=' + ','.join(tex)) if tex else ''}{extra}")
    return lines
