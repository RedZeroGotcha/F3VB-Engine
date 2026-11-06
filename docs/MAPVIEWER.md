# F3VB Map Viewer (v19): escolher um mapa e abrir em 3D

Segundo executável do visualizador (`F3VBMapViewer.exe`, ao lado do `F3VBViewer.exe`). **Confirmado por você no Windows (v15):** abre o Mainmenu (GL 4.6, RTX 3050, 24.309 triângulos, 3 ms). **O v16 (modos de cor e legenda) só foi compilado contra *stubs* aqui; o teste de execução é o seu: me mande o log e prints.**

## O que ele mostra
Geometria 3D do `.8tre` (cor de vértice + iluminação simples), a **planta** do `.rle` como textura no chão, **entidades** (caixas por tipo: criatura vermelha, arma laranja, munição amarela, porta azul, container amarelo, item rosa, objeto verde), **grafo de navegação** (verde), **gatilhos** (ciano = viagem para outro mapa, magenta = script, azul = região, cinza = vínculo), **caminhos** de patrulha (azul claro), sons (roxo), marcadores (branco), efeitos (laranja), pontos de entrada (pilares amarelos) e água (azul). **Sem texturas ainda** (v17), mas **desde o v16 cada triângulo sabe material, página de lightmap e textura difusa** e a tecla `M` colore por isso (abaixo).

## Passo a passo
```powershell
# 1) Gerar os mapas (a partir do que você já extraiu; precisa dos .map, .rle e .8tre em RETAIL)
cd "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\tools\py"
$env:PYTHONUTF8 = "1"
$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
py -3 -m f3vb convert-retail --maps
# saída: ...\original_assets\CONVERTED\maps\<mapa>.f3map (+ .png, .txt, .json e maps_summary.txt)
# (v16) RECONVERTA: os .f3map do v15 são versão 1 (sem material/página/textura); o viewer avisa se abrir um deles
# conferir o parser de lotes nos .8tre (esperado: nos=5925 nao_fechados=0 fora_bbox=0 triangulos=1928117):
py -3 -m f3vb tre-check "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets\RETAIL\Maps"
```
```bat
:: 2) Compilar (Developer Command Prompt for VS, cmd): gera F3VBViewer.exe, F3VBMapViewer.exe, f3v_analyze.exe e os testes
cd /d "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\viewer"
build.bat
testar_nucleo.bat
```
```bat
:: 3) Abrir: sem argumento ele abre o dialogo "Escolha um mapa" (pasta padrao: ...\original_assets\CONVERTED\maps)
build\F3VBMapViewer.exe
build\F3VBMapViewer.exe "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets\CONVERTED\maps\00_04_Tutorial_Vault.f3map"
```
Se o `.exe` ficar em `build\Release`, use esse caminho (veja `type build\exe_path.txt`). A pasta padrão vem de `F3VB_ASSETS_DIR` (se definida) ou de `..\..\original_assets` relativo ao `.exe`.

## Teclas
`O` abrir | `PgDn`/`PgUp` próximo/anterior mapa da pasta | `G` geometria | `R` planta | `E` entidades | `N` navegação | `T` gatilhos | `P` caminhos | `S` sons/marcadores/efeitos/entradas | `A` água | `W` wireframe | `L` luz | `C` cores de vértice | **`M` modo de cor da geometria (cor de vértice → material → página de lightmap → textura difusa)** | `Tab`/`Shift+Tab` seleciona entidade (o HUD mostra protótipo, instância, nome, script e posição) | `Z` foca nela | `Home` câmera inicial | `2` vista de cima | `1` vista 3D | `F2` relatório completo no log | `F1` ajuda | `Esc` sair. Mouse: esquerdo gira, direito/meio move, roda dá zoom.

## v19: cor certa e fim das manchas brancas
- **Fórmula nova** (a mesma do jogo, `docs/JOGO.md`): sem luz do OpenGL (a luz da demo é pré-calculada); `(difusa ou branco) × cor de vértice × 2`, depois `× lightmap × 2`. Antes, materiais `BASE_2X`/`NO_TEXTURE` ficavam pretos ou estourados.
- Quando acha texturas, abre direto em **difusa + lightmap** e **sem a planta no chão** (a planta por cima da geometria era parte das manchas). `X` (texturas) e `R` (planta) continuam alternando.
- O `.8tre` agora é lido com a gramática completa (cabeçalho do 1º lote nos 12 bytes iniciais do `IDXS`): se o log mostrar `AVISO: N triangulos sem material/pagina/textura`, o mapa foi convertido com um kit antigo: rode `convert-retail --maps`.
- Diálogos (`V`): ids 1-based e as respostas mostram a ação e o destino (veja `docs/VM_E_DIALOGOS.md`).

## Texturas (v18)
`X` alterna: **sem textura** (modos de cor `M`) → **difusa** → **difusa + lightmap** (multitextura; lightmap ×2). Precisa de `.f3map` v3 (UVs) e de `CONVERTED\\textures\\*.f3tex`: `py -3 -m f3vb convert-retail --maps` e `py -3 -m f3vb export-textures`. O log ao abrir o mapa diz quantas difusas/lightmaps foram carregadas e quais faltam (`textures_summary.txt` tem a lista completa). **Suposições:** `ALPHABLEND` = mistura por alfa (desenhado por último); nome com `IMAGE_ALPHA` = recorte de alfa 0,5; lightmap = textura `.ctx` pelo mesmo nome. Se a aparência estiver errada em algum modo, mande prints dos 3 modos do mesmo ângulo.

## Scripts e diálogos (v17, ajustado no v18)
`V` abre o diálogo da entidade selecionada (`Tab`) rodando o script dela na VM (se ele não tem diálogo, procura um por nome; **`K`** troca para o próximo script de diálogo); `1`-`9` respondem; `B` levanta todos os scripts do mapa. Precisa de `CONVERTED\\scripts` (`py -3 -m f3vb export-scripts`) e, para ver os textos, do `.stf` ali ao lado. Tudo em `docs/VM_E_DIALOGOS.md`.

## Modos de cor e legenda (v16)
`M` alterna o que colore a geometria: **cor de vértice** (padrão), **material** (um matiz por material do `MATD`), **página de lightmap** (uma cor por página `.ctx`; cinza = sem lightmap) e **textura difusa** (uma cor por `.tga`; cinza = sem textura). Embaixo à esquerda aparece a **legenda** (as 14 maiores): amostra de cor, nome e número de triângulos; no modo material o nome vem com mistura e superfície (ex.: `#2 SK_BASE_LM_2X (OPAQUE, CONCRETE)`). **O que observar:** (1) os **espinhos sumiram** no Mainmenu? (2) no modo material, as superfícies fazem sentido (piso, parede, vidro...)? (3) no modo página de lightmap, as cores formam **remendos contíguos** (paredes vizinhas na mesma página)? Se algo parecer aleatório, mande prints dos três modos e o log.

## O log
`logs\f3mapviewer_log_MM-DD-YY_hh-mm-ss.txt` (na pasta do `.exe`; mesmo formato do outro visualizador): versão, GL (fornecedor, renderizador, versão), pasta padrão, **cada mapa aberto** com contagens (triângulos, entidades, nós de navegação, gatilhos...), a caixa do mapa, a linha **"espinhos"** (triângulos com uma aresta maior que 20% da diagonal do mapa; deve ser ~0, só pisos enormes legítimos) e as **tabelas de materiais, páginas de lightmap e texturas com triângulos por item**, **quantos pontos caem em células ocupadas da planta** (deve dar ~94%), os gatilhos de viagem, cada comando de tecla, `F2` (lista todas as entidades com marca "FORA da planta" quando a posição não cai em célula ocupada) e uma linha de estado a cada 30 s. Se faltar "sessão encerrada normalmente", o programa caiu.

## Formato `.f3map`
Container de chunks (`F3MP` + versão **2** (o leitor aceita 1 e 2) + `INFO`, `GRID`, `GEOM`, `MATL`, `TEXN`, `ENTS`, `NAVN`, `ENTR`, `TRIG`, `PATH`, `SOND`, `MARK`, `EFFT`, `WATR`, `LGHT`); na v2 cada lista de triângulos do `GEOM` leva material, página e textura; layout em `tools/py/f3vb/mapexport.py`. É **derivado de conteúdo do jogo**: fica em `original_assets\CONVERTED\maps` e nunca vai para o projeto.

## O que foi testado
- **v16 (sintético):** `.f3map` v1 e v2 lidos em C++ (listas com material/página/textura, tabela de materiais, nomes `.tga`/`.ctx`), fuzz em 3 arquivos, sintaxe do viewer com stubs (0 avisos no g++ do sandbox). Execução no Windows: a sua.
- **Leitor C++ do `.f3map`:** fixture sintético (Python → C++), 4.000 mutações aleatórias sem quebrar, e os **34 `.f3map` reais** (1.659.759 triângulos; 186 de 197 pontos em células ocupadas, igual ao Python).
- **Janela, OpenGL, dialogo e teclas:** **não executado** (só sintaxe com stubs). Prováveis riscos para você observar: o dialogo abrir na pasta certa, desempenho em `Spelunking` (≈300 mil triângulos em vertex arrays, sem otimização), cores muito escuras onde a cor de vértice é cinza 50%, câmera inicial longe/perto demais (`Home` reseta).

## Limitações
Sem texturas e sem lightmaps na tela (v17); sem colisão e sem scripts (a VM ainda não está ligada ao mapa); sem personagens (as caixas são marcadores: o protótipo `.CRT` ainda não foi decodificado para chegar ao modelo); a água é desenhada como quadrados de 10 × 10 unidades.
