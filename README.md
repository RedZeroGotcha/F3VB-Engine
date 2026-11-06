# F3VB Engine: kit do projeto (v21): leitores, extrator, conversor, visualizadores e o jogo que lê a demo original direto

Recriar a demo técnica do Van Buren (Jefferson Engine) numa engine nova de 64 bits, **a partir da cópia que cada pessoa já tem**.
Nada do jogo é distribuído aqui: os dados extraídos ficam em `original_assets/` (fora do projeto, ignorado pelo git).

## Comece por aqui
| Quero... | Leia |
|---|---|
| **Jogar: menu, criação de personagem, Junktown, diálogos (`F3VB.exe`)** | **`docs/JOGO.md`** |
| Entender como tudo se encaixa | `docs/COMO_TUDO_FUNCIONA.md` |
| Rodar (comandos prontos para colar) | `COMANDOS.md` |
| Extrair a demo e organizar os arquivos | `docs/ORIGINAL_ASSETS.md` |
| Ver modelos/animações, usar o log e a análise | `docs/VISUALIZADOR.md` |
| Saber o que já foi medido na demo | `docs/ACHADOS_DEMO.md` |
| Saber para onde o projeto vai | `docs/PARA_ONDE_VAMOS.md` |
| **Conferir licenças e direitos antes de usar algo de terceiros** | `docs/LICENCAS_E_DIREITOS.md`, `CLEANROOM.md` |
| O histórico de versões | `docs/CHANGELOG.md` |
| A segunda engine (RPGGL) e o plano do v13 | `docs/RPGGL_ANALISE.md`, `engine_test/README.md` |
| Formatos de mapa (`.map`), grade (`.rle`), geometria (`.8tre`) e telas (`.int`) | `docs/specs/map.md`, `docs/specs/rle.md`, `docs/specs/tre.md`, `docs/specs/int.md` |
| Abrir um mapa em 3D (`F3VBMapViewer.exe`) | `docs/MAPVIEWER.md` |
| Comparação honesta com o OpenJE | `docs/COMPARACAO_OPENJE.md` |
`CODIGO_COMPLETO.md` e `CODIGO_THIRD_PARTY.md` trazem todo o código em texto (`py -3 scripts\gerar_codigo_md.py`).

## Mapa das pastas
```
tools/py/f3vb/     leitores e conversores em Python (grp, rht, b3d, gr2, f3m, glb, extrator, layout, VM de diálogo...)
tools/py/tests/    137 testes (dados sintéticos; compilam e rodam o núcleo C++ se houver g++)
engine_test/       RPGGL v0.2 (segunda engine de teste do autor, inalterada) com teste headless
viewer/            F3VB (o jogo), F3VBViewer, F3VBMapViewer, F3VBUiPreview, F3VBScript, f3v_analyze, núcleo portátil e testes
src/               VM de scripts AMX (código próprio) e base C++ dos formatos
scripts/           gerar_codigo_md, verificar_sem_assets, empacotar, rodar_tudo
third_party/       só o parser GR2 do blendergranny (MIT), com LICENSE e UPSTREAM.md
docs/              documentação (specs/ tem as specs de formato)
original_assets/   (criada pelo extrator; NUNCA no projeto) RETAIL = originais, CONVERTED = convertidos
private/           (ignorada) saídas de análise
```

## Início rápido (Windows)
```powershell
# 1) Extrair a demo (uma vez) e converter. Destino: <projeto>\original_assets (ou a pasta que você já usa)
cd "F:\F3VB Engine\M1\f3vb-starter-v21\f3vb\tools\py"
py -3 -m f3vb extract-all "F:\F3VB Engine\Demo\Demo" --png --f3m --maps
# ja tem original_assets de uma versao anterior? basta reconverter e trazer fontes/textos:
py -3 -m f3vb convert-retail --maps
py -3 -m f3vb import-loose "F:\F3VB Engine\Demo\Demo"
```
```bat
:: 2) Compilar (Developer Command Prompt do Visual Studio, cmd) e jogar
cd /d "F:\F3VB Engine\M1\f3vb-starter-v21\f3vb\viewer"
build.bat
jogar.bat
```
O log fica em `viewer\build\logs\f3vb_log_*.txt`. Os visualizadores continuam: `build\F3VBMapViewer.exe` (mapas) e `build\F3VBViewer.exe <modelo.f3m>` (modelos e animações).

## Requisitos
Python 3.10+ (sem dependências externas); para o visualizador: Visual Studio (C++), CMake. Opcional: g++ (testes C++ no Linux), ImHex, Ghidra.

## Regras de ouro
1. **Dados do jogo nunca entram no projeto** (`scripts\verificar_sem_assets.py` confere; `scripts\empacotar.py` roda a checagem sozinho).
2. **Tudo de terceiros é registrado** em `docs/LICENCAS_E_DIREITOS.md` antes de entrar.
3. **Cada versão atualiza a documentação**: `docs/CHANGELOG.md`, este README e os docs do que mudou.
4. Quando algo parecer errado no visualizador, **mande o log** (`logs\f3viewer_log_*.txt`) ou o relatório do `f3v_analyze`.

## Estado do projeto (resumo; detalhes em `docs/ACHADOS_DEMO.md`)
- `.grp`/`.rht`: 7.044 recursos resolvidos. GR2: 547 arquivos lidos sem a DLL. AMX: 119/119 decodificados e VM roda scripts reais.
- B3D: 1.472 de 3.915 convertem para F3M; o resto (a "cauda do B3D"), terreno dos tiles, Effects, `.rle`, `.stf`/`.mve` e mapas `8TRE`/`EMAP` seguem em aberto.
- v12: animações quebradas diagnosticadas e corrigidas no conversor (escala da malha ~10,24× e posicionamento inicial do GR2); confirmado no Windows pelo autor (cão e Eyebot animam bem).
- **v21: F3VB Engine, gameplay de verdade.**
  - Jogador andando pelo chão; **combate em tempo real**; todas as animações (parado, andar, correr, ataque, dano, morte).
  - Personagens no chão, com sombra.
  - Vagão do menu chegando como no original.
  - Cores dos personagens corrigidas na NVIDIA; câmera sem limbo.
  - Ícone com a logo; `--autoteste` e `F3VBCheck --tudo`.

  Veja `docs/CHANGELOG.md`.
- v20: o jogo lê a demo original direto: `.rht` + `.grp`, com seletor de pasta na 1ª vez gravado no `F3VB.ini`. Tem:
  - vídeos da Interplay/Black Isle com som (Bink da própria demo);
  - menu 3D com os personagens do original;
  - criação de personagem com boneco 3D e aparência de verdade;
  - Junktown com o jogador, NPCs e objetos em 3D;
  - câmera, barra de ações e fórmulas medidas nos prints do original;
  - salvar/carregar.

  Veja `docs/JOGO.md` e `docs/CHANGELOG.md`.
- v19: o jogo começa a existir. `F3VB.exe` abre com a **interface original** (`.int` decifrado, TrueType e TGA/BMP próprios), **criação de personagem** funcional, Junktown com câmera livre e **diálogos por clique**; manchas brancas do mapa corrigidas (cabeçalho do 1º lote do `.8tre`); diálogos com ids 1-based e todas as respostas; `import-loose` e `int-info`. Veja `docs/JOGO.md`.
- v18: **texturas no mapa** (`X`: difusa e lightmap; `.f3map` v3 com UVs, `export-textures`), **bug da VM corrigido** (scripts compilados com depuração), diálogo achado por nome (`V`/`K`), levantamento com natives chamadas e base da interface (`ui-info`). Veja `docs/CHANGELOG.md`.
- v17: **VM ligada ao mapa**: `V` no F3VBMapViewer roda o diálogo do script da entidade selecionada, `B` levanta os scripts do mapa; novo `F3VBScript` (linha de comando) e `export-scripts`. Texturas ficam para o v18. Veja `docs/VM_E_DIALOGOS.md`.
- v16: **lotes de índices do `.8tre`** (cada triângulo sabe material, página de lightmap e textura difusa; acabam os "espinhos" do Mainmenu), `.f3map` v2 e **tecla `M` no F3VBMapViewer** (colorir por material/lightmap/textura, com legenda). Novo comando `tre-check`. Aguarda o seu teste no Windows. Próximo (v17): desenhar com as texturas.
- v15: **geometria 3D dos mapas (`.8tre`) decodificada** e novo executável **`F3VBMapViewer.exe`** (escolher um mapa e ver em 3D; aguarda o seu teste no Windows). `convert-retail --maps` gera os `.f3map`. Próximo: ligar triângulo a material/lightmap.
- v14: **mapas e grades decodificados** (37 `.map` e 38 `.rle` reais; `map-info`, `rle-info`, `convert-retail --maps` geram relatório, JSON e imagem de visão superior). Próximo: `.8tre` (geometria 3D dos mapas).
- v13 (marco 13.1): humanos corrigidos (nomes de ossos e animações por família), RPGGL no repositório com teste, `b3dmat` e `pack-sample`. Próximos: motor (`src/engine`), Scene Viewer, ragdoll no esqueleto, mapas.

## Comandos de análise de formatos (a partir de `tools/py`)
```bash
python -m f3vb inventory | probe | grp --list | rht | catalog | report | resumo | sample | gr2 | inspect | extract | b3dfail | amxdis | stf | export-glb | export-f3m | export-list
python -m f3vb extract-all <demo> [--out DIR] [--png] [--glb] [--f3m] [--plan-only]      # demo -> original_assets
python -m f3vb convert-retail [--out DIR] [--png] [--glb] [--f3m] [--maps]               # RETAIL -> CONVERTED (sem a demo)
python -m f3vb map-info NOME [--png mapa.png] [--json mapa.json] [--out relatorio.txt]    # mapa decodificado (.map + .rle)
python -m f3vb rle-info NOME [--png grade.png]                                            # grade de tiles
python -m f3vb tre-info NOME                                                              # geometria 3D (.8tre) + lotes
python -m f3vb tre-check [PASTA]                                                          # confere o parser de lotes em todos os .8tre
python -m f3vb int-info NOME                                                              # árvore de widgets de uma tela .int (v19)
python -m f3vb import-loose <demo>                                                        # Fonts\*.ttf e English.stf -> original_assets (v19)
python -m f3vb map-info NOME --f3map mapa.f3map                                           # mapa completo para o F3VBMapViewer
python -m unittest discover -s tests -t .
```
Veja `COMANDOS.md` para a lista completa com exemplos. Leia `CLEANROOM.md` antes de commitar qualquer coisa.
