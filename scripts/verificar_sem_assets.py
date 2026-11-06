"""Verifica que o projeto NAO contem dados do jogo (nem derivados) antes de empacotar ou publicar.

Uso (na raiz do projeto):  py -3 scripts\\verificar_sem_assets.py
Codigo de saida 0 = limpo; 1 = achou algo suspeito (a lista e impressa). Tambem e chamado por empacotar.py.

O que e considerado conteudo do jogo (ou derivado dele):
  - extensoes de formatos do jogo e de saidas convertidas: .grp .rht .gr2 .skl .b3d .tga .bmp .amx .f3m .glb .mve .rle .stf .wav .ogg .mp3
  - qualquer coisa dentro de original_assets/ (exceto os arquivos de protecao que o proprio projeto cria) e de private/
  - arquivos grandes (> 5 MB) fora de third_party/
Pastas ignoradas na varredura: .git, __pycache__, build.
"""
from __future__ import annotations

import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
EXT_JOGO = {".grp", ".rht", ".gr2", ".skl", ".b3d", ".tga", ".bmp", ".amx", ".f3m", ".glb", ".mve", ".rle", ".stf", ".wav", ".ogg", ".mp3"}
IGNORAR = {".git", "__pycache__", "build"}
PERMITIDOS_EM_ASSETS = {".gitignore", "LEIA-ME.txt"}
LIMITE = 5 * 1024 * 1024


def varrer(raiz: Path = RAIZ) -> list[str]:
    achados: list[str] = []
    for p in sorted(raiz.rglob("*")):
        rel = p.relative_to(raiz)
        partes = rel.parts
        if any(x in IGNORAR for x in partes) or not p.is_file():
            continue
        if partes[0] == "original_assets":
            if rel.name not in PERMITIDOS_EM_ASSETS or len(partes) > 2:
                achados.append(f"{rel}  (dentro de original_assets/)")
            continue
        if partes[0] == "private":
            achados.append(f"{rel}  (dentro de private/)")
            continue
        if p.suffix.lower() in EXT_JOGO:
            achados.append(f"{rel}  (extensao {p.suffix.lower()} de dado do jogo ou derivado)")
        elif partes[0] != "third_party" and p.stat().st_size > LIMITE:
            achados.append(f"{rel}  (arquivo grande: {p.stat().st_size // 1024} KB)")
    return achados


def main() -> int:
    achados = varrer()
    if achados:
        print("ATENCAO: possiveis dados do jogo no projeto. NAO empacote nem publique assim:")
        for a in achados:
            print("  -", a)
        return 1
    print("OK: nenhum dado do jogo encontrado no projeto.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
