"""Leitor de `.8tre` (recurso tipo 400 "TRE_Octree"): geometria 3D estatica de um mapa, organizada em octree.

Formato (MEDIDO em 38 arquivos reais, 106 MB; todos tokenizam ate o ultimo byte). Chunks = tag[4] + u32 tamanho do PAYLOAD (cabecalho de 8 bytes, aninhados):
  8TRE (raiz; o tamanho cobre o arquivo menos 8 menos 4 x numero de folhas)
    HEAD  4 x u32 (2, ?, ?, ?)                            versao 2; o resto sem nome
    LGTD  INFO{u32 N}  + N x LGTR (64 B: tipo, flags, cor, posicao, direcao, 2 f32, cor 2)   luzes (opcional)
    LVLD  INFO(16 B: u32 largura, u32 altura, f32, f32 = 84, 84, 0.5, 0.5 no MarkTest) + LVL0..: grade de niveis (NAO decodificada)
    MATD  INFO{u32 N} + N x MATR (196 B: u32, nome[80], RGBA, mistura[16], superficie[16], floats, flags)
    TXTD  u32, u32 + TXTR{nome\\0}...                      nomes de textura (.tga) e de lightmap (.ctx)
    VTXD  u32 N blocos + N x VTXB{u32 nv, "XYZ " (3 f32), "NRML" (3 f32), "DIFU" (RGBA8), "TXUV" (2 f32), "LMUV" (2 f32)} (cada fluxo: tag[4] + u32 bytes)
    TREE  sequencia PLANA de NODE e LEAF, na ordem de busca em profundidade (a hierarquia nao e necessaria para desenhar):
      NODE/LEAF = INFO + [IDXD{IDXS, FLGS}] (+ bytes extras so nas folhas)
        INFO NODE (64 B): caixa min/max (6 f32), u32 0, u32 mascara/contagem, 4 x u32 (ENDERECOS de memoria do jogo, lixo), zeros
        INFO LEAF (32 B): caixa min/max (6 f32), u32 BLOCO de vertices, u32 1
        IDXS: 3 x u32 (lixo/enderecos) + u16 indices; triangulos = floor(n / 3)
      O tamanho declarado de cada LEAF vem 4 bytes MENOR que o conteudo real (os 4 bytes finais pertencem a folha): a proxima ficha
      comeca em inicio + 8 + declarado + 4. Para NODE o tamanho e exato.
IDXS e um fluxo de LOTES (ver o bloco "fluxo de indices em lotes" mais abaixo): cada lote tem material, pagina de lightmap e textura difusa.
Indices: u16. Os 2 bits altos de alguns u16 NAO sao flags de indice: sao CABECALHOS DE LOTE (0x8000 = material, 0x4000 = pagina de lightmap) no meio do fluxo
(gramatica no bloco "fluxo de indices em lotes" abaixo; medido em 38 arquivos: 0 nos que nao fecham, 0 indices fora da caixa da folha). `triangles()` ainda aplica a mascara
0x3FFF como rede de seguranca quando um indice cai fora do bloco ou da caixa da folha. NAO decifrado: o significado de FLGS (so a contagem: 1 byte por triangulo), os bytes
extras das folhas, LVL0.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass, field


class TreError(ValueError):
    pass


def _is_tag(b: bytes) -> bool:
    return len(b) == 4 and all(65 <= c <= 90 or 48 <= c <= 57 or c == 95 for c in b)


def _chunks(d: bytes, s: int, e: int):
    out, p = [], s
    while p < e:
        if p + 8 > e or not _is_tag(d[p:p + 4]):
            raise TreError(f"chunk invalido em {p}")
        n = struct.unpack_from("<I", d, p + 4)[0]
        if p + 8 + n > e:
            raise TreError(f"chunk {d[p:p + 4]!r} em {p} ultrapassa o pai")
        out.append((d[p:p + 4].decode(), p + 8, n))
        p += 8 + n
    return out


@dataclass
class VtxBlock:
    count: int
    xyz: tuple          # 3 * count floats
    normals: tuple      # 3 * count floats (vazio se ausente)
    colors: bytes       # 4 * count (RGBA) ou b""
    uv: tuple           # 2 * count
    lightmap_uv: tuple  # 2 * count


@dataclass
class TriBatch:
    material: int       # indice em MATD (0 = lote inicial, sem cabecalho)
    page: int           # pagina de lightmap, 1-based na lista ".ctx" (0 = nenhuma)
    texture: int        # textura difusa, 1-based na lista ".tga" (0 = nenhuma)
    indices: tuple      # u16 brutos, multiplos de 3


@dataclass
class TreNode:
    kind: str           # "NODE" | "LEAF"
    bbox: tuple         # (xmin, ymin, zmin, xmax, ymax, zmax)
    block: int
    indices: tuple      # u16 brutos (sem os 12 bytes de cabecalho)
    flags: bytes
    start: int
    batches: list = field(default_factory=list)     # TriBatch (preenchido por read_tre)
    parsed: bool = True                              # False: o fluxo de indices nao fechou; so os lotes validos foram guardados
    head: tuple = ()                                 # v19: os 6 u16 dos 12 bytes iniciais do IDXS = cabecalho do 1o grupo/pagina/sublote
    full_grammar: bool = False                       # v19: True se o fluxo inteiro (cabecalho + indices) fechou pela gramatica, sem lote implicito


@dataclass
class TreFile:
    head: tuple = ()
    lights: list = field(default_factory=list)
    level_info: tuple = ()
    materials: list = field(default_factory=list)
    textures: list = field(default_factory=list)
    blocks: list = field(default_factory=list)
    nodes: list = field(default_factory=list)
    total_bytes: int = 0

    def triangles_ex(self, eps: float = 0.6):
        """Lista de (bloco, (i0, i1, i2), material, pagina, textura) com os indices corrigidos. Devolve (lista, estatisticas)."""
        stats = {"indices": 0, "outside_bbox": 0, "masked": 0, "bad": 0, "unparsed_nodes": 0, "dropped_flagged": 0}
        tris = []
        for nd in self.nodes:
            if not nd.indices:
                continue
            if not nd.parsed:
                stats["unparsed_nodes"] += 1
            if nd.block >= len(self.blocks):
                stats["bad"] += len(nd.indices)
                continue
            blk = self.blocks[nd.block]
            xyz, n = blk.xyz, blk.count
            bb = nd.bbox
            for ba in nd.batches:
                u = ba.indices
                for i in range(0, len(u) - 2, 3):
                    t = []
                    for x in u[i:i + 3]:
                        pick = None
                        for c in (x, x & 0x3FFF, x & 0x7FFF, x & 0xBFFF):
                            if c < n:
                                px, py, pz = xyz[3 * c:3 * c + 3]
                                if bb[0] - eps <= px <= bb[3] + eps and bb[1] - eps <= py <= bb[4] + eps and bb[2] - eps <= pz <= bb[5] + eps:
                                    pick = c
                                    break
                        stats["indices"] += 1
                        if pick is None:
                            stats["outside_bbox"] += 1
                            pick = x if x < n else (x & 0x3FFF)
                            if pick >= n:
                                stats["bad"] += 1
                                pick = 0
                        elif pick != x:
                            stats["masked"] += 1
                        t.append(pick)
                    tris.append((nd.block, tuple(t), ba.material, ba.page, ba.texture))
        return tris, stats

    def triangles(self, eps: float = 0.6):
        """Compatibilidade: lista de (bloco, (i0, i1, i2)) e estatisticas."""
        tris, stats = self.triangles_ex(eps)
        return [(b, t) for b, t, _m, _p, _k in tris], stats

    def batch_stats(self) -> dict:
        """Resumo dos lotes: triangulos por material / pagina de lightmap / textura difusa, nos com indices e nos que nao fecharam."""
        mats: dict = {}
        pages: dict = {}
        texs: dict = {}
        nodes = unparsed = full = 0
        for nd in self.nodes:
            if not nd.indices:
                continue
            nodes += 1
            full += 1 if nd.full_grammar else 0
            if not nd.parsed:
                unparsed += 1
            for ba in nd.batches:
                nt = len(ba.indices) // 3
                mats[ba.material] = mats.get(ba.material, 0) + nt
                pages[ba.page] = pages.get(ba.page, 0) + nt
                texs[ba.texture] = texs.get(ba.texture, 0) + nt
        return {"materials": mats, "pages": pages, "textures": texs, "nodes": nodes, "unparsed": unparsed, "full_grammar": full}

    def bounds(self):
        xs = [x for b in self.blocks for x in b.xyz[0::3]]
        ys = [x for b in self.blocks for x in b.xyz[1::3]]
        zs = [x for b in self.blocks for x in b.xyz[2::3]]
        if not xs:
            return None
        return (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))


def _cstr(b: bytes) -> str:
    return b.split(b"\0", 1)[0].decode("latin-1")


def _read_node(d: bytes, p: int) -> tuple:
    tag = d[p:p + 4].decode("latin-1")
    declared = struct.unpack_from("<I", d, p + 4)[0]
    q = p + 8
    if d[q:q + 4] != b"INFO":
        raise TreError(f"{tag} em {p} sem INFO")
    n = struct.unpack_from("<I", d, q + 4)[0]
    info = d[q + 8:q + 8 + n]
    q += 8 + n
    idx, flg, head = (), b"", ()
    if d[q:q + 4] == b"IDXD":
        n = struct.unpack_from("<I", d, q + 4)[0]
        end = q + 8 + n
        r = q + 8
        if d[r:r + 4] != b"IDXS":
            raise TreError(f"IDXD em {q} sem IDXS")
        m = struct.unpack_from("<I", d, r + 4)[0]
        body = d[r + 8:r + 8 + m]
        if len(body) < 12 or r + 8 + m > end:
            raise TreError(f"IDXS em {r} invalido")
        idx = struct.unpack_from("<%dH" % ((len(body) - 12) // 2), body, 12)
        head = struct.unpack_from("<6H", body, 0)
        r += 8 + m
        if r < end and d[r:r + 4] == b"FLGS":
            m = struct.unpack_from("<I", d, r + 4)[0]
            flg = d[r + 8:r + 8 + m]
        q = end
    if not flg and d[q:q + 4] == b"FLGS":
        m = struct.unpack_from("<I", d, q + 4)[0]
        flg = d[q + 8:q + 8 + m]
    end = p + 8 + declared + (4 if tag == "LEAF" else 0)
    if len(info) < 32:
        raise TreError(f"INFO de {tag} em {p} pequeno ({len(info)})")
    bbox = struct.unpack_from("<6f", info, 0)
    block = struct.unpack_from("<I", info, 24)[0]
    return TreNode(tag, bbox, block, idx, flg, p, head=head), end


# ---------------------------------------------------------------------------------------------- fluxo de indices em lotes
# IDXS = [lote inicial sem cabecalho] [grupo]*, alinhado a 4 bytes (1 u16 de enchimento = 0 quando a posicao fica impar):
#   grupo   = [0x8000|material][A] pagina+   |   pagina+          (A = total de u16 das paginas seguintes)
#   pagina  = [0x4000|pagina][B] sublote+     (B = total de u16 dos sublotes, com enchimento)
#   sublote = [k][C] indices[C]  (+1 de enchimento se C impar)  k = textura difusa (1-based), C multiplo de 3
# Os 2 bits altos dos cabecalhos colidem com indices de vertice de blocos grandes (> 16383); por isso cada cabecalho e validado por
# intervalo (material < N materiais, pagina <= N .ctx, k <= N .tga), pelo fechamento exato das contagens e pelo total = 3 x len(FLGS).


class _Lim:
    """Limites de validacao dos cabecalhos. strict=True exige que os u16 de enchimento (alinhamento a 4 bytes) sejam 0; split_batches tenta primeiro o modo
    estrito e so cai para o frouxo se nada fechar (o frouxo era o comportamento do v16 inicial, validado nos 38 arquivos reais)."""

    def __init__(self, mats, ctx, tga):
        self.mats, self.ctx, self.tga = max(mats, 1), ctx, tga
        self.strict = False


def _sub_batches(u, q, total, lim):
    end, out = q + total, []
    while q < end:
        if q + 2 > end:
            return None
        k, c = u[q], u[q + 1]
        if (k & 0xC000) or (c & 0xC000) or c % 3 or k > lim.tga or q + 2 + c > end:
            return None
        if lim.strict and (c & 1) and (q + 2 + c >= end or u[q + 2 + c] != 0):
            return None
        out.append((k, tuple(u[q + 2:q + 2 + c])))
        q += 2 + c + (c & 1)
    return out if q == end else None


def _page_group(u, p, lim):
    if p + 2 > len(u):
        return None
    a, b = u[p], u[p + 1]
    if (a & 0xC000) != 0x4000 or (b & 0xC000) or (a & 0x3FFF) > lim.ctx or p + 2 + b > len(u):
        return None
    subs = _sub_batches(u, p + 2, b, lim)
    return None if subs is None else (a & 0x3FFF, b, subs)


def _mat_hdr(u, p, lim):
    if p + 2 > len(u):
        return None
    a, b = u[p], u[p + 1]
    if (a & 0xC000) != 0x8000 or (b & 0xC000) or (a & 0x3FFF) >= lim.mats or _page_group(u, p + 2, lim) is None:
        return None
    return a & 0x3FFF


def _parse_rest(u, p, lim, partial=None):
    """Le grupos/paginas/sublotes de p ate o fim. Devolve a lista de TriBatch ou None se nao fechar."""
    n, out, mat, page = len(u), [], 0, 0
    while n - p >= 3:
        m = _mat_hdr(u, p, lim)
        if m is not None:
            mat, p = m, p + 2
        g = _page_group(u, p, lim)
        if g is not None:
            page, b, subs = g
            out.extend(TriBatch(mat, page, k, idx) for k, idx in subs)
            p += 2 + b
        else:                                   # sublote sem cabecalho de pagina (pagina e material herdados)
            if p + 2 > n:
                break
            k, c = u[p], u[p + 1]
            if (k & 0xC000) or (c & 0xC000) or c % 3 or k > lim.tga or p + 2 + c > n:
                if partial is not None:
                    partial.extend(out)
                return None
            if lim.strict and (c & 1) and (p + 2 + c >= n or u[p + 2 + c] != 0):
                if partial is not None:
                    partial.extend(out)
                return None
            out.append(TriBatch(mat, page, k, tuple(u[p + 2:p + 2 + c])))
            p += 2 + c + (c & 1)
        if p & 1:
            if lim.strict and (p >= n or u[p] != 0):
                if partial is not None:
                    partial.extend(out)
                return None
            p += 1
    if partial is not None:
        partial.extend(out)
    return out if n - p <= 1 else None


def split_full(head, u, tri_count, lim):
    """v19: os 12 bytes iniciais do IDXS (que o v15-v18 tratava como "3 x u32 de lixo") sao 6 u16 = o cabecalho do PRIMEIRO lote:
    [0x8000|material][A] [0x4000|pagina][B] [k][C]. Medido em Mainmenu e 03_Tutorial_Junktown: C == tamanho do "lote inicial" do v16 em 100% dos nos.
    Aqui o fluxo inteiro (cabecalho + indices) e lido pela gramatica de grupos desde a posicao 0, sem lote implicito.
    Devolve a lista de TriBatch, ou None se nao fechar (o chamador cai no split_batches antigo)."""
    if len(head) != 6 or (head[0] & 0xC000) != 0x8000:
        return None
    full = tuple(head) + tuple(u)
    try:
        for strict in (True, False):
            lim.strict = strict
            out = _parse_rest(full, 0, lim)
            if out is None:
                continue
            if tri_count and sum(len(b.indices) for b in out) != 3 * tri_count:
                continue
            return out
    finally:
        lim.strict = False
    return None


def split_batches(u, tri_count, lim):
    """Divide o fluxo de indices de um no em lotes. Devolve (lista de TriBatch, ok). tri_count = len(FLGS) (0 se nao ha)."""
    n = len(u)
    total_idx = 3 * tri_count
    try:
        for strict in (True, False):                       # 1o: enchimento exigido = 0 (evita fechamentos falsos); 2o: frouxo (v16 inicial)
            lim.strict = strict
            for q in range(0, n + 1, 2):
                # lote inicial: L0 indices (multiplo de 3) + 1 de enchimento se L0 impar
                l0 = q if q % 3 == 0 else (q - 1 if (q - 1) % 3 == 0 and (q - 1) % 2 == 1 else None)
                if l0 is None:
                    continue
                if strict and l0 != q and u[q - 1] != 0:
                    continue
                if q < n and not ((u[q] & 0xC000) in (0x4000, 0x8000) or (q + 1 < n and (u[q] & 0xC000) == 0 and u[q] <= lim.tga and u[q + 1] % 3 == 0 and u[q + 1] > 0)):
                    continue
                rest = _parse_rest(u, q, lim)
                if rest is None:
                    continue
                got = sum(len(b.indices) for b in rest)
                if tri_count and l0 + got != total_idx:
                    continue
                head = [TriBatch(0, 0, 0, tuple(u[:l0]))] if l0 else []
                return head + rest, True
    finally:
        lim.strict = False
    # nao fechou: guarda o que da para aproveitar (o lote inicial ate o primeiro cabecalho plausivel)
    first = next((q for q in range(0, n, 2) if (u[q] & 0xC000) in (0x4000, 0x8000) and (_mat_hdr(u, q, lim) is not None or _page_group(u, q, lim) is not None)), n)
    first -= first % 3
    partial: list = []
    _parse_rest(u, first + (first & 1) + (0 if first % 2 == 0 else 0), lim, partial)
    return ([TriBatch(0, 0, 0, tuple(u[:first]))] if first else []) + partial, False


def read_tre(data: bytes) -> TreFile:
    if len(data) < 16 or data[:4] != b"8TRE":
        raise TreError("nao e um 8TRE")
    n = struct.unpack_from("<I", data, 4)[0]
    if 8 + n > len(data):
        raise TreError("tamanho da raiz ultrapassa o arquivo")
    t = TreFile(total_bytes=len(data))
    top = {tag: (p, sz) for tag, p, sz in _chunks(data, 8, 8 + n)}
    for need in ("HEAD", "VTXD", "TREE"):
        if need not in top:
            raise TreError(f"falta o chunk {need}")
    p, sz = top["HEAD"]
    t.head = struct.unpack_from("<4I", data, p) if sz >= 16 else ()
    if "LGTD" in top:
        p, sz = top["LGTD"]
        for tag, q, m in _chunks(data, p, p + sz):
            if tag == "LGTR" and m >= 64:
                f = struct.unpack_from("<2I3f3f3f2f3f", data, q)
                t.lights.append({"type": f[0], "flags": f[1], "color": list(f[2:5]), "pos": list(f[5:8]), "dir": list(f[8:11]), "params": list(f[11:13]), "color2": list(f[13:16])})
    if "LVLD" in top:
        p, sz = top["LVLD"]
        for tag, q, m in _chunks(data, p, p + sz):
            if tag == "INFO" and m >= 16:
                t.level_info = struct.unpack_from("<2I2f", data, q)
                break
    if "MATD" in top:
        p, sz = top["MATD"]
        for tag, q, m in _chunks(data, p, p + sz):
            if tag == "MATR" and m >= 196:
                b = data[q:q + m]
                t.materials.append({"id": struct.unpack_from("<I", b, 0)[0], "name": _cstr(b[4:84]), "color": list(b[84:88]), "blend": _cstr(b[88:104]),
                                    "surface": _cstr(b[104:120]), "flags": struct.unpack_from("<I", b, m - 4)[0]})
    if "TXTD" in top:
        p, sz = top["TXTD"]
        for tag, q, m in _chunks(data, p + 8, p + sz):
            if tag == "TXTR":
                t.textures.append(_cstr(data[q:q + m]))
    p, sz = top["VTXD"]
    nb = struct.unpack_from("<I", data, p)[0]
    for tag, q, m in _chunks(data, p + 4, p + sz):
        if tag != "VTXB":
            continue
        cnt = struct.unpack_from("<I", data, q)[0]
        pos, streams = q + 4, {}
        while pos + 8 <= q + m:
            tok = data[pos:pos + 4].decode("latin-1")
            ln = struct.unpack_from("<I", data, pos + 4)[0]
            if pos + 8 + ln > q + m:
                raise TreError(f"fluxo {tok} ultrapassa o VTXB")
            streams[tok] = (pos + 8, ln)
            pos += 8 + ln
        if "XYZ " not in streams or streams["XYZ "][1] != 12 * cnt:
            raise TreError("VTXB sem XYZ coerente")

        def f32(tok, per):
            if tok not in streams or streams[tok][1] != 4 * per * cnt:
                return ()
            return struct.unpack_from("<%df" % (per * cnt), data, streams[tok][0])
        col = data[streams["DIFU"][0]:streams["DIFU"][0] + 4 * cnt] if "DIFU" in streams and streams["DIFU"][1] == 4 * cnt else b""
        t.blocks.append(VtxBlock(cnt, f32("XYZ ", 3), f32("NRML", 3), col, f32("TXUV", 2), f32("LMUV", 2)))
    if len(t.blocks) != nb:
        raise TreError(f"VTXD declara {nb} blocos, achei {len(t.blocks)}")
    p, sz = top["TREE"]
    q = p
    while q < len(data):
        if data[q:q + 4] not in (b"NODE", b"LEAF"):
            raise TreError(f"ficha inesperada em {q}: {data[q:q + 6]!r}")
        nd, q = _read_node(data, q)
        t.nodes.append(nd)
    if q != len(data):
        raise TreError(f"arvore termina em {q}, arquivo tem {len(data)}")
    lim = _Lim(len(t.materials), sum(1 for x in t.textures if x.lower().endswith(".ctx")), sum(1 for x in t.textures if x.lower().endswith(".tga")))
    for nd in t.nodes:
        if nd.indices:
            full = split_full(nd.head, nd.indices, len(nd.flags), lim)
            if full is not None:
                nd.batches, nd.parsed, nd.full_grammar = full, True, True
            else:
                nd.batches, nd.parsed = split_batches(nd.indices, len(nd.flags), lim)
    return t
