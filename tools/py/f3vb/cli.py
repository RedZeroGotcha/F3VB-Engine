"""CLI: python -m f3vb {inventory|probe|report} ..."""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from collections import Counter

from .amxdis import analyze_amx, disassemble
from .analyze import aggregate_grp, format_resumo
from .b3dfail import b3d_failures
from .export import DEFAULT_MESH_SCALE, Exporter, export_b3d_to_glb
from .extractor import ExtractOptions, convert_retail, extract_all, fmt_bytes as _fb, plan as _plan
from .layout import AssetsLayout, default_assets_dir
from .f3m import write_f3m
from .gr2export import read_clips, read_skeleton
from .index import ResourceIndex
from .stf import StfError, parse_stf
from .catalog import build_catalog, catalog_summary, catalog_tsv
from .classify import classify
from .extract import base_class, extract, sample
from .gr2tool import summarize_gr2
from .probes import probe_amx, probe_b3d, probe_bmp, probe_tga
from .grp import GrpError, GrpFile
from .inventory import inventory
from .knowledge import RESOURCE_EXT
from .probe import probe_file
from .rht import RhtError, read_rht


def _game_dir(arg: str | None) -> str:
    d = arg or os.environ.get("F3VB_GAME_DIR")
    if not d:
        sys.exit("Informe a pasta da demo (argumento ou variável F3VB_GAME_DIR).")
    return d


def _fmt_bytes(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.1f} {unit}" if unit != "B" else f"{n} B"
        n /= 1024
    return f"{n} B"


def cmd_inventory(a) -> None:
    inv = inventory(_game_dir(a.game_dir), with_hash=a.hash)
    print(f"Pasta: {inv['root']}")
    print(f"Arquivos: {inv['total_files']}  Total: {_fmt_bytes(inv['total_bytes'])}")
    print(f"Pasta Override presente: {inv['has_override_dir']}")
    print("\nPor extensão (maiores primeiro):")
    for ext, v in inv["by_ext"].items():
        print(f"  .{ext:<14} {v['files']:>6} arquivos  {_fmt_bytes(v['bytes']):>10}")
    if inv["duplicates"]:
        print(f"\nDUPLICATAS (mesmo nome e tamanho): {len(inv['duplicates'])} grupo(s), {_fmt_bytes(inv['duplicate_bytes'])} repetidos")
        for grupo in inv["duplicates"]:
            print("  -", " = ".join(grupo))
    print("\nArquivos .grp:")
    for e in inv["grp_files"]:
        print(f"  {e['path']}  ({_fmt_bytes(e['size'])})")
    print("\nArquivos notáveis:")
    for n, e in inv["notable"].items():
        print(f"  {n:<14} {'ok  ' + _fmt_bytes(e['size']) if e else 'AUSENTE'}")
    if a.json:
        Path(a.json).write_text(json.dumps(inv, indent=2), encoding="utf-8")
        print(f"\nJSON salvo em {a.json}")


def cmd_probe(a) -> None:
    r = probe_file(a.file, head_bytes=a.head)
    print(f"{r['path']}  ({_fmt_bytes(r['size'])})")
    print(f"Primeiros 16 bytes em ASCII: {r.get('head_ascii_16', '')!r}\n")
    print(r["head_hex"])
    print("\nMagics (contagem aproximada):")
    for k, v in r["magics"].items():
        print(f"  {k:<28} {v['count']:>8}  offsets: {v['first_offsets']}")
    print("\nChunks plausíveis (tags deduzidas dos patterns, NÃO verificadas):")
    for tag, v in r["chunks"].items():
        print(f"  {tag}  [{v['group']}]  {v['plausible']:>6}  offsets: {v['first_offsets']}")


def _grp_summary(path, with_classes: bool = True, aggregates: bool = False) -> dict:
    with GrpFile(path) as g:
        info = g.info
        classes: Counter = Counter()
        sizes = [lp.length for lp in info.lumps]
        if with_classes:
            for lp in info.lumps:
                classes[classify(g.head(lp, 64), lp.length)] += 1
        return {
            "path": str(path),
            "version": [info.v_major, info.v_minor],
            "lumps": info.n_files,
            "file_size": info.file_size,
            "payload_bytes": sum(sizes),
            "largest_lump": max(sizes, default=0),
            "classes": dict(classes.most_common()),
            "issues": info.issues[:20],
            "issues_total": len(info.issues),
            "aggregates": aggregate_grp(g) if aggregates else None,
        }


def cmd_grp(a) -> None:
    try:
        summ = _grp_summary(a.file)
    except GrpError as exc:
        sys.exit(f"Não parece um .grp neste layout: {exc}")
    print(f"{summ['path']}  ({_fmt_bytes(summ['file_size'])})")
    print(f"Versão {summ['version'][0]}.{summ['version'][1]}  lumps: {summ['lumps']}  "
          f"dados: {_fmt_bytes(summ['payload_bytes'])}  maior: {_fmt_bytes(summ['largest_lump'])}")
    print("Conteúdo por heurística de magic (palpites, '?' = fraco):")
    for k, v in summ["classes"].items():
        print(f"  {k:<22} {v:>7}")
    if summ["issues_total"]:
        print(f"\nPROBLEMAS ({summ['issues_total']}):")
        for i in summ["issues"]:
            print("  -", i)
    if a.list or a.extract:
        out = Path(a.extract) if a.extract else None
        if out:
            out.mkdir(parents=True, exist_ok=True)
        with GrpFile(a.file) as g:
            for lp in g.info.lumps:
                head = g.head(lp, 64)
                cls = classify(head, lp.length)
                if a.list:
                    print(f"  #{lp.index:<6} off={lp.offset:<10} len={lp.length:<10} {cls}")
                if out and lp.end <= g.info.file_size:
                    ext = cls.split(":")[0].split(" ")[0].rstrip("?") or "bin"
                    (out / f"lump_{lp.index:05d}.{ext if ext.isalnum() else 'bin'}").write_bytes(g.read(lp))
        if out:
            print(f"\nLumps extraídos em {out} (guarde em private/, nunca commite)")


def cmd_rht(a) -> None:
    try:
        info = read_rht(a.file)
    except RhtError as exc:
        sys.exit(f"Não parece um resource.rht neste layout: {exc}")
    s = info.summary()
    print(f"{a.file}  ({_fmt_bytes(s['file_size'])})  versão {s['version'][0]}.{s['version'][1]}")
    print(f"Entradas: {s['entries']}  offsetPacks={s['offset_packs']}  offsetResources={s['offset_resources']}  "
          f"(fim da tabela de entradas={s['entries_end']})")
    print("\nPor tipo:")
    for name, v in s["by_type"].items():
        ext = RESOURCE_EXT.get(v["type_id"], "?")
        print(f"  {v['type_id']:>5} {name:<18} .{ext:<5} {v['count']:>7}")
    if s["unknown_type_ids"]:
        print("\nTIPOS NÃO CATALOGADOS nos patterns:", s["unknown_type_ids"])
    print("\nPor pack:", s["by_pack"])
    if s["unknown_pack_values"]:
        print("Valores de pack fora do enum:", s["unknown_pack_values"])
    print(f"\nCampo 'one': {s['field_one_values']}")
    print(f"'number': {s['number_unique']} únicos, faixa {s['number_range']}")
    print(f"'offset': faixa {s['offset_range']}")
    print("\nStrings da região de packs (offset relativo, texto):")
    for off, txt in s["pack_region_strings"]:
        print(f"  {off:>5}  {txt!r}")
    if s["issues"]:
        print("\nPROBLEMAS:", *s["issues"][:10], sep="\n  - ")
    if a.entries:
        print("\nPrimeiras entradas:")
        for e in info.entries[:a.entries]:
            print(f"  #{e.index:<6} pack={info.pack_label(e.pack):<10} lump={e.number:<6} type={e.type_name:<18} "
                  f"name={info.resource_name(e.offset)!r}")


def _print_manifest(m: dict) -> None:
    print(f"{m['count']} arquivo(s) de {m['source_grp']}")
    for e in m["files"]:
        pr = e.get("probe") or {}
        extra = ""
        if e["class"].startswith("gr2") and "version" in pr:
            extra = f"  v{pr['version']} {pr['compressions']}"
        elif e["class"].startswith("tga") and "width" in pr:
            extra = f"  {pr['width']}x{pr['height']} {pr['bpp']}bpp {pr['type_name']}"
        elif e["class"].startswith("b3d"):
            extra = f"  tex={len(pr.get('textures', []))} mat={len(pr.get('materials', []))} bones={pr.get('bone_count', 0)}"
        print(f"  {e['file']:<22} lump #{e['lump']:<6} {e['length']:>10} B  {e['class']}{extra}")
    for c in m.get("not_found", []):
        print(f"  (nenhum lump '{c}' encontrado no intervalo de tamanho pedido)")


def cmd_extract(a) -> None:
    classes = a.classes.split(",") if a.classes else None
    indices = [int(x) for x in a.indices.split(",")] if a.indices else None
    m = extract(a.file, a.out, classes=classes, indices=indices, limit=a.limit)
    _print_manifest(m)
    print(f"\nSaída em {a.out} (guarde em private/, nunca commite)")


def cmd_sample(a) -> None:
    m = sample(a.file, a.out, classes=tuple(a.classes.split(",")), per_class=a.per_class,
               min_bytes=a.min_bytes, max_bytes=a.max_bytes, strategy=a.strategy, make_zip=not a.no_zip,
               text_only=a.so_texto)
    _print_manifest(m)
    if m.get("zip"):
        print(f"\nZIP para enviar à análise: {m['zip']}")


def cmd_gr2(a) -> None:
    data = Path(a.file).read_bytes()
    print(json.dumps(summarize_gr2(data, deep=not a.no_deep), indent=2, ensure_ascii=False))


def cmd_inspect(a) -> None:
    if a.grp is not None:
        with GrpFile(a.grp) as g:
            data = g.read(int(a.file))
    else:
        data = Path(a.file).read_bytes()
    cls = classify(data[:64], len(data))
    b = base_class(cls)
    print(f"classe: {cls}  tamanho: {len(data)}")
    if b == "gr2":
        res = summarize_gr2(data, deep=not a.no_deep)
    elif b == "b3d":
        res = probe_b3d(data)
    elif b == "tga":
        res = probe_tga(data)
    elif b == "bmp":
        res = probe_bmp(data)
    elif b == "amx":
        res = probe_amx(data)
    else:
        res = {"nota": "sem sonda específica para esta classe"}
    print(json.dumps(res, indent=2, ensure_ascii=False))


def cmd_catalog(a) -> None:
    game = _game_dir(a.game_dir)
    rht_path = a.rht
    if not rht_path:
        found = next(Path(game).rglob("resource.rht"), None)
        if not found:
            sys.exit("resource.rht não encontrado; use --rht CAMINHO")
        rht_path = str(found)
    info = read_rht(rht_path)
    cat = build_catalog(game, info)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "catalogo.txt").write_text(catalog_tsv(cat), encoding="utf-8")
    text = catalog_summary(cat, info)
    (out / "catalogo_resumo.txt").write_text(text, encoding="utf-8")
    print(text)
    print(f"Arquivos: {out / 'catalogo.txt'} e {out / 'catalogo_resumo.txt'}")


def cmd_amxdis(a) -> None:
    if a.grp is not None:
        with GrpFile(a.grp) as g:
            data = g.read(int(a.file))
    else:
        data = Path(a.file).read_bytes()
    if a.check:
        r = analyze_amx(data)
        r = {k: v for k, v in r.items() if not k.startswith("_")}
        print(json.dumps(r, indent=2, ensure_ascii=False))
    else:
        print(disassemble(data))


def cmd_b3dmat(a) -> None:
    """Materiais, texturas, UVs e ossos de um modelo B3D (le de original_assets/RETAIL; nao imprime nada do conteudo do jogo alem de nomes e numeros)."""
    from .b3d import read_b3d
    from .export import resolve_bone_aliases
    from .gr2export import read_skeleton
    from .imagecodec import decode_bmp, decode_tga
    from .index import RetailIndex
    ix = RetailIndex.from_csv(a.assets) if a.assets else RetailIndex.from_csv(default_assets_dir())
    raw = ix.read_named(a.name, "G3D_Model")
    if raw is None:
        sys.exit(f"modelo '{a.name}' nao encontrado em RETAIL (use 'py -3 -m f3vb export-list G3D_Model --prefix {a.name[:4]}' para ver nomes)")
    model = read_b3d(raw)
    lines = [f"MODELO {a.name}: {len(list(model.surfaces()))} superficie(s), {len(model.bones)} osso(s) citados"]
    sraw = ix.read_named(a.name, "SKEL_Skeleton")
    joints = read_skeleton(sraw)[0] if sraw else None
    bone_names = [b.name for b in model.bones]
    if joints:
        res, aliased = resolve_bone_aliases(bone_names, joints)
        jn = {j.name for j in joints}
        lines.append(f"esqueleto '{a.name}': {len(joints)} juntas; ossos do B3D sem junta (mesmo com apelidos): {[b for b in res if b not in jn]}")
        lines.append(f"ossos casados por sufixo: {aliased}")
    else:
        lines.append(f"esqueleto '{a.name}' nao encontrado em RETAIL")
    lines.append("ossos do B3D: " + ", ".join(bone_names))
    seen_tex = {}
    for ni, (node, surf) in enumerate(model.surfaces()):
        us = [p[0] for p in surf.uv0]; vs = [p[1] for p in surf.uv0]
        lines.append(f"--- superficie #{ni} no '{getattr(node, 'name', '')}': {len(surf.positions)} vertices, familia {surf.family}, "
                     f"uv0 u[{min(us, default=0):.3f},{max(us, default=0):.3f}] v[{min(vs, default=0):.3f},{max(vs, default=0):.3f}], "
                     f"uv1 {'sim' if surf.uv1 else 'nao'}, cor de vertice {'sim' if surf.colors else 'nao'}, lightmap {surf.lightmap}")
        for mi, m in enumerate(surf.materials):
            lines.append(f"    material #{mi} '{m.name}' blend={m.blend} tipo={m.type} floats={[round(x, 3) for x in m.floats[:13]]}")
            for t in m.textures:
                stem = t.rsplit('.', 1)[0]
                if stem not in seen_tex:
                    tr = ix.read_named(stem, "TGA_Image")
                    info = "NAO encontrada em RETAIL"
                    if tr is not None:
                        try:
                            w, h, _ = decode_bmp(tr) if tr[:2] == b"BM" else decode_tga(tr)
                            info = f"{w}x{h}"
                        except Exception as exc:
                            info = f"erro: {exc}"
                    seen_tex[stem] = info
                lines.append(f"        textura '{t}': {seen_tex[stem]}")
    text = "\n".join(lines)
    print(text)
    if a.out:
        Path(a.out).write_text(text + "\n", encoding="utf-8")
        print(f"\nRelatorio em {a.out}")


def cmd_pack_sample(a) -> None:
    from .index import RetailIndex
    from .sample import write_sample_zip
    root = Path(a.assets) if a.assets else default_assets_dir()
    ix = RetailIndex.from_csv(root)
    out = Path(a.out) if a.out else root.parent / "private" / ("amostra_" + "_".join(a.names[:3]) + ".zip")
    out.parent.mkdir(parents=True, exist_ok=True)
    n, total, warns = write_sample_zip(ix, a.names, out, a.anims)
    for w in warns:
        print("aviso:", w)
    print(f"{n} arquivos ({total // 1024} KB sem compressao) em {out}")
    print("Este zip tem conteudo do jogo: so para enviar a quem esta depurando, nunca publicar.")


def _find_resource(ix, name: str, type_name: str):
    """Procura por nome exato e depois por trecho (precisa ser unico)."""
    hit = ix.find(name, type_name)
    if hit:
        return hit
    cands = [(n, pk, num) for n, pk, num in ix.entries.get(type_name, []) if name.lower() in n.lower()]
    if len(cands) == 1:
        return cands[0][1], cands[0][2]
    if not cands:
        sys.exit(f"'{name}' nao encontrado em RETAIL (tipo {type_name}).")
    sys.exit(f"'{name}' e ambiguo em {type_name}: " + ", ".join(sorted(c[0] for c in cands)[:12]))


def cmd_map_info(a) -> None:
    from .index import RetailIndex
    from .mapreport import analyze_map
    if Path(a.name).is_file():                       # arquivo avulso .map (+ --rle)
        data = Path(a.name).read_bytes()
        rle = Path(a.rle).read_bytes() if a.rle else None
        label = Path(a.name).stem
    else:
        ix = RetailIndex.from_csv(a.assets) if a.assets else RetailIndex.from_csv(default_assets_dir())
        pk, num = _find_resource(ix, a.name, "MAP_Map")
        data = ix.read(pk, num)
        label = a.name
        from .mapfile import read_map
        hdr = read_map(data).header
        stem = hdr.get("rle", "").rsplit(".", 1)[0]
        rle = ix.read_named(stem, "RLE") if stem else None
        tstem = hdr.get("tre", "").rsplit(".", 1)[0]
        tre_raw = ix.read_named(tstem, "TRE_Octree") if tstem else None
    if Path(a.name).is_file():
        tre_raw = Path(a.tre).read_bytes() if a.tre else None
    m, grid, info, txt, js, png, f3 = analyze_map(label, data, rle, tre_raw)
    print(txt)
    if a.out:
        Path(a.out).write_text(txt, encoding="utf-8")
    if a.json:
        Path(a.json).write_text(js, encoding="utf-8")
    if a.png and png:
        Path(a.png).write_bytes(png)
        print(f"imagem: {a.png}")
    if a.f3map and f3:
        Path(a.f3map).write_bytes(f3)
        print(f"mapa para o F3VBMapViewer: {a.f3map} ({len(f3) // 1024} KB)")


def cmd_tre_info(a) -> None:
    from .index import RetailIndex
    from .tre import read_tre
    if Path(a.name).is_file():
        data = Path(a.name).read_bytes()
    else:
        ix = RetailIndex.from_csv(a.assets) if a.assets else RetailIndex.from_csv(default_assets_dir())
        pk, num = _find_resource(ix, a.name, "TRE_Octree")
        data = ix.read(pk, num)
    t = read_tre(data)
    tris, st = t.triangles()
    b = t.bounds()
    print(f"8TRE {len(data)} bytes; HEAD {t.head}; LVLD info {t.level_info}")
    print(f"blocos de vertices: {[x.count for x in t.blocks]}; nos {sum(1 for n in t.nodes if n.kind == 'NODE')}, folhas {sum(1 for n in t.nodes if n.kind == 'LEAF')}; triangulos {len(tris)}")
    print(f"caixa: x[{b[0]:.1f},{b[3]:.1f}] y[{b[1]:.1f},{b[4]:.1f}] z[{b[2]:.1f},{b[5]:.1f}]")
    print(f"materiais: {[(m['name'], m['blend'], m['surface']) for m in t.materials]}")
    print(f"texturas: {t.textures}; luzes: {len(t.lights)}")
    print(f"indices: {st['indices']}, fora da caixa da folha {st['outside_bbox']} ({100 * st['outside_bbox'] / max(1, st['indices']):.2f}%), corrigidos por mascara {st['masked']}, invalidos {st['bad']}")
    bs = t.batch_stats()
    print(f"lotes (v16): nos com indices {bs['nodes']}, que nao fecharam {bs['unparsed']}; materiais usados {len(bs['materials'])}, paginas de lightmap {len([x for x in bs['pages'] if x])}, texturas difusas {len([x for x in bs['textures'] if x])}")
    for mid, nt in sorted(bs["materials"].items()):
        mt = t.materials[mid] if mid < len(t.materials) else None
        print(f"  material #{mid} {mt['name'] if mt else '(fora da tabela)'} ({mt['blend'] + ', ' + mt['surface'] if mt else '?'}): {nt} triangulos")
    for k, nt in sorted(bs["textures"].items(), key=lambda kv: -kv[1])[:12]:
        names = [x for x in t.textures if x.lower().endswith('.tga')]
        print(f"  textura difusa {k}: {names[k - 1] if 1 <= k <= len(names) else '(nenhuma)'}: {nt} triangulos")


def cmd_tre_check(a) -> None:
    """Confere o parser de lotes em TODOS os .8tre de uma pasta (ou do RETAIL): nos que nao fecham, indices fora da caixa da folha, triangulos."""
    from .tre import read_tre
    if a.folder:
        files = sorted(Path(a.folder).rglob("*.8tre"))
        reader = lambda f: f.read_bytes()                                    # noqa: E731
    else:
        from .index import RetailIndex
        ix = RetailIndex.from_csv(a.assets) if a.assets else RetailIndex.from_csv(default_assets_dir())
        files = sorted(ix.entries.get("TRE_Octree", []))                     # (nome, pack, numero)
        reader = lambda r: ix.read(r[1], r[2])                               # noqa: E731
    tot = {"arquivos": 0, "nos": 0, "gramatica_completa": 0, "nao_fechados": 0, "fora_bbox": 0, "triangulos": 0, "sem_estado": 0, "erros": 0}
    for f in files:
        name = f.name if hasattr(f, "name") else f[0]
        try:
            t = read_tre(reader(f))
        except Exception as e:                                               # noqa: BLE001
            tot["erros"] += 1
            print(f"ERRO {name}: {e}")
            continue
        bs = t.batch_stats()
        tris, st = t.triangles_ex()
        tot["arquivos"] += 1
        tot["nos"] += bs["nodes"]
        tot["nao_fechados"] += bs["unparsed"]
        tot["fora_bbox"] += st["outside_bbox"]
        tot["triangulos"] += len(tris)
        tot["gramatica_completa"] += bs["full_grammar"]
        sem = sum(1 for tr in tris if tr[2] == 0 and tr[3] == 0 and tr[4] == 0)       # v19: deve ser 0 (era o "lote inicial" sem textura nem lightmap)
        tot["sem_estado"] += sem
        flag = "" if not bs["unparsed"] and not st["outside_bbox"] and bs["full_grammar"] == bs["nodes"] else "  <== atencao"
        print(f"{name:40s} nos {bs['nodes']:5d} gramatica {bs['full_grammar']:5d} nao-fechados {bs['unparsed']:3d} fora-da-caixa {st['outside_bbox']:6d} "
              f"triangulos {len(tris):8d} sem-estado {sem:6d}{flag}")
    print(f"TOTAL: {tot}")
    print("Esperado (v19, 38 arquivos): nos=5925 gramatica_completa=5925 nao_fechados=0 fora_bbox=0 triangulos=1928117 sem_estado=0")
    print("(sem_estado = triangulos com material 0, sem pagina e sem textura: no v16-v18 eram o 'lote inicial' e saiam brancos)")


def cmd_rle_info(a) -> None:
    from .index import RetailIndex
    from .rle import read_rle, rle_to_png
    if Path(a.name).is_file():
        data = Path(a.name).read_bytes()
    else:
        ix = RetailIndex.from_csv(a.assets) if a.assets else RetailIndex.from_csv(default_assets_dir())
        pk, num = _find_resource(ix, a.name, "RLE")
        data = ix.read(pk, num)
    g = read_rle(data)
    print(f"grade {g.width} colunas x {g.rows} linhas (cabecalho: {g.width}, {g.rows + 1}, {g.dim_c}; vazio = 0x{g.fill:02x})")
    print(f"celulas nao vazias: {sum(1 for v in g.cells if v != g.fill)} de {len(g.cells)}; caixa: {g.bbox()}")
    print(f"tiles: {g.histogram()}")
    if a.png:
        Path(a.png).write_bytes(rle_to_png(g))
        print(f"imagem: {a.png}")


def cmd_b3dfail(a) -> None:
    rht = read_rht(a.rht) if a.rht else None
    text = b3d_failures(a.file, rht=rht, per_reason=a.per_reason, max_bytes=a.max_bytes)
    if a.out:
        Path(a.out).write_text(text, encoding="utf-8")
        print(text.split("\n", 1)[0])
        print(f"Relatório em {a.out}")
    else:
        print(text)


def cmd_export_list(a) -> None:
    game = _game_dir(a.game_dir)
    with ResourceIndex(game) as ix:
        rows = ix.search(a.type, a.prefix or "")
        print(f"{len(rows)} recursos do tipo {a.type}" + (f" com prefixo '{a.prefix}'" if a.prefix else ""))
        for nm, pack, num in rows[:a.limit]:
            print(f"  {nm:<48} {pack}#{num}")
        if len(rows) > a.limit:
            print(f"  ... (use --limit para ver mais)")


def cmd_export_glb(a) -> None:
    game = _game_dir(a.game_dir)
    with ResourceIndex(game) as ix:
        data = ix.read_named(a.name, "G3D_Model")
        if data is None:
            close = [n for n, _, _ in ix.search("G3D_Model", a.name[:4])][:8]
            sys.exit(f"modelo '{a.name}' não encontrado. Parecidos: {close}. Use: export-list G3D_Model --prefix XX")
        joints, clips = None, []
        skel_name = a.skeleton if a.skeleton is not None else a.name
        if skel_name:
            sdata = ix.read_named(skel_name, "SKEL_Skeleton")
            if sdata is not None:
                joints, _ = read_skeleton(sdata)
                print(f"esqueleto '{skel_name}': {len(joints)} ossos")
            elif a.skeleton:
                sys.exit(f"esqueleto '{a.skeleton}' não encontrado")
        if joints and not a.no_anim:
            prefix = a.anim if a.anim is not None else a.name + "_"
            rows = ix.search("GR2_Animation", prefix)
            if a.max_anims and len(rows) > a.max_anims:
                print(f"{len(rows)} animações com prefixo '{prefix}': exportando as {a.max_anims} primeiras (use --max-anims 0 para todas)")
                rows = rows[:a.max_anims]
            names = {j.name for j in joints}
            for nm, pack, num in rows:
                try:
                    clips += read_clips(ix.read(pack, num), names, clip_name=nm)
                except Exception as exc:
                    print(f"  aviso: animação {nm}: {type(exc).__name__}: {exc}")
        def loader(stem):
            raw = ix.read_named(stem, "TGA_Image")
            return raw
        glb, rep = export_b3d_to_glb(data, a.name, texture_loader=loader, joints=joints, clips=clips,
                                     unit_scale=a.scale, convert=not a.no_convert, mesh_scale=a.mesh_scale)
    out = Path(a.out) if a.out else Path("private/glb") / f"{a.name}.glb"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(glb)
    st = rep["stats"]
    print(f"GLB salvo em {out} ({len(glb)} bytes): {st['surfaces']} superfícies, {st['vertices']} vértices, "
          f"{st['triangles']} triângulos, {st['textures']} texturas, {st['joints']} ossos, {st['animations']} animações")
    for w in rep["warnings"][:15]:
        print("  aviso:", w)
    from .glb import validate_glb
    probs = validate_glb(glb)
    print("validação estrutural:", "OK" if not probs else probs)


def cmd_stf(a) -> None:
    data = Path(a.file).read_bytes()
    try:
        stf = parse_stf(data)
    except StfError as exc:
        sys.exit(f"Não parece um .stf neste layout: {exc}\nPrimeiros 32 bytes: {data[:32].hex(' ')}")
    print(f"{a.file}: magic={stf.magic!r} entradas={stf.count} tamanho={len(data)}")
    for i in (a.id or []):
        try:
            print(f"  [{i}] {stf.text(data, i)!r}")
        except StfError as exc:
            print(f"  [{i}] erro: {exc}")
    if a.info or not a.id:
        for i in range(min(5, stf.count)):
            off, ln, const = stf.entries[i]
            print(f"  entrada {i}: offset={off} length={ln} constante={const.hex(' ')} texto={stf.text(data, i)[:60]!r}")


def cmd_extract_all(a) -> None:
    game = _game_dir(a.game_dir)
    if a.plan_only:
        p = _plan(game)
        print(f"{p['files']} arquivos, {_fb(p['bytes'])} no tamanho real")
        return
    opts = ExtractOptions(raw=not a.no_raw, png=a.png, glb=a.glb, f3m=a.f3m, maps=a.maps, anims_per_model=a.anims)
    last = [0.0]

    def prog(d, t, m):
        import time as _t
        if _t.time() - last[0] > 0.5 or d == t:
            last[0] = _t.time()
            print(f"\r{d}/{t}  {m[:60]:<60}", end="", flush=True)

    layout = AssetsLayout(a.out)
    print(f"Destino: {layout.root}  (RETAIL = originais, CONVERTED = convertidos)")
    try:
        log = extract_all(game, a.out, opts, progress=prog)
    except FileNotFoundError as exc:
        sys.exit(f"\nErro: {exc}. Confira se a pasta da demo é a que tem o resource.rht e a pasta data com os .grp.")
    print(f"\nConcluído em {log['seconds']} s: {log['written']} arquivos ({_fb(log['bytes'])}), PNG {log['png']}, "
          f"F3M {log['f3m_models']}+{log['f3m_skeletons']}, GLB {log['glb']}, falhas {len(log['failures'])}")
    print(f"Relatório: {layout.report}")
    # v19: as fontes e o English.stf ficam SOLTOS na pasta da demo (fora dos .grp); traz junto
    from .loose import import_loose
    try:
        r = import_loose(Path(game), layout.root)
        print(f"Arquivos soltos (v19): {r['fonts']} fonte(s), English.stf {'ok' if not any('English.stf' in m for m in r['missing']) else 'NAO ACHADO'}")
    except OSError as exc:
        print(f"Arquivos soltos (v19): nao copiados ({exc}); rode  py -3 -m f3vb import-loose <pasta da demo>")


def cmd_convert_retail(a) -> None:
    layout = AssetsLayout(a.out)
    if not (a.png or a.glb or a.f3m or a.maps):
        a.png = a.f3m = a.maps = True
    if layout.converted_is_stale():
        print("Aviso: CONVERTED veio de um conversor antigo (v11 ou sem registro); será refeita agora.")
    opts = ExtractOptions(png=a.png, glb=a.glb, f3m=a.f3m, maps=a.maps, anims_per_model=a.anims)
    last = [0.0]

    def prog(d, t, m):
        import time as _t
        if _t.time() - last[0] > 0.5 or d == t:
            last[0] = _t.time()
            print(f"\r{d}/{t}  {m[:60]:<60}", end="", flush=True)

    try:
        log = convert_retail(a.out, opts, progress=prog)
    except FileNotFoundError as exc:
        sys.exit(str(exc))
    print(f"\nConcluído em {log['seconds']} s: PNG {log['png']}, F3M {log['f3m_models']}+{log['f3m_skeletons']}, "
          f"GLB {log['glb']}, mapas {log['maps']}, scripts {log.get('scripts', 0)}, texturas {log.get('textures', 0)}, falhas {len(log['failures'])}")
    print(f"Relatório: {layout.report}")


def cmd_export_scripts(a) -> None:
    from .extractor import export_scripts
    try:
        log = export_scripts(a.out)
    except FileNotFoundError as exc:
        sys.exit(str(exc))
    print(f"scripts gravados em CONVERTED/scripts: {log['scripts']}; falhas {len(log['failures'])}")
    for f in log["failures"][:10]:
        print("  ", f)


def cmd_export_textures(a) -> None:
    from .extractor import export_textures
    try:
        log = export_textures(a.out, a.overwrite)
    except FileNotFoundError as exc:
        sys.exit(str(exc))
    print(f"texturas em CONVERTED/textures: {log['textures']}; nao encontradas/falharam: {len(log['textures_missing'])}; falhas {len(log['failures'])}")
    for m in log["textures_missing"][:15]:
        print("  faltando:", m)
    if len(log["textures_missing"]) > 15:
        print(f"  (+{len(log['textures_missing']) - 15}; lista completa em CONVERTED/textures/textures_summary.txt)")


def _stf_lookup(layout):
    """v19: acha o English.stf (CONVERTED/scripts, F3VB_STF ou a pasta da demo) e devolve id(1-based) -> texto; None se nao achar."""
    import os
    cands = [os.environ.get("F3VB_STF", ""), str(layout.converted / "scripts" / "English.stf"), str(layout.root / "English.stf")]
    for c in cands:
        if c and Path(c).is_file():
            data = Path(c).read_bytes()
            try:
                st = parse_stf(data)
            except StfError:
                continue
            return lambda i, st=st, data=data: st.text_id(data, i)
    return None


def cmd_import_loose(a) -> None:
    """v19: copia os arquivos soltos da demo (Fonts\\*.ttf, English.stf) para original_assets."""
    from .layout import AssetsLayout
    from .loose import import_loose
    layout = AssetsLayout(a.out).ensure()
    demo = Path(_game_dir(a.demo))
    try:
        r = import_loose(demo, layout.root, overwrite=a.overwrite)
    except FileNotFoundError as exc:
        sys.exit(f"Erro: {exc}")
    for c in r["copied"]:
        print(f"copiado  {c}")
    for c in r["skipped"]:
        print(f"igual    {c}")
    for m in r["missing"]:
        print(f"FALTANDO {m}")
    print(f"{r['fonts']} fonte(s) TrueType, {len(r['copied'])} copiado(s), {len(r['skipped'])} ja igual(is)")
    if r["missing"]:
        print("Confira se a pasta e a da demo (a que tem F3.exe, Fonts e English.stf).")


def cmd_int_info(a) -> None:
    """v19: arvore de widgets de UM .int (nome do recurso no RETAIL ou caminho de arquivo), com os textos do .stf."""
    from .intfile import IntError, describe, read_int
    from .layout import AssetsLayout
    layout = AssetsLayout(a.out).ensure()
    if Path(a.name).is_file():
        data = Path(a.name).read_bytes()
    else:
        from .index import RetailIndex
        ix = RetailIndex.from_csv(layout.root)
        pk, num = _find_resource(ix, a.name, "INT_Interface")
        data = ix.read(pk, num)
    try:
        f = read_int(data)
    except IntError as exc:
        sys.exit(f"nao decodificou: {exc}")
    print("\n".join(describe(f, _stf_lookup(layout))))


def cmd_ui_info(a) -> None:
    """v18: inventario e sonda dos .int (layouts de tela) do pack Interface + imagens da UI em CONVERTED/ui/*.f3tex."""
    from .index import RetailIndex
    from .layout import AssetsLayout
    from .uiinfo import export_ui_images, ui_report
    layout = AssetsLayout(a.out).ensure()
    if not layout.index_csv.exists():
        sys.exit(f"{layout.index_csv} nao existe: rode extract-all antes (precisa da pasta do jogo uma vez).")
    ix = RetailIndex.from_csv(layout.root)
    text = ui_report(ix, _stf_lookup(layout))
    dest = Path(a.report) if a.report else layout.converted / "ui" / "ui_report.txt"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text, encoding="utf-8")
    print(text[:2500] + ("..." if len(text) > 2500 else ""))
    print(f"relatorio completo: {dest}")
    if not a.no_images:
        n, fails = export_ui_images(ix, layout.converted, a.overwrite)
        print(f"imagens da interface em CONVERTED/ui: {n}; falhas {len(fails)}")
        for f in fails[:10]:
            print("  ", f)


def cmd_export_f3m(a) -> None:
    game = _game_dir(a.game_dir)
    with ResourceIndex(game) as ix:
        data = ix.read_named(a.name, "G3D_Model")
        if data is None:
            sys.exit(f"modelo '{a.name}' não encontrado (use export-list G3D_Model)")
        joints, clips = None, []
        sk = a.skeleton if a.skeleton is not None else a.name
        if sk:
            sd = ix.read_named(sk, "SKEL_Skeleton")
            if sd is not None:
                joints, _ = read_skeleton(sd)
        if joints:
            rows = ix.search("GR2_Animation", (a.anim if a.anim is not None else a.name + "_"))
            if a.max_anims:
                rows = rows[:a.max_anims]
            names = {j.name for j in joints}
            for nm, pk, num in rows:
                try:
                    clips += read_clips(ix.read(pk, num), names, clip_name=nm)
                except Exception as exc:
                    print(f"  aviso: {nm}: {exc}")
        from .b3d import read_b3d as _rb
        ex = Exporter(unit_scale=a.scale, convert=not a.no_convert, mesh_scale=a.mesh_scale)
        sc = ex.scene(_rb(data), a.name, texture_loader=lambda stem: ix.read_named(stem, "TGA_Image"), joints=joints, clips=clips)
    out = Path(a.out) if a.out else Path("private/f3m") / f"{a.name}.f3m"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(write_f3m(sc))
    st = ex.stats
    print(f"F3M salvo em {out}: {st['vertices']} vértices, {st['triangles']} triângulos, {st['textures']} texturas, "
          f"{st['joints']} ossos, {st['animations']} animações")
    for w in ex.warnings[:10]:
        print("  aviso:", w)


def cmd_zipdir(a) -> None:
    import zipfile
    src = Path(a.dir)
    dest = Path(a.zip)
    dest.parent.mkdir(parents=True, exist_ok=True)
    n = 0
    with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(src.rglob("*")):
            if p.is_file() and p.resolve() != dest.resolve():
                z.write(p, p.relative_to(src).as_posix())   # barras normais (o Compress-Archive do PS 5.1 usa \\)
                n += 1
    print(f"{n} arquivo(s) em {dest}")


def cmd_report(a) -> None:
    game = _game_dir(a.game_dir)
    inv = inventory(game, with_hash=a.hash)
    report = {"schema": 1, "inventory": {k: v for k, v in inv.items() if k != "files"},
              "probes": []}
    report["grps"] = []
    vistos = set()
    for e in sorted(inv["grp_files"], key=lambda x: (len(x["path"]), x["path"].lower())):
        chave = (e["path"].split("/")[-1].lower(), e["size"])
        if chave in vistos:
            print(f"Duplicata ignorada: {e['path']}", file=sys.stderr)
            report.setdefault("duplicate_grps_ignored", []).append(e["path"])
            continue
        vistos.add(chave)
        print(f"Sondando {e['path']} ...", file=sys.stderr)
        if a.raw_probe:
            report["probes"].append(probe_file(Path(game) / e["path"]))
        try:
            report["grps"].append(_grp_summary(Path(game) / e["path"], aggregates=True))
        except GrpError as exc:
            report["grps"].append({"path": e["path"], "error": str(exc)})
    rht_entry = inv["notable"].get("resource.rht")
    if rht_entry:
        try:
            report["rht"] = read_rht(Path(game) / rht_entry["path"]).summary()
        except RhtError as exc:
            report["rht"] = {"error": str(exc)}
    report = json.loads(json.dumps(report))  # normaliza chaves (int -> str), igual ao que `resumo` relê
    Path(a.out).write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Relatório salvo em {a.out}")
    if a.resumo:
        Path(a.resumo).write_text(format_resumo(report), encoding="utf-8")
        print(f"Resumo salvo em {a.resumo}")


def cmd_resumo(a) -> None:
    report = json.loads(Path(a.report).read_text(encoding="utf-8"))
    print(format_resumo(report))


def main(argv=None) -> None:
    ap = argparse.ArgumentParser(prog="f3vb")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("inventory", help="inventário da pasta do jogo")
    p.add_argument("game_dir", nargs="?")
    p.add_argument("--hash", action="store_true", help="calcular SHA-256 (lento)")
    p.add_argument("--json")
    p.set_defaults(fn=cmd_inventory)

    p = sub.add_parser("probe", help="sondar um .grp sem assumir o layout")
    p.add_argument("file")
    p.add_argument("--head", type=int, default=128)
    p.set_defaults(fn=cmd_probe)

    p = sub.add_parser("grp", help="ler cabeçalho e tabela de lumps de um .grp")
    p.add_argument("file")
    p.add_argument("--list", action="store_true", help="listar todos os lumps")
    p.add_argument("--extract", metavar="DIR", help="extrair lumps (use private/)")
    p.set_defaults(fn=cmd_grp)

    p = sub.add_parser("rht", help="ler resource.rht e resumir")
    p.add_argument("file")
    p.add_argument("--entries", type=int, default=0, metavar="N", help="mostrar N entradas")
    p.set_defaults(fn=cmd_rht)

    p = sub.add_parser("extract", help="extrair lumps de um .grp (filtro por classe/índice) + manifest.json")
    p.add_argument("file")
    p.add_argument("--out", default="private/extract")
    p.add_argument("--classes", help="ex.: gr2,b3d,tga")
    p.add_argument("--indices", help="ex.: 10,11,12")
    p.add_argument("--limit", type=int)
    p.set_defaults(fn=cmd_extract)

    p = sub.add_parser("sample", help="escolher poucos lumps por classe e gerar ZIP p/ análise")
    p.add_argument("file")
    p.add_argument("--out", default="private/sample")
    p.add_argument("--classes", default="gr2,b3d,tga")
    p.add_argument("--per-class", type=int, default=2)
    p.add_argument("--min-bytes", type=int, default=1024)
    p.add_argument("--max-bytes", type=int, default=3_000_000)
    p.add_argument("--strategy", choices=["smallest", "median", "largest"], default="smallest")
    p.add_argument("--no-zip", action="store_true")
    p.add_argument("--so-texto", action="store_true",
                   help="ZIP só com .txt (manifesto e hexdumps), sem os arquivos binários do jogo")
    p.set_defaults(fn=cmd_sample)

    p = sub.add_parser("gr2", help="resumo de um .gr2 (header, seções, compressão, malha/esqueleto/animação)")
    p.add_argument("file")
    p.add_argument("--no-deep", action="store_true", help="só cabeçalho e seções")
    p.set_defaults(fn=cmd_gr2)

    p = sub.add_parser("inspect", help="classificar e sondar um arquivo (ou um lump: --grp ARQ.grp INDICE)")
    p.add_argument("file", help="caminho do arquivo, ou índice do lump se usar --grp")
    p.add_argument("--grp")
    p.add_argument("--no-deep", action="store_true")
    p.set_defaults(fn=cmd_inspect)

    p = sub.add_parser("catalog", help="cruzar resource.rht com os .grp (nomes, tipos, validação)")
    p.add_argument("game_dir", nargs="?")
    p.add_argument("--rht", help="caminho do resource.rht (padrão: procura na pasta)")
    p.add_argument("--out", default="private/catalogo")
    p.set_defaults(fn=cmd_catalog)

    p = sub.add_parser("amxdis", help="desmontar um script AMX (ou --check para só validar)")
    p.add_argument("file", help="arquivo .amx, ou índice do lump se usar --grp")
    p.add_argument("--grp")
    p.add_argument("--check", action="store_true")
    p.set_defaults(fn=cmd_amxdis)

    p = sub.add_parser("b3dfail", help="texto com hexdump dos .b3d não decodificados (por motivo)")
    p.add_argument("file", help=".grp com modelos")
    p.add_argument("--rht", help="resource.rht (para mostrar os nomes dos modelos)")
    p.add_argument("--per-reason", type=int, default=2)
    p.add_argument("--max-bytes", type=int, default=4096)
    p.add_argument("--out")
    p.set_defaults(fn=cmd_b3dfail)

    p = sub.add_parser("export-list", help="listar recursos por tipo (ex.: G3D_Model, SKEL_Skeleton, GR2_Animation)")
    p.add_argument("type", help="G3D_Model | SKEL_Skeleton | GR2_Animation | TGA_Image | ...")
    p.add_argument("--prefix", default="")
    p.add_argument("--limit", type=int, default=60)
    p.add_argument("game_dir", nargs="?")
    p.set_defaults(fn=cmd_export_list)

    p = sub.add_parser("export-glb", help="exportar um modelo (+esqueleto, animações e texturas) para GLB")
    p.add_argument("name", help="nome do modelo (ver export-list G3D_Model)")
    p.add_argument("game_dir", nargs="?")
    p.add_argument("--skeleton", help="nome do esqueleto (padrão: o mesmo nome do modelo, se existir)")
    p.add_argument("--anim", help="prefixo das animações (padrão: NOME_)")
    p.add_argument("--max-anims", type=int, default=20, help="0 = todas")
    p.add_argument("--no-anim", action="store_true")
    p.add_argument("--scale", type=float, default=1.0, help="fator de escala (1 unidade do jogo = N metros)")
    p.add_argument("--no-convert", action="store_true", help="não converter mão-esquerda para mão-direita")
    p.add_argument("--mesh-scale", type=float, default=DEFAULT_MESH_SCALE,
                   help="escala da geometria do B3D em relação ao esqueleto (padrão 100/1024 = 1/10.24; 1 = sem correção, como no v11)")
    p.add_argument("--out")
    p.set_defaults(fn=cmd_export_glb)

    p = sub.add_parser("stf", help="ler a tabela de strings (.stf) e consultar ids de texto")
    p.add_argument("file")
    p.add_argument("--id", type=int, action="append", help="id de texto (pode repetir)")
    p.add_argument("--info", action="store_true")
    p.set_defaults(fn=cmd_stf)

    p = sub.add_parser("extract-all", help="EXTRAIR todos os arquivos dos .grp (tamanho real) para uma pasta")
    p.add_argument("game_dir", nargs="?")
    p.add_argument("--out", default=None, help="pasta de destino (padrão: <projeto>/original_assets, ou F3VB_ASSETS_DIR)")
    p.add_argument("--plan-only", action="store_true", help="só mostrar quantos arquivos e bytes")
    p.add_argument("--no-raw", action="store_true", help="não regravar RETAIL: converter a partir do RETAIL existente")
    p.add_argument("--png", action="store_true", help="converter texturas para PNG")
    p.add_argument("--glb", action="store_true", help="converter modelos para GLB")
    p.add_argument("--f3m", action="store_true", help="converter modelos/esqueletos para F3M (visualizador)")
    p.add_argument("--maps", action="store_true", help="decodificar mapas (.map + .rle) para CONVERTED\\maps")
    p.add_argument("--anims", type=int, default=8, help="animações por modelo (0 = todas)")
    p.set_defaults(fn=cmd_extract_all)

    p = sub.add_parser("b3dmat", help="materiais, texturas, UVs e ossos de um modelo B3D (le de original_assets/RETAIL)")
    p.add_argument("name", help="nome do modelo, ex.: HumFStr")
    p.add_argument("--assets", default=None, help="pasta original_assets (padrão: <projeto>/original_assets)")
    p.add_argument("--out", default=None, help="gravar o relatório neste .txt")
    p.set_defaults(fn=cmd_b3dmat)

    p = sub.add_parser("pack-sample", help="monta um .zip de amostra (b3d + esqueleto + texturas + animações) de modelos de RETAIL, para enviar para análise")
    p.add_argument("names", nargs="+", help="nomes dos modelos, ex.: HumFStr HumMNor")
    p.add_argument("--anims", type=int, default=6, help="animações por modelo (padrão 6; 0 = nenhuma)")
    p.add_argument("--assets", default=None, help="pasta original_assets (padrão: <projeto>/original_assets)")
    p.add_argument("--out", default=None, help="zip de saída (padrão: <projeto>/private/amostra_*.zip)")
    p.set_defaults(fn=cmd_pack_sample)

    p = sub.add_parser("map-info", help="decodifica um mapa (.map + .rle): relatório, JSON e imagem de visão superior")
    p.add_argument("name", help="nome (ou trecho) do recurso MAP_Map em RETAIL, ou caminho de um arquivo .map")
    p.add_argument("--assets", default=None, help="pasta original_assets (padrão: <projeto>/original_assets)")
    p.add_argument("--rle", default=None, help="com um .map avulso: caminho do .rle")
    p.add_argument("--out", default=None, help="gravar o relatório neste .txt")
    p.add_argument("--json", default=None, help="gravar o mapa decodificado neste .json")
    p.add_argument("--png", default=None, help="gravar a imagem de visão superior neste .png")
    p.add_argument("--tre", default=None, help="com um .map avulso: caminho do .8tre")
    p.add_argument("--f3map", default=None, help="gravar o mapa para o F3VBMapViewer neste .f3map")
    p.set_defaults(fn=cmd_map_info)

    p = sub.add_parser("tre-info", help="geometria 3D de um .8tre (blocos, nós, triângulos, caixa, materiais)")
    p.add_argument("name", help="nome do recurso TRE_Octree em RETAIL ou caminho de um .8tre")
    p.add_argument("--assets", default=None)
    p.set_defaults(fn=cmd_tre_info)

    p = sub.add_parser("export-scripts", help="copia os scripts .amx do RETAIL para CONVERTED/scripts (o F3VBMapViewer v17 liga entidade -> script pelo nome)")
    p.add_argument("--out", default=None, help="pasta original_assets (padrao: a de sempre)")
    p.set_defaults(fn=cmd_export_scripts)

    p = sub.add_parser("export-textures", help="converte as texturas (difusas e lightmaps) citadas pelos mapas para CONVERTED/textures/*.f3tex (precisa dos .f3map)")
    p.add_argument("--out", default=None, help="pasta original_assets (padrao: a de sempre)")
    p.add_argument("--overwrite", action="store_true", help="refaz as que ja existem")
    p.set_defaults(fn=cmd_export_textures)

    p = sub.add_parser("ui-info", help="inventario do pack Interface, arvores dos .int (layouts, v19) e imagens da UI -> CONVERTED/ui/*.f3tex")
    p.add_argument("--out", default=None, help="pasta original_assets (padrao: a de sempre)")
    p.add_argument("--report", default=None, help="onde gravar o relatorio (padrao: CONVERTED/ui/ui_report.txt)")
    p.add_argument("--no-images", action="store_true", help="so o relatorio")
    p.add_argument("--overwrite", action="store_true")
    p.set_defaults(fn=cmd_ui_info)

    p = sub.add_parser("import-loose", help="v19: copia Fonts\\*.ttf e English.stf (arquivos soltos da demo) para original_assets/RETAIL")
    p.add_argument("demo", nargs="?", default=None, help="pasta da demo (a que tem F3.exe, Fonts e English.stf); padrao: a de sempre")
    p.add_argument("--out", default=None, help="pasta original_assets (padrao: a de sempre)")
    p.add_argument("--overwrite", action="store_true", help="copia mesmo se o destino ja for igual")
    p.set_defaults(fn=cmd_import_loose)

    p = sub.add_parser("int-info", help="v19: arvore de widgets de um .int (layout de tela), com textos do .stf")
    p.add_argument("name", help="nome do recurso INT_Interface (ex.: Mainmenu) ou caminho de um .int")
    p.add_argument("--out", default=None, help="pasta original_assets (padrao: a de sempre)")
    p.set_defaults(fn=cmd_int_info)

    p = sub.add_parser("tre-check", help="confere o parser de lotes em todos os .8tre de uma pasta (ou do RETAIL)")
    p.add_argument("folder", nargs="?", default=None, help="pasta com .8tre (ex.: TRE_Octree descompactado); sem ela usa o RETAIL")
    p.add_argument("--assets", default=None)
    p.set_defaults(fn=cmd_tre_check)

    p = sub.add_parser("rle-info", help="grade de tiles de um .rle (dimensões, histograma, imagem)")
    p.add_argument("name", help="nome do recurso RLE em RETAIL ou caminho de um .rle")
    p.add_argument("--assets", default=None)
    p.add_argument("--png", default=None, help="gravar a grade como .png")
    p.set_defaults(fn=cmd_rle_info)

    p = sub.add_parser("convert-retail", help="refazer CONVERTED (png/f3m/glb) a partir de original_assets/RETAIL, sem precisar dos .grp")
    p.add_argument("--out", default=None, help="pasta original_assets (padrão: <projeto>/original_assets)")
    p.add_argument("--png", action="store_true")
    p.add_argument("--glb", action="store_true")
    p.add_argument("--f3m", action="store_true")
    p.add_argument("--maps", action="store_true", help="decodificar os mapas (.map + .rle) para CONVERTED\\maps (txt, json, png)")
    p.add_argument("--anims", type=int, default=8, help="animações por modelo (0 = todas)")
    p.set_defaults(fn=cmd_convert_retail)

    p = sub.add_parser("export-f3m", help="exportar UM modelo (+esqueleto, animações, texturas) para o visualizador")
    p.add_argument("name")
    p.add_argument("game_dir", nargs="?")
    p.add_argument("--skeleton")
    p.add_argument("--anim")
    p.add_argument("--max-anims", type=int, default=20)
    p.add_argument("--scale", type=float, default=1.0)
    p.add_argument("--no-convert", action="store_true")
    p.add_argument("--mesh-scale", type=float, default=DEFAULT_MESH_SCALE,
                   help="escala da geometria do B3D em relação ao esqueleto (padrão 100/1024; 1 = sem correção, como no v11)")
    p.add_argument("--out")
    p.set_defaults(fn=cmd_export_f3m)

    p = sub.add_parser("zipdir", help="zipar uma pasta com barras normais (portável)")
    p.add_argument("dir")
    p.add_argument("zip")
    p.set_defaults(fn=cmd_zipdir)

    p = sub.add_parser("report", help="inventário + sondagem de todos os .grp em JSON")
    p.add_argument("game_dir", nargs="?")
    p.add_argument("--hash", action="store_true")
    p.add_argument("-o", "--out", default="inspect_report.json")
    p.add_argument("--resumo", metavar="ARQ.txt", help="também salvar o resumo curto em texto")
    p.add_argument("--raw-probe", action="store_true", help="incluir a varredura crua dos .grp (lenta)")
    p.set_defaults(fn=cmd_report)

    p = sub.add_parser("resumo", help="imprimir o resumo curto de um report.json")
    p.add_argument("report")
    p.set_defaults(fn=cmd_resumo)

    a = ap.parse_args(argv)
    a.fn(a)


if __name__ == "__main__":
    main()
