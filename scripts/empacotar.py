"""Empacota o projeto em um .zip pronto para enviar (sem assets, sem builds), apos regenerar os .md de codigo.

Uso (na raiz do projeto):  py -3 scripts\\empacotar.py v12
Gera ..\\f3vb-starter-v12.zip (ao lado da pasta do projeto). Aborta se achar dados do jogo (verificar_sem_assets.py).
"""
from __future__ import annotations

import subprocess
import sys
import zipfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import verificar_sem_assets as vsa  # noqa: E402

PULAR = {".git", "__pycache__", "build", "original_assets", "private", "logs"}


def main() -> int:
    versao = sys.argv[1] if len(sys.argv) > 1 else "vX"
    subprocess.run([sys.executable, str(RAIZ / "scripts" / "gerar_codigo_md.py")], check=True)
    achados = vsa.varrer()
    if achados:
        print("Abortado: dados do jogo no projeto:")
        for a in achados:
            print("  -", a)
        return 1
    destino = RAIZ.parent / f"f3vb-starter-{versao}.zip"
    n = 0
    with zipfile.ZipFile(destino, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(RAIZ.rglob("*")):
            rel = p.relative_to(RAIZ.parent)
            if not p.is_file() or any(x in PULAR for x in p.relative_to(RAIZ).parts):
                continue
            z.write(p, rel.as_posix())
            n += 1
    print(f"{destino}  ({n} arquivos)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
