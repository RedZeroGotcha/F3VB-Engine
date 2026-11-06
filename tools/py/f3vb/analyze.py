"""Agregados sobre TODOS os lumps de um .grp: responde, num único relatório pequeno,
"quais versões/compressões de GR2 existem", "o que os B3D referenciam", etc."""
from __future__ import annotations

from collections import Counter

from .classify import classify
from .gr2tool import summarize_gr2
from .grp import GrpFile
from .amxdis import analyze_amx
from .probes import probe_amx, probe_b3d, probe_tga

BASE = lambda label: label.split(" ")[0].split(":")[0].rstrip("?")  # noqa: E731


def aggregate_grp(g: GrpFile, max_examples: int = 3, max_names: int = 300) -> dict:
    gr2 = Counter(); gr2_ex: dict = {}; gr2_err = Counter()
    deep = {"files_ok": 0, "files_with_errors": 0, "errors": Counter(), "skeletons": 0, "bones_total": 0,
            "animations": 0, "duration_total_s": 0.0, "duration_max_s": 0.0, "transform_tracks_total": 0,
            "codecs": Counter()}
    b3d_n = b3d_bones = b3d_tex = b3d_nodes = 0
    b3d_surf = b3d_verts = b3d_weighted = b3d_complete = b3d_full = 0
    b3d_fam: Counter = Counter()
    b3d_stop = Counter(); b3d_stop_ex: dict = {}; b3d_sig = Counter(); b3d_blend = Counter(); b3d_mtype = Counter()
    tex_files: set = set(); bone_names: Counter = Counter()
    tga_n = tga_ok = 0; tga_dims = Counter()
    amx_n = 0; amx_ver = Counter(); amx_pub: set = set(); amx_nat: set = set(); amx_err = 0
    amx_ok = 0; amx_fail: list = []; amx_called: Counter = Counter(); amx_ops: Counter = Counter(); amx_instr = 0
    amx_arity: dict = {}; amx_consts: dict = {}

    for lp in g.info.lumps:
        if lp.end > g.info.file_size or lp.length == 0:
            continue
        b = BASE(classify(g.head(lp, 64), lp.length))
        if b == "gr2":
            try:
                sm = summarize_gr2(g.read(lp), deep=True)
                h = sm["header"]
                key = f"v{h['version']} ptr{h['pointer_size']} {'+'.join(h['compressions'])}"
                gr2[key] += 1
                gr2_ex.setdefault(key, [])
                if len(gr2_ex[key]) < max_examples:
                    gr2_ex[key].append(lp.index)
                if sm["errors"]:
                    deep["files_with_errors"] += 1
                    for e in sm["errors"]:
                        deep["errors"][f"{e['stage']}: {e['error'][:70]}"] += 1
                else:
                    deep["files_ok"] += 1
                sks = sm.get("skeletons") or []
                deep["skeletons"] += len(sks)
                deep["bones_total"] += sum(x.get("bone_count", 0) for x in sks)
                an = sm.get("animations") or {}
                for a in an.get("animations", []):
                    deep["animations"] += 1
                    dur = float(a.get("duration") or 0.0)
                    deep["duration_total_s"] += dur
                    deep["duration_max_s"] = max(deep["duration_max_s"], dur)
                for tg in an.get("track_groups", []):
                    deep["transform_tracks_total"] += tg.get("transform_track_count", 0)
                    for tr in tg.get("first_transform_tracks", []):
                        for k in ("orientation", "position", "scale_shear"):
                            c = (tr.get(k) or {}).get("codec")
                            if c:
                                deep["codecs"][f"{k}:{c}"] += 1
            except Exception as exc:
                gr2_err[f"{type(exc).__name__}: {str(exc)[:70]}"] += 1
        elif b == "b3d":
            p = probe_b3d(g.read(lp))
            b3d_n += 1
            b3d_sig[p["signature"]] += 1
            st = p.get("stopped")
            skey = (st["reason"][:60] + f" [{st.get('tag','')}]") if st else "chegou ao fim (token 0x01)"
            b3d_stop[skey] += 1
            if st and "ainda não" not in st["reason"]:
                ex = b3d_stop_ex.setdefault(skey, [])
                if len(ex) < 4:
                    ex.append({"lump": lp.index, "offset": st["offset"]})
            b3d_bones += 1 if p.get("bone_count") else 0
            b3d_tex += 1 if p["textures"] else 0
            b3d_nodes += 1 if "first_node_offset" in p else 0
            for sf in p.get("surfaces", []):
                b3d_surf += 1
                b3d_verts += sf["vertices"]
                b3d_weighted += 1 if sf.get("bone_weights") else 0
                b3d_fam[sf["family"]] += 1
                b3d_complete += 1 if sf["complete"] else 0
            b3d_full += 1 if (p.get("stopped") is None) else 0
            for t in p["textures"]:
                if len(tex_files) < max_names:
                    tex_files.add(t["file"])
            for m in p["materials"]:
                b3d_blend[m["blend_state"]] += 1
                b3d_mtype[m["material_type"]] += 1
            for bn in p["bones"]:
                bone_names[bn["name"]] += 1
        elif b == "tga":
            tga_n += 1
            p = probe_tga(g.read(lp))
            if p.get("plausible") and p.get("size_ok", True):
                tga_ok += 1
                tga_dims[f"{p['width']}x{p['height']} {p['bpp']}bpp {p['type_name']}"] += 1
        elif b == "amx":
            amx_n += 1
            data = g.read(lp)
            p = probe_amx(data)
            if "error" in p:
                amx_err += 1
                continue
            amx_ver[f"file_v{p['file_version']} amx_v{p['amx_version']} defsize={p['defsize']} magic={p['magic']}"] += 1
            for nm in p["publics"]:
                if len(amx_pub) < max_names:
                    amx_pub.add(nm)
            for nm in p["natives"]:
                if len(amx_nat) < max_names * 2:
                    amx_nat.add(nm)
            r = analyze_amx(data)
            if r["ok"]:
                amx_ok += 1
                amx_instr += r["instructions"]
                amx_called.update(r["natives_called"])
                amx_ops.update(r["opcodes"])
                for nm, ar in r["native_arity"].items():
                    for na, cnt in ar.items():
                        amx_arity.setdefault(nm, Counter())[na] += cnt
                for nm, pos_map in r["native_const_args"].items():
                    for pos, vals in pos_map.items():
                        amx_consts.setdefault(nm, {}).setdefault(pos, Counter()).update(vals)
            elif len(amx_fail) < 10:
                amx_fail.append({"lump": lp.index, "errors": r["errors"][:2]})

    return {
        "gr2": {"combinations": dict(gr2), "examples_lump_index": gr2_ex, "errors": dict(gr2_err),
                "deep": {**{k: v for k, v in deep.items() if k not in ("errors", "codecs")},
                         "errors": dict(deep["errors"]), "codecs_first_tracks": dict(deep["codecs"]),
                         "duration_total_s": round(deep["duration_total_s"], 2),
                         "duration_max_s": round(deep["duration_max_s"], 2)}},
        "b3d": {"count": b3d_n, "signatures": dict(b3d_sig), "stop_reasons": dict(b3d_stop), "stop_examples": b3d_stop_ex,
                "fully_decoded_files": b3d_full, "surfaces_complete": b3d_complete, "surface_families": dict(b3d_fam), "surfaces": b3d_surf, "vertices_total": b3d_verts, "surfaces_with_bone_weights": b3d_weighted,
                "with_textures": b3d_tex, "with_top_level_bones": b3d_bones, "reaching_scene_node": b3d_nodes,
                "blend_states": dict(b3d_blend.most_common(10)), "material_types": dict(b3d_mtype.most_common(10)),
                "texture_files": sorted(tex_files), "top_bone_names": dict(bone_names.most_common(25))},
        "tga": {"candidates": tga_n, "plausible": tga_ok, "dimensions_top": dict(tga_dims.most_common(15))},
        "amx": {"candidates": amx_n, "invalid": amx_err, "versions": dict(amx_ver),
                "decoded_ok": amx_ok, "decode_failures": amx_fail, "instructions_total": amx_instr,
                "natives_called": dict(amx_called.most_common()),
                "native_arity": {k: dict(v) for k, v in amx_arity.items()},
                "native_const_args": {k: {p: dict(c.most_common(5)) for p, c in v.items()} for k, v in amx_consts.items()}, "opcodes_used": dict(amx_ops.most_common()),
                "publics": sorted(amx_pub), "natives": sorted(amx_nat)},
    }


def format_resumo(report: dict) -> str:
    L: list[str] = []
    inv = report.get("inventory", {})
    L.append("=== RESUMO F3VB ===")
    L.append(f"Pasta: {inv.get('root')}")
    L.append(f"Arquivos: {inv.get('total_files')}  Bytes: {inv.get('total_bytes')}  Override: {inv.get('has_override_dir')}")
    for n, e in (inv.get("notable") or {}).items():
        L.append(f"  {n}: {'ok (' + str(e['size']) + ' B)' if e else 'AUSENTE'}")
    L.append("Extensões (top 12): " + ", ".join(f".{k}={v['files']}" for k, v in list(inv.get("by_ext", {}).items())[:12]))
    for g in report.get("grps", []):
        L.append("")
        if "error" in g:
            L.append(f"[GRP] {g['path']}  ERRO: {g['error']}")
            continue
        L.append(f"[GRP] {g['path']}  lumps={g['lumps']}  v{g['version'][0]}.{g['version'][1]}  issues={g['issues_total']}")
        L.append("  classes: " + ", ".join(f"{k}={v}" for k, v in list(g["classes"].items())[:10]))
        a = g.get("aggregates")
        if not a:
            continue
        L.append(f"  GR2 combinações: {a['gr2']['combinations'] or 'nenhum'}")
        if a["gr2"]["errors"]:
            L.append(f"  GR2 erros: {a['gr2']['errors']}")
        d = a["gr2"]["deep"]
        if d["files_ok"] or d["files_with_errors"]:
            L.append(f"  GR2 leitura profunda: ok={d['files_ok']} com_erro={d['files_with_errors']} esqueletos={d['skeletons']} "
                     f"ossos={d['bones_total']} animações={d['animations']} duração_total={d['duration_total_s']}s "
                     f"maior={d['duration_max_s']}s trilhas_transform={d['transform_tracks_total']}")
            L.append(f"       codecs (primeiras trilhas): {d['codecs_first_tracks']}  erros: {d['errors']}")
        b = a["b3d"]
        if b["count"]:
            L.append(f"  B3D: {b['count']} (com texturas={b['with_textures']}, ossos no topo={b['with_top_level_bones']}, chegam a SceneNode={b['reaching_scene_node']})")
            L.append(f"       ARQUIVOS decodificados por completo={b['fully_decoded_files']}/{b['count']}  superfícies completas={b['surfaces_complete']}/{b['surfaces']}  famílias={b['surface_families']}")
            L.append(f"       superfícies lidas={b['surfaces']}  vértices (soma)={b['vertices_total']}  com pesos de osso={b['surfaces_with_bone_weights']}")
            L.append(f"       assinaturas={b['signatures']}  paradas={b['stop_reasons']}")
            if b.get("stop_examples"):
                L.append(f"       exemplos de paradas inesperadas (lump, offset): {b['stop_examples']}")
            L.append(f"       blend={b['blend_states']}  material_type={b['material_types']}")
            L.append(f"       texturas citadas (até 15): {b['texture_files'][:15]}")
            L.append(f"       ossos mais comuns: {b['top_bone_names']}")
        t = a["tga"]
        if t["candidates"]:
            L.append(f"  TGA: candidatos={t['candidates']} plausíveis={t['plausible']}  top dimensões={t['dimensions_top']}")
        m = a["amx"]
        if m["candidates"]:
            L.append(f"  AMX (Pawn?): {m['candidates']} candidatos, inválidos={m['invalid']}, versões={m['versions']}")
            L.append(f"       DECODIFICADOS por completo (instruction set clássico): {m['decoded_ok']}/{m['candidates']}  instruções={m['instructions_total']}  falhas={m['decode_failures']}")
            L.append(f"       opcodes usados ({len(m['opcodes_used'])}/138): {m['opcodes_used']}")
            L.append(f"       natives CHAMADAS (contagem de chamadas no código): {m['natives_called']}")
            L.append(f"       publics (até 40): {m['publics'][:40]}")
            L.append(f"       natives (até 60): {m['natives'][:60]}")
    r = report.get("rht")
    if r:
        L.append("")
        if "error" in r:
            L.append(f"[RHT] ERRO: {r['error']}")
        else:
            L.append(f"[RHT] entradas={r['entries']} v{r['version'][0]}.{r['version'][1]} offsetPacks={r['offset_packs']} offsetResources={r['offset_resources']}")
            L.append("  por tipo: " + ", ".join(f"{k}={v['count']}" for k, v in r["by_type"].items()))
            L.append(f"  tipos fora da tabela: {r['unknown_type_ids']}  packs fora do enum: {r['unknown_pack_values']}")
            L.append(f"  campo one: {r['field_one_values']}  number únicos={r['number_unique']} faixa={r['number_range']}  offset faixa={r['offset_range']}")
            L.append(f"  strings da região de packs: {r['pack_region_strings'][:12]}")
    return "\n".join(L)
