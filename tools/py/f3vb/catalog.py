"""Catálogo de recursos: cruza o resource.rht com os .grp reais.

Para cada entrada do .rht: pack -> nome do .grp, number -> índice do lump, offset -> nome do recurso
(hipótese), type -> tipo. Valida a hipótese comparando o tipo com a classe detectada no conteúdo do lump.
"""
from __future__ import annotations

import os
from collections import Counter, defaultdict
from pathlib import Path

from .classify import classify
from .grp import GrpError, GrpFile
from .rht import RhtInfo

BASE = lambda label: label.split(" ")[0].split(":")[0].rstrip("?")  # noqa: E731


def find_grps(game_dir: str | os.PathLike) -> tuple[dict, list]:
    """nome-base (minúsculo) -> caminho. Duplicatas (mesmo nome e tamanho) são ignoradas e listadas."""
    root = Path(game_dir)
    found: dict[str, Path] = {}
    dups: list[str] = []
    for p in sorted(root.rglob("*"), key=lambda q: (len(q.parts), str(q).lower())):
        if p.is_file() and p.suffix.lower() == ".grp":
            key = p.stem.lower()
            if key in found:
                same = found[key].stat().st_size == p.stat().st_size
                dups.append(f"{p} ({'idêntico em tamanho a' if same else 'DIFERENTE de'} {found[key]})")
                continue
            found[key] = p
    return found, dups


def build_catalog(game_dir, rht: RhtInfo) -> dict:
    grps, dups = find_grps(game_dir)
    by_pack: dict[str, list] = defaultdict(list)
    for e in rht.entries:
        by_pack[rht.pack_label(e.pack)].append(e)

    rows: list[dict] = []
    pack_report: dict[str, dict] = {}
    agree: dict[str, Counter] = defaultdict(Counter)
    issues: list[str] = []
    for pack, ents in sorted(by_pack.items()):
        path = grps.get(pack.lower())
        rep = {"entries": len(ents), "grp": str(path) if path else None, "lumps": None,
               "count_match": None, "numbers_are_permutation": None}
        nums = sorted(e.number for e in ents)
        rep["numbers_are_permutation"] = nums == list(range(len(ents)))
        if path is None:
            issues.append(f"pack '{pack}': .grp não encontrado na pasta")
            pack_report[pack] = rep
            continue
        try:
            with GrpFile(path) as g:
                rep["lumps"] = g.info.n_files
                rep["count_match"] = g.info.n_files == len(ents)
                for e in ents:
                    cls, length = None, None
                    if 0 <= e.number < g.info.n_files:
                        lp = g.info.lumps[e.number]
                        length = lp.length
                        cls = classify(g.head(lp, 64), lp.length) if lp.end <= g.info.file_size else "fora"
                    else:
                        issues.append(f"{pack}#{e.number}: índice fora do .grp ({g.info.n_files} lumps)")
                    base = BASE(cls) if cls else "?"
                    agree[e.type_name][base] += 1
                    rows.append({"pack": pack, "number": e.number, "type": e.type, "type_name": e.type_name,
                                 "name": rht.resource_name(e.offset), "class": cls, "length": length,
                                 "offset": e.offset})
        except GrpError as exc:
            issues.append(f"pack '{pack}': {exc}")
        pack_report[pack] = rep

    # lumps de .grp que nenhuma entrada do .rht referencia
    referenced = {(r["pack"], r["number"]) for r in rows}
    unreferenced = 0
    for pack, rep in pack_report.items():
        if rep["lumps"]:
            unreferenced += sum(1 for i in range(rep["lumps"]) if (pack, i) not in referenced)

    # consistência dos nomes: a distância entre offsets sucessivos deve ser len(nome)+1
    ordered = sorted(rht.entries, key=lambda e: e.offset)
    ok = bad = 0
    for a, b in zip(ordered, ordered[1:]):
        name = rht.resource_name(a.offset)
        if name is None or b.offset == a.offset:
            continue
        if b.offset - a.offset == len(name) + 1:
            ok += 1
        else:
            bad += 1
    names_present = sum(1 for r in rows if r["name"])
    return {"rows": rows, "pack_report": pack_report, "agreement": {k: dict(v) for k, v in agree.items()},
            "issues": issues, "duplicates_ignored": dups, "unreferenced_lumps": unreferenced,
            "name_spacing": {"consistent": ok, "inconsistent": bad}, "names_present": names_present}


def catalog_tsv(cat: dict) -> str:
    out = ["pack\tnumero\ttipo_id\ttipo\tclasse_detectada\ttamanho\tnome_do_recurso"]
    for r in sorted(cat["rows"], key=lambda r: (r["pack"].lower(), r["number"])):
        out.append(f"{r['pack']}\t{r['number']}\t{r['type']}\t{r['type_name']}\t{r['class']}\t{r['length']}\t{r['name']}")
    return "\n".join(out) + "\n"


def catalog_summary(cat: dict, rht: RhtInfo) -> str:
    L = ["=== CATÁLOGO: cruzamento resource.rht x .grp ===",
         f"entradas no .rht: {len(rht.entries)}   linhas resolvidas: {len(cat['rows'])}   "
         f"nomes de recurso lidos: {cat['names_present']}   lumps sem entrada: {cat['unreferenced_lumps']}"]
    sp = cat["name_spacing"]
    L.append(f"consistência 'offset do próximo - offset = len(nome)+1': {sp['consistent']} ok / {sp['inconsistent']} diferentes")
    L += ["", "--- por pack (nome do .grp) ---", f"{'pack':<12} {'entradas':>8} {'lumps':>7}  contagem  permutação(0..n-1)  .grp"]
    for pack, r in sorted(cat["pack_report"].items()):
        L.append(f"{pack:<12} {r['entries']:>8} {str(r['lumps']):>7}  {str(r['count_match']):<8}  {str(r['numbers_are_permutation']):<18}  {r['grp']}")
    L += ["", "--- tipo do .rht x classe detectada no conteúdo (concordância) ---"]
    for tname, dist in sorted(cat["agreement"].items()):
        total = sum(dist.values())
        parts = ", ".join(f"{k}={v}" for k, v in sorted(dist.items(), key=lambda kv: -kv[1]))
        L.append(f"{tname:<18} total={total:<5} {parts}")
    L += ["", "--- amostras de nomes por tipo (até 4) ---"]
    seen: dict[str, list] = defaultdict(list)
    for r in sorted(cat["rows"], key=lambda r: (r["pack"].lower(), r["number"])):
        if len(seen[r["type_name"]]) < 4:
            seen[r["type_name"]].append(f"{r['pack']}#{r['number']}={r['name']!r}")
    for t, v in sorted(seen.items()):
        L.append(f"{t:<18} " + " | ".join(v))
    if cat["duplicates_ignored"]:
        L += ["", "--- .grp duplicados ignorados ---"] + cat["duplicates_ignored"]
    if cat["issues"]:
        L += ["", f"--- PROBLEMAS ({len(cat['issues'])}) ---"] + cat["issues"][:30]
    return "\n".join(L) + "\n"
