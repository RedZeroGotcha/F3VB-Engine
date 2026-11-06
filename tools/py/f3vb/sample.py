"""Monta um .zip de AMOSTRA de um ou mais modelos a partir de original_assets/RETAIL, para enviar para analise.

Inclui, por modelo: o .b3d, o esqueleto (.skl), todas as texturas citadas pelos materiais (.tga/.bmp) e N animacoes (.gr2,
da propria familia: CR_Dog_* ou HumM_*/HumF_*). Mantem os nomes originais e a estrutura Pack/Tipo/Nome.ext do RETAIL.
O zip e conteudo do jogo: serve para enviar a quem esta ajudando a depurar, nunca para publicar (fica em private/, ignorada pelo git).
"""
from __future__ import annotations

import zipfile
from pathlib import Path

from .b3d import read_b3d
from .index import RetailIndex


def collect_sample(retail_index: RetailIndex, names, anims: int = 6) -> tuple[list, list]:
    """Devolve (linhas (pack, numero, tipo, nome, caminho), avisos) dos arquivos da amostra."""
    from .extractor import anim_prefixes
    rows = {(r[0].lower(), r[1]): r for r in retail_index.rows}
    chosen: dict = {}
    warns: list = []

    def add(pack, number):
        r = rows.get((pack.lower(), number))
        if r:
            chosen[(r[0].lower(), r[1])] = r

    for name in names:
        hit = retail_index.find(name, "G3D_Model")
        if not hit:
            warns.append(f"modelo '{name}' nao encontrado em RETAIL")
            continue
        add(*hit)
        try:
            model = read_b3d(retail_index.read(*hit))
        except Exception as exc:
            warns.append(f"{name}: B3D nao le ({exc})")
            model = None
        sk = retail_index.find(name, "SKEL_Skeleton")
        if sk:
            add(*sk)
        else:
            warns.append(f"{name}: esqueleto nao encontrado")
        tex = set()
        if model is not None:
            for _node, surf in model.surfaces():
                for m in surf.materials:
                    tex.update(t.rsplit(".", 1)[0] for t in m.textures)
        for t in sorted(tex):
            th = retail_index.find(t, "TGA_Image")
            if th:
                add(*th)
            else:
                warns.append(f"{name}: textura '{t}' nao encontrada")
        found = []
        for pre in anim_prefixes(name):
            found += [r for r in retail_index.search("GR2_Animation", pre) if r not in found]
        found.sort(key=lambda r: (0 if any(k in r[0] for k in ("Idle_01", "Walk_01", "Run_01")) else 1, r[0]))
        for _nm, pk, num in found[:anims]:
            add(pk, num)
    return list(chosen.values()), warns


def write_sample_zip(retail_index: RetailIndex, names, out_zip, anims: int = 6) -> tuple[int, int, list]:
    rows, warns = collect_sample(retail_index, names, anims)
    total = 0
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as z:
        for _pack, _num, _tipo, _nome, rel, _size in rows:
            p = Path(retail_index.retail) / rel
            z.write(p, rel)
            total += p.stat().st_size
        z.writestr("LEIA-ME.txt", "Amostra do jogo (conteudo protegido): nao publicar.\nModelos: " + ", ".join(names) + "\n")
    return len(rows), total, warns
