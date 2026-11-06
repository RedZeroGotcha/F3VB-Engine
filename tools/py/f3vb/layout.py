"""Layout das pastas de dados do jogo (v12).

Tudo que sai do jogo do jogador fica em UMA pasta dentro do projeto, `original_assets/`:

  original_assets/
    RETAIL/        arquivos ORIGINAIS extraidos dos .grp, byte a byte: <Pack>/<Tipo>/<Nome>.<ext>  (b3d, gr2, tga, amx, ...)
    CONVERTED/     arquivos CONVERTIDOS pelas nossas ferramentas: png/, glb/, f3m/models/, f3m/skeletons/
    index.csv      pack, numero, tipo, nome, caminho (relativo a RETAIL/), tamanho
    EXTRACAO_RELATORIO.txt
    LEIA-ME.txt    aviso (conteudo do jogo, nao publicar)
    .gitignore     "*" (impede que qualquer coisa daqui seja versionada)

Cada jogador extrai o PROPRIO jogo. Nada disso entra no projeto distribuido (ver scripts/verificar_sem_assets.py).
CONVERTED pode ser refeita a qualquer momento a partir de RETAIL (`convert-retail`), sem precisar dos .grp.
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ASSETS_DIRNAME = "original_assets"
RETAIL = "RETAIL"
CONVERTED = "CONVERTED"
# Sobe quando o conversor muda geometria/convencoes dos arquivos convertidos (precisa refazer CONVERTED):
#   11 -> 12: escala da malha B3D -> unidades do esqueleto (1/10.24) e compensacao do posicionamento inicial do GR2.
#   12 -> 13: ossos do B3D casados com as juntas por sufixo ("L Clavicle" -> "L Clavicle_1"); animacoes de humanos pela familia (HumM_/HumF_).
CONVERTER_VERSION = 19   # v19: lote inicial do .8tre com material/pagina/textura (cabecalho de 12 bytes do IDXS); .int decodificado

_README = """CONTEUDO DO JOGO - NAO PUBLICAR, NAO VERSIONAR, NAO COMPARTILHAR.

Esta pasta guarda arquivos extraidos da SUA copia do jogo (demo tecnica "Van Buren").
  RETAIL/     originais extraidos (b3d, gr2, tga, amx, ...)
  CONVERTED/  convertidos pelo F3VB (f3m, png, glb)
Para refazer so os convertidos (sem os .grp):  py -3 -m f3vb convert-retail --png --f3m
Para apagar tudo, basta apagar a pasta. O projeto F3VB (codigo e docs) nao depende de nada daqui para compilar.
"""


def project_root() -> Path:
    """Raiz do projeto f3vb (a pasta que tem tools/, viewer/, docs/)."""
    return Path(__file__).resolve().parents[3]


def default_assets_dir() -> Path:
    env = os.environ.get("F3VB_ASSETS_DIR")
    return Path(env) if env else project_root() / ASSETS_DIRNAME


class AssetsLayout:
    def __init__(self, root=None):
        self.root = Path(root) if root else default_assets_dir()
        self.retail = self.root / RETAIL
        self.converted = self.root / CONVERTED

    def ensure(self) -> "AssetsLayout":
        self.retail.mkdir(parents=True, exist_ok=True)
        self.converted.mkdir(parents=True, exist_ok=True)
        gi = self.root / ".gitignore"
        if not gi.exists():
            gi.write_text("*\n", encoding="utf-8")
        rd = self.root / "LEIA-ME.txt"
        if not rd.exists():
            rd.write_text(_README, encoding="utf-8")
        return self

    @property
    def index_csv(self) -> Path:
        return self.root / "index.csv"

    @property
    def report(self) -> Path:
        return self.root / "EXTRACAO_RELATORIO.txt"

    @property
    def converter_file(self) -> Path:
        return self.converted / "CONVERTER_VERSION.json"

    def write_converter_info(self, extra: dict | None = None) -> None:
        from .export import DEFAULT_MESH_SCALE
        info = {"converter_version": CONVERTER_VERSION, "mesh_scale": DEFAULT_MESH_SCALE}
        info.update(extra or {})
        self.converter_file.write_text(json.dumps(info, indent=2), encoding="utf-8")

    def converter_version(self) -> int | None:
        try:
            return int(json.loads(self.converter_file.read_text(encoding="utf-8")).get("converter_version"))
        except (OSError, ValueError, TypeError):
            return None

    def converted_is_stale(self) -> bool:
        """True se CONVERTED existe mas foi gerada por um conversor mais antigo (ou sem registro, como no v11)."""
        has_files = self.converted.exists() and any(self.converted.rglob("*.f3m"))
        return has_files and (self.converter_version() or 0) < CONVERTER_VERSION
