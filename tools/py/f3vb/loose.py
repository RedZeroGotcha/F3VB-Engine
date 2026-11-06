"""v19: arquivos SOLTOS da demo (fora dos .grp) que o jogo precisa.

A demo do Van Buren guarda dentro dos .grp quase tudo, mas alguns arquivos ficam soltos na pasta dela:

    <demo>\\Fonts\\*.ttf        fontes TrueType usadas pelos rotulos da interface
                                (r_fallouty.ttf = "Fallouty", TT0807M_.TTF = "Gothic 821 Condensed BT",
                                 JH_FALLOUT.TTF = "JH_Fallout")
    <demo>\\Fonts\\fontlist.ini lista de fontes que o jogo registra (quando existir)
    <demo>\\English.stf         tabela de textos (rotulos, descricoes, dialogos)

`import_loose` copia esses arquivos para dentro de original_assets, ao lado do resto do que foi extraido:

    original_assets\\RETAIL\\Fonts\\*.ttf (+ fontlist.ini)
    original_assets\\RETAIL\\English.stf
    original_assets\\CONVERTED\\scripts\\English.stf   (onde o F3VBMapViewer e o F3VBScript procuram)

Assim o F3VB.exe acha tudo so com F3VB_ASSETS_DIR, sem precisar de F3VB_DEMO_DIR.
Nada disso vai para o projeto (regra CLEANROOM): fica so na pasta original_assets do proprio jogador.
"""
from __future__ import annotations

import shutil
from pathlib import Path

FONT_EXTS = (".ttf",)
FONT_EXTRAS = ("fontlist.ini",)


def _find_child(folder: Path, name: str):
    """Acha um filho sem diferenciar maiusculas (a demo mistura 'Fonts', 'fonts', 'TT0807M_.TTF')."""
    if not folder.is_dir():
        return None
    low = name.lower()
    for p in folder.iterdir():
        if p.name.lower() == low:
            return p
    return None


def _same(a: Path, b: Path) -> bool:
    try:
        return b.exists() and a.stat().st_size == b.stat().st_size and a.read_bytes() == b.read_bytes()
    except OSError:
        return False


def import_loose(demo: Path, assets_root: Path, overwrite: bool = False) -> dict:
    """Copia fontes e English.stf da pasta da demo. Devolve {'copied': [...], 'skipped': [...], 'missing': [...]}."""
    demo = Path(demo)
    assets_root = Path(assets_root)
    if not demo.is_dir():
        raise FileNotFoundError(f"pasta da demo nao existe: {demo}")
    retail = assets_root / "RETAIL"
    out = {"copied": [], "skipped": [], "missing": []}

    def copy(src: Path, dst: Path) -> None:
        if dst.exists() and not overwrite and _same(src, dst):
            out["skipped"].append(str(dst))
            return
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        out["copied"].append(str(dst))

    fonts = _find_child(demo, "Fonts")
    nfonts = 0
    if fonts is None:
        out["missing"].append(str(demo / "Fonts"))
    else:
        for p in sorted(fonts.iterdir()):
            if p.is_file() and (p.suffix.lower() in FONT_EXTS or p.name.lower() in FONT_EXTRAS):
                copy(p, retail / "Fonts" / p.name)
                nfonts += p.suffix.lower() in FONT_EXTS
        if nfonts == 0:
            out["missing"].append(str(fonts / "*.ttf"))

    stf = _find_child(demo, "English.stf")
    if stf is None or not stf.is_file():
        out["missing"].append(str(demo / "English.stf"))
    else:
        copy(stf, retail / "English.stf")
        copy(stf, assets_root / "CONVERTED" / "scripts" / "English.stf")
    out["fonts"] = nfonts
    return out
