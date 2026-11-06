"""Relatório em texto dos .b3d que a sonda NÃO decodifica por completo: agrupa por motivo e mostra o menor
arquivo de cada grupo com hexdump (para decifrar o layout sem enviar o arquivo do jogo)."""
from __future__ import annotations

import re
from collections import defaultdict

from .classify import classify
from .grp import GrpFile
from .probe import hexdump
from .probes import probe_b3d


def _key(reason: str) -> str:
    return re.sub(r"-?\d+", "N", reason)[:90]


def b3d_failures(grp_path, rht=None, per_reason: int = 2, max_bytes: int = 4096) -> str:
    names: dict = {}
    if rht is not None:
        pack = None
        stem = str(grp_path).replace("\\", "/").split("/")[-1].rsplit(".", 1)[0].lower()
        for e in rht.entries:
            if rht.pack_label(e.pack).lower() == stem:
                names[e.number] = rht.resource_name(e.offset)
    groups: dict = defaultdict(list)
    total = ok = 0
    with GrpFile(grp_path) as g:
        for lp in g.info.lumps:
            if lp.length == 0 or lp.end > g.info.file_size:
                continue
            if not classify(g.head(lp, 64), lp.length).startswith("b3d"):
                continue
            total += 1
            data = g.read(lp)
            p = probe_b3d(data)
            if p.get("ended_at") == len(data) and p["stopped"] is None:
                ok += 1
                continue
            st = p["stopped"] or {"reason": "terminou antes do fim do arquivo", "offset": p.get("ended_at", 0)}
            groups[_key(st["reason"])].append((len(data), lp.index, p, st, data))
    L = [f"B3D em {grp_path}: {total} arquivos, {ok} decodificados por completo, {total - ok} nao"]
    for key, items in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        L += ["", "=" * 78, f"MOTIVO: {key}   ({len(items)} arquivos)"]
        for size, idx, p, st, data in sorted(items, key=lambda t: t[0])[:per_reason]:
            L += ["", f"-- lump #{idx} nome={names.get(idx)!r} tamanho={size} parou em {st.get('offset')} ({st.get('tag','')})",
                  f"   nos={p.get('node_names')}  superficies={[(s['family'], s['vertices'], s['complete']) for s in p.get('surfaces', [])]}",
                  f"   globais={p.get('globals')}  materiais={len(p.get('materials', []))} texturas={len(p.get('textures', []))} ossos={p.get('bone_count', 0)}"]
            if size <= max_bytes:
                L.append(hexdump(data))
            else:
                at = int(st.get("offset") or 0)
                lo = max(0, at - 256)
                L += ["   (arquivo grande: inicio e trecho em torno do ponto de parada)", hexdump(data[:384]),
                      f"   ... trecho {lo}..{lo + 640}", hexdump(data[lo:lo + 640], base=lo)]
    return "\n".join(L) + "\n"
