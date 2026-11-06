"""Relatorio de texto, JSON e imagem de visao superior (PNG) de um mapa decodificado (mapfile + rle)."""
from __future__ import annotations

import json
import math

from .imagecodec import encode_png
from .mapexport import CELL_SIZE_DEFAULT, build_f3map, grid_origin
from .mapfile import MapFile, estimate_cell_scale
from .rle import EMPTY, RleGrid, rle_to_rgba

KIND_COLOR = {"CRT": (255, 70, 70), "WEA": (255, 160, 40), "AMO": (255, 200, 80), "ARM": (230, 140, 60), "ITM": (255, 120, 200),
              "DOR": (80, 140, 255), "CON": (240, 230, 70), "USE": (200, 255, 80)}


def _fmt(v):
    return "(" + ", ".join(f"{x:.2f}" for x in v) + ")"


def map_report(name: str, m: MapFile, grid: RleGrid | None, scale_info, tre=None) -> str:
    L = [f"=== MAPA {name} ==="]
    h = m.header
    L.append(f"arquivo: {m.total_bytes} bytes, {len(m.chunks)} chunks, bytes nao explicados pelos decodificadores: {m.unexplained_bytes}")
    L.append(f"cabecalho: tre='{h.get('tre')}' rle='{h.get('rle')}' mapa-ambiente='{h.get('ambient_map')}'")
    L.append(f"           color_a={h.get('color_a')} color_b={h.get('color_b')} flag={h.get('flag')} floats={h.get('floats')} (cores/floats: hipoteses de nevoa/fundo)")
    if m.camera:
        L.append(f"camera: {m.camera.get('floats')}")
    if grid is not None:
        bb = grid.bbox()
        L.append(f"grade (.rle): {grid.width} colunas x {grid.rows} linhas, celulas nao vazias: {sum(1 for v in grid.cells if v != EMPTY)} de {len(grid.cells)}, caixa {bb}")
        L.append(f"              tiles: {grid.histogram()}")
        s, ok, tot = scale_info
        if tre is not None:
            L.append(f"mundo->celula (regra medida): celula = (posicao - origem) / 0,5, origem = canto minimo da geometria do .8tre {tuple(round(v, 2) for v in grid_origin(tre))}: {ok} de {tot} pontos em celulas nao vazias")
        else:
            L.append(f"mundo->celula sem o .8tre: melhor fator entre 0,5/1/2/4 = x{s:g} ({ok} de {tot} pontos); com o .8tre a regra e unica (veja docs/specs/tre.md)")
    else:
        L.append("grade (.rle): nao encontrada em RETAIL")
    L.append(f"\n[ENTIDADES] {len(m.entities)}")
    for e in m.entities:
        ev = e.get("eeov") or {}
        inv = ev.get("inventory") or []
        L.append(f"  {e['kind']:3s} {e['prototype']:44s} pos {_fmt(e['pos'])} yaw {e['yaw']:.2f}  inst='{ev.get('instance')}'"
                 + (f" script='{ev.get('script')}'" if ev.get("script") else "") + (f" skin='{ev.get('skin')}'" if ev.get("skin") else "")
                 + (f" nome='{ev.get('display_name')}'" if ev.get("display_name") else "") + (f" inventario={len(inv)}" if inv else ""))
    L.append(f"\n[EFEITOS] {len(m.effects)}")
    for e in m.effects:
        L.append(f"  {e['instance']:40s} pos {_fmt(e['pos'])} efeito '{e['effect']}'")
    L.append(f"\n[PONTOS DE ENTRADA] {len(m.entry_points)}")
    for e in m.entry_points:
        L.append(f"  id {e['id']}: " + " ".join(_fmt(p) for p in e.get("positions", []) if p[0] or p[2]) + f"  angulos {[round(a, 2) for a in e.get('angles', [])]}")
    L.append(f"\n[GRAFO DE NAVEGACAO] {len(m.nav_nodes)} nos")
    for i, n in enumerate(m.nav_nodes):
        L.append(f"  no {i:2d}: pos {_fmt(n['pos'])} tipo {n['kind']} vizinhos {n['links']}")
    L.append(f"\n[GATILHOS] {len(m.triggers)}")
    for t in m.triggers:
        r = t["region"]
        extra = ""
        if "script" in t: extra = f" -> script '{t['script'].get('script')}'"
        if "travel" in t: extra = f" -> viagem para '{t['travel'].get('target_map')}' ponto de entrada {t['travel'].get('entry_point')}"
        if "binding" in t: extra = f" -> vinculo id {t['binding'].get('id')}"
        L.append(f"  regiao tipo {r.get('kind')} com {len(r.get('points', []))} vertices{extra}")
    L.append(f"\n[CAMINHOS] {len(m.paths)}")
    for p in m.paths:
        L.append(f"  {p['name']}: {len(p['points'])} pontos")
    L.append(f"\n[SONS] {len(m.sounds)}")
    for s in m.sounds:
        L.append(f"  {s['name']:44s} pos {_fmt(s['pos'])} arquivo '{s['sound']}'")
    L.append(f"\n[MARCADORES] {len(m.markers)}")
    for k in m.markers:
        L.append(f"  pos {_fmt(k['pos'])} icone '{k['icon']}' texto-id {k['text_id']}")
    if tre is not None:
        tris, st = tre.triangles()
        b = tre.bounds()
        L.append(f"\n[GEOMETRIA .8tre] {len(tre.blocks)} bloco(s) de vertices ({sum(x.count for x in tre.blocks)} vertices), {len(tre.nodes)} nos/folhas, {len(tris)} triangulos; "
                 f"caixa x[{b[0]:.1f},{b[3]:.1f}] y[{b[1]:.1f},{b[4]:.1f}] z[{b[2]:.1f},{b[5]:.1f}]; {len(tre.materials)} materiais {[x['name'] for x in tre.materials][:6]}; "
                 f"texturas {tre.textures[:6]}; {len(tre.lights)} luzes; indices fora da caixa da folha: {st['outside_bbox']} de {st['indices']}")
        bs = tre.batch_stats()
        L.append(f"  lotes: {len(bs['materials'])} materiais usados, {len([x for x in bs['pages'] if x])} paginas de lightmap, {len([x for x in bs['textures'] if x])} texturas difusas; "
                 f"nos com indices {bs['nodes']}, que nao fecharam {bs['unparsed']}")
        for mid, nt in sorted(bs["materials"].items()):
            mt = tre.materials[mid] if mid < len(tre.materials) else None
            L.append(f"    material #{mid} {mt['name'] if mt else '(fora da tabela)'} ({mt['blend'] + ', ' + mt['surface'] if mt else '?'}): {nt} triangulos")
    L.append(f"\n[AGUA] {len(m.water)} bloco(s), {sum(len(w['patches']) for w in m.water)} remendos")
    n_gap = sum(len(e.get('eeov', {}).get('gaps', [])) for e in m.entities)
    L.append(f"\nnumeros ainda nao decifrados nas sobrescritas de entidade (EEOV): {n_gap} lacunas (veja docs/specs/map.md)")
    return "\n".join(L) + "\n"


def map_to_json(name: str, m: MapFile, grid: RleGrid | None, scale_info) -> str:
    d = m.to_dict()
    d["name"] = name
    d["grid"] = None if grid is None else {"width": grid.width, "rows": grid.rows, "histogram": grid.histogram(), "bbox": grid.bbox()}
    d["cell_scale"] = {"factor": scale_info[0], "hits": scale_info[1], "points": scale_info[2]}
    return json.dumps(d, indent=1, ensure_ascii=False)


class _Canvas:
    def __init__(self, w, h, rgba, k):
        self.k, self.w, self.h = k, w * k, h * k
        self.px = bytearray(self.w * self.h * 4)
        for y in range(h):
            row = rgba[y * w * 4:(y + 1) * w * 4]
            big = bytearray()
            for x in range(w):
                big += row[x * 4:x * 4 + 4] * k
            for dy in range(k):
                o = (y * k + dy) * self.w * 4
                self.px[o:o + self.w * 4] = big

    def dot(self, x, y, c, r=2):
        for yy in range(int(y) - r, int(y) + r + 1):
            for xx in range(int(x) - r, int(x) + r + 1):
                if 0 <= xx < self.w and 0 <= yy < self.h:
                    o = (yy * self.w + xx) * 4
                    self.px[o:o + 4] = bytes((c[0], c[1], c[2], 255))

    def line(self, x0, y0, x1, y1, c):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            t = i / max(n, 1)
            self.dot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, c, 0)


def map_overlay_png(m: MapFile, grid: RleGrid, scale: float, origin=(0.0, 0.0)) -> bytes:
    """Visao superior: grade de tiles + entidades (cor por tipo), nos e arestas de navegacao, entradas, gatilhos, caminhos, sons, marcadores, agua."""
    k = max(1, min(6, 900 // max(grid.width, 1)))
    cv = _Canvas(grid.width, grid.rows, rle_to_rgba(grid), k)
    P = lambda pos: ((pos[0] - origin[0]) * scale * k, (pos[2] - origin[1]) * scale * k)
    for t in m.triggers:
        pts = t["region"].get("points", [])
        col = (0, 220, 255) if "travel" in t else ((255, 0, 255) if "script" in t else (0, 150, 200))
        for i in range(len(pts)):
            a, b = P(pts[i]), P(pts[(i + 1) % len(pts)])
            cv.line(a[0], a[1], b[0], b[1], col)
    for p in m.paths:
        pts = [q["pos"] for q in p["points"]]
        for i in range(len(pts) - 1):
            a, b = P(pts[i]), P(pts[i + 1])
            cv.line(a[0], a[1], b[0], b[1], (120, 170, 255))
    for i, n in enumerate(m.nav_nodes):
        for j in n["links"]:
            if 0 <= j < len(m.nav_nodes):
                a, b = P(n["pos"]), P(m.nav_nodes[j]["pos"])
                cv.line(a[0], a[1], b[0], b[1], (60, 200, 90))
    for n in m.nav_nodes:
        x, y = P(n["pos"]); cv.dot(x, y, (90, 255, 120), 1)
    for w in m.water:
        for q in w["patches"]:
            x, y = P(q["pos"]); cv.dot(x, y, (50, 90, 255), 2)
    for s in m.sounds:
        x, y = P(s["pos"]); cv.dot(x, y, (190, 90, 255), 1)
    for mk in m.markers:
        x, y = P(mk["pos"]); cv.dot(x, y, (255, 255, 255), 2)
    for e in m.entities:
        x, y = P(e["pos"]); cv.dot(x, y, KIND_COLOR.get(e["kind"], (255, 255, 255)), 2)
    for ef in m.effects:
        x, y = P(ef["pos"]); cv.dot(x, y, (255, 120, 0), 1)
    for ep in m.entry_points:
        for q in ep.get("positions", []):
            if q[0] or q[2]:
                x, y = P(q); cv.dot(x, y, (255, 255, 0), 3)
    return encode_png(cv.w, cv.h, bytes(cv.px))


def analyze_map(name: str, map_bytes: bytes, rle_bytes: bytes | None, tre_bytes: bytes | None = None):
    """Devolve (MapFile, RleGrid|None, scale_info, texto, json, png|None, f3map|None). Com o .8tre vale a regra unica
    celula = (posicao - origem) / 0,5; sem ele, estima o fator entre 0,5/1/2/4."""
    from .mapfile import read_map
    from .rle import read_rle
    from .tre import read_tre
    m = read_map(map_bytes)
    grid = read_rle(rle_bytes) if rle_bytes is not None else None
    tre = read_tre(tre_bytes) if tre_bytes is not None else None
    if tre is not None and grid is not None:
        org = grid_origin(tre)
        pts = [e["pos"] for e in m.entities] + [n["pos"] for n in m.nav_nodes] + [q for ep in m.entry_points for q in ep.get("positions", []) if q[0] or q[2]]
        ok = sum(1 for p in pts if 0 <= int((p[0] - org[0]) / CELL_SIZE_DEFAULT) < grid.width and 0 <= int((p[2] - org[1]) / CELL_SIZE_DEFAULT) < grid.rows
                 and grid.at(int((p[0] - org[0]) / CELL_SIZE_DEFAULT), int((p[2] - org[1]) / CELL_SIZE_DEFAULT)) != 0x2F)
        info = (1.0 / CELL_SIZE_DEFAULT, ok, len(pts))
        png = map_overlay_png(m, grid, info[0], org)
    else:
        info = estimate_cell_scale(m, grid)
        png = map_overlay_png(m, grid, info[0]) if grid is not None else None
    f3 = build_f3map(name, m, grid, tre) if (tre is not None or grid is not None) else None
    return m, grid, info, map_report(name, m, grid, info, tre), map_to_json(name, m, grid, info), png, f3
