"""Gera CODIGO_COMPLETO.md (e CODIGO_THIRD_PARTY.md) com todo o texto do projeto, para ler/enviar.

Uso (na raiz do projeto):  py -3 scripts\\gerar_codigo_md.py
"""
from __future__ import annotations

import re
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
EXT = {".py": "python", ".ps1": "powershell", ".md": "markdown", ".txt": "text", ".hpp": "cpp",
       ".cpp": "cpp", ".h": "cpp", ".pyw": "python", ".bat": "bat", ".toml": "toml", ".cfg": "ini"}
NOMES = {"CMakeLists.txt": "cmake", ".gitignore": "text", "LICENSE": "text"}
IGNORAR = {"private", "__pycache__", "build", ".git"}
SAIDAS = {"CODIGO_COMPLETO.md", "CODIGO_THIRD_PARTY.md"}


def lang(p: Path) -> str | None:
    return NOMES.get(p.name) or EXT.get(p.suffix.lower())


def cerca(texto: str) -> str:
    runs = [len(m.group(0)) for m in re.finditer(r"`+", texto)]
    return "`" * max(3, (max(runs) if runs else 0) + 1)


def coletar(base: Path, so_third_party: bool) -> list[Path]:
    arquivos = []
    for p in sorted(base.rglob("*")):
        rel = p.relative_to(RAIZ)
        if not p.is_file() or any(part in IGNORAR for part in rel.parts) or p.name in SAIDAS:
            continue
        in_tp = rel.parts[0] == "third_party"
        if in_tp != so_third_party or lang(p) is None:
            continue
        arquivos.append(p)
    return arquivos


def escrever(destino: Path, titulo: str, arquivos: list[Path]) -> None:
    linhas = [f"# {titulo}", "", f"{len(arquivos)} arquivos. Gerado por `scripts/gerar_codigo_md.py`.", "", "## Índice", ""]
    for p in arquivos:
        linhas.append(f"- `{p.relative_to(RAIZ).as_posix()}`")
    for p in arquivos:
        texto = p.read_text(encoding="utf-8", errors="replace")
        c = cerca(texto)
        linhas += ["", f"## {p.relative_to(RAIZ).as_posix()}", "", f"{c}{lang(p)}", texto.rstrip("\n"), c]
    destino.write_text("\n".join(linhas) + "\n", encoding="utf-8")
    print(f"{destino.name}: {len(arquivos)} arquivos, {destino.stat().st_size} bytes")


if __name__ == "__main__":
    escrever(RAIZ / "CODIGO_COMPLETO.md", "F3VB: código completo do projeto", coletar(RAIZ, False))
    escrever(RAIZ / "CODIGO_THIRD_PARTY.md",
             "F3VB: código de terceiros vendorizado (blendergranny, MIT)", coletar(RAIZ, True))
