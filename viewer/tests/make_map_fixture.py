"""Gera um .f3map SINTETICO (sem dados do jogo) para os testes do leitor C++.  Uso: python make_map_fixture.py PASTA"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / "tools" / "py"
sys.path.insert(0, str(ROOT))
from f3vb.mapexport import build_f3map  # noqa: E402
from f3vb.mapfile import read_map  # noqa: E402
from f3vb.rle import read_rle  # noqa: E402
from f3vb.tre import read_tre  # noqa: E402
from tests.test_maps import make_map, make_rle  # noqa: E402
from tests.test_tre import group, make_tre, page, sub  # noqa: E402

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
(out / "teste.f3map").write_bytes(build_f3map("Teste", read_map(make_map()), read_rle(make_rle()), read_tre(make_tre())))
(out / "so_mapa.f3map").write_bytes(build_f3map("So_mapa", read_map(make_map()), None, None))
# v1 (formato do v15) e v2 com lotes: material, pagina de lightmap e textura difusa
(out / "v2.f3map").write_bytes(build_f3map("V2", read_map(make_map()), read_rle(make_rle()), read_tre(make_tre()), version=2))
(out / "v1.f3map").write_bytes(build_f3map("V1", read_map(make_map()), read_rle(make_rle()), read_tre(make_tre()), version=1))
u = [0, 1, 2, 2, 1, 3] + group(1, page(2, sub(2, [0, 1, 3]), sub(1, [3, 2, 0]))) + group(2, page(1, sub(0, [1, 2, 3])))
lotes = read_tre(make_tre(idx=u, ntris=5, nmat=3, textures=("a.tga", "b.tga", "lm_0.ctx", "lm_1.ctx")))
(out / "lotes.f3map").write_bytes(build_f3map("Lotes", read_map(make_map()), read_rle(make_rle()), lotes))
print(f"fixtures do mapa em {out}: 5 arquivos")
