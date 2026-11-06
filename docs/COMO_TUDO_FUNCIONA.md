# Como tudo isso funciona (v18), em linguagem simples e com os detalhes técnicos

## A ideia em uma frase
O jogo original é um conjunto de **caixas lacradas** (`.grp`). Construímos (1) **chaves** que abrem as caixas, (2) um **tradutor** que converte
o conteúdo para formatos modernos e (3) um **motor novo** que usa esse conteúdo e **roda os scripts originais**. Nada do jogo vai para o projeto:
cada pessoa extrai a própria cópia para `original_assets/`.

## O caminho dos dados (do disco até a tela)
```
 demo (.grp + resource.rht + F3.exe)           <- fica FORA do projeto, é do jogador
        |  py -3 -m f3vb extract-all ...        (1x; precisa da demo)
        v
 original_assets/RETAIL/<Pack>/<Tipo>/<Nome>.<ext>      b3d, gr2, skl, tga, amx ... byte a byte   + index.csv
        |  py -3 -m f3vb convert-retail ...     (quantas vezes quiser; NÃO precisa da demo)
        v
 original_assets/CONVERTED/{f3m,png,glb}/...            formatos abertos (+ CONVERTER_VERSION.json)
        |  viewer\build\F3VBViewer.exe modelo.f3m   (ou f3v_analyze)
        v
 janela OpenGL + logs\f3viewer_log_*.txt                 ver, animar, medir, registrar
```
O motor futuro (v13+) lê de `RETAIL` (scripts, mapas) e de `CONVERTED` (modelos, texturas), nunca dos `.grp` diretamente.

## As camadas
| Camada | Para que serve | Onde | Estado (medido ou testado) |
|---|---|---|---|
| **1. Leitura** | Abrir `.grp`/`.rht`; ler B3D (modelos), GR2 (esqueleto/animação), TGA/BMP, AMX (scripts) | `tools/py/f3vb/` (Python), `src/formats/` (C++) | `.grp`/`.rht`: 7.044 recursos resolvidos. GR2: 547 arquivos lidos sem a DLL. AMX: 119/119. B3D: 1.472 de 3.915 convertem para F3M (o resto é a "cauda do B3D", em aberto) |
| **2. Conversão** | B3D+GR2+TGA → F3M (visualizador), GLB (Blender), PNG | `export.py`, `gr2export.py`, `f3m.py`, `extractor.py` | Cão e Eyebot validados numericamente (abaixo) |
| **3. Visualização/diagnóstico** | Ver, animar, medir e registrar em log | `viewer/` (C++: `f3m`, `scene`, `analyze`, `log`, `viewer_main`) | Compila e abre no Windows (v11 confirmado); v12 aguarda seu teste |
| **4. Motor** | Mapa, personagens, scripts, física, áudio | `src/amx/` (VM de scripts), `src/host/` | VM roda scripts reais; mapa e física: v13+ |

## As convenções dos dados (o que descobrimos na prática)
1. **Mão-esquerda → mão-direita:** o conversor inverte Z (posições, quaternions, matrizes). Hipótese herdada; `--no-convert` desliga.
2. **Escala da malha × esqueleto (descoberta do v12):** a geometria do B3D vem em unidades **~10,24× maiores** que o esqueleto do GR2 (medido pelo ajuste de
   vértices de peso ~1 ao segmento do osso dono: cão 10,15–10,35, Eyebot 10,3–10,5; hipótese `1024/100`). Na **pose de bind** isso não aparece
   (`mundo × ibm = identidade` para qualquer escala), mas **qualquer animação** gira os vértices em torno de pivôs errados e estica a malha
   (arestas 3,7–8,7× no cão). O conversor v12 multiplica a geometria por `DEFAULT_MESH_SCALE = 100/1024`; o esqueleto e as animações ficam como estão.
3. **Posicionamento inicial do GR2:** no Eyebot, `mundo_do_osso × InverseWorld4x4` é a MESMA translação (0,0117; 0; 0,0087) em todas as 8 juntas: o `InverseWorld`
   foi calculado com um deslocamento global que a hierarquia de TRS locais não inclui (no cão é zero). O conversor aplica o oposto às raízes (e às chaves de
   translação das raízes), e a pose de bind skinada passa a reproduzir a malha com erro ~1e-7. É o "deslocamento de ~0,012" que ficou sem explicação até o v11.
4. **Skinning:** `v' = Σ peso × (mundo_da_junta × ibm) × v`, com junta casada por **nome** de osso. Vértices sem influência ficam parados no bind.
5. **Nomes de osso (v13):** o exportador de origem do GR2 acrescenta `_1` quando há nomes repetidos; o B3D guarda o nome original. Medido em HumFSkl: `L Clavicle` no B3D, `L Clavicle_1` no GR2. O casamento tenta, em ordem, nome exato, `nome_1`..`nome_9`, sem o `_N` e sem diferenciar maiúsculas (conversor: `resolve_bone_aliases`; visualizador: `Scene::find_joint`). Osso sem junta mesmo assim: aquela parte do peso fica no bind.
6. **Animações de humanos (v13):** compartilhadas por família, `HumM_*` e `HumF_*`; animais têm as suas (`CR_Dog_*`). `anim_prefixes()` no extrator.
7. **Animação:** canais por osso (T/R/S) amostrados e interpolados (lerp/slerp); canais cujo osso não existe no esqueleto são ignorados e contados no aviso.

## Como sabemos que está certo (e o que ainda não)
- **Pose de bind skinada == malha** (erro ~1e-7) para cão e Eyebot: prova que `ibm` e a hierarquia são coerentes.
- **Esticamento das arestas** durante a animação (razão pose/bind): no cão caiu de p95 3,7–8,7× para 1,25–1,7×. Uma malha rígida e bem pesada mantém ~1×.
- **C++ == Python de referência** no skinning (≤ 5e-7). Isso prova que as duas contas são iguais, **não** que a convenção esteja certa: por isso a análise por
  esticamento e por ajuste de escala existe.
- **Ainda não provado:** o valor exato da escala (10,24 é hipótese com 2 modelos), pesos e retarget dos corpos humanos, partes do B3D (terreno, Effects), mapas.

## Diagnóstico: o log e a análise (v12)
O visualizador grava `logs\f3viewer_log_MM-DD-YY_hh-mm-ss.txt` com tudo (teclas, carregamentos, `P`, `K`, escala) e uma análise completa do modelo; `f3v_analyze`
gera o mesmo relatório sem janela. Detalhes e como ler: `docs/VISUALIZADOR.md`. **Quando algo parecer errado: mande o log (ou o relatório) em vez de só descrever.**

## Mapas (v14)
Um mapa são **três arquivos ligados pelos nomes** que estão dentro do `.map`: o **`.map`** (`MAP_Map`: entidades, entradas, navegação, gatilhos, caminhos, sons), o **`.rle`** (`RLE`: a grade de tiles, a planta de pisos) e o **`.8tre`** (`TRE_Octree`: a geometria 3D, em octree), mais a imagem de mapa de área `<Mapa>_AM.tga` e as páginas de lightmap `<Mapa>_N.tga`. Todos vivem no **mesmo espaço de mundo**: `célula do .rle = (posição − origem) / 0,5`. `convert-retail --maps` junta tudo e grava, por mapa, relatório, JSON, imagem de visão superior e o **`.f3map`** que o `F3VBMapViewer.exe` abre. Formatos: `docs/specs/map.md`, `rle.md` e `tre.md`; uso: `docs/MAPVIEWER.md`.

## Geometria dos mapas: lotes de índices e `.f3map` v2 (v16)
**O problema.** A geometria de um mapa (`.8tre`) guarda os vértices em *blocos* e, em cada nó da octree, uma lista de índices de triângulos. No v15 o programa lia essa lista como "só números de vértice", e no Mainmenu apareciam triângulos gigantes (os *espinhos*).
**A descoberta.** A lista não é só de números: no meio dela há **cabeçalhos** que dizem "a partir daqui, o material é X", "a página de lightmap é Y" e "a textura difusa é Z". Lendo esses cabeçalhos como índices o programa inventava triângulos que ligavam vértices distantes. O leitor novo (`tre.py::split_batches`) separa a lista em **lotes** e cada lote vira um `TriBatch(material, página, textura, índices)`.
**A dificuldade.** Os cabeçalhos usam os dois bits mais altos de um número de 16 bits, e blocos grandes (até 63.887 vértices) também usam esses bits em índices *de verdade*. Para não confundir, o leitor só aceita um cabeçalho se os valores fazem sentido (material existe na tabela, página existe na lista de lightmaps...), se as contagens internas fecham **exatamente** e se o total bate com `3 × len(FLGS)`. Primeiro exige que os números de enchimento sejam 0 (modo estrito); se nada fechar, aceita qualquer enchimento (modo frouxo).
**O que segue pelo caminho.** `convert-retail --maps` grava um `.f3map` **v2**: cada lista de triângulos leva (bloco, material, página, textura) e há duas tabelas novas, `MATL` (materiais: nome, cor, mistura, superfície) e `TEXN` (nomes das texturas `.tga` e `.ctx`). O `F3VBMapViewer` (tecla `M`) pinta por material, página de lightmap ou textura e mostra a legenda; o log lista as tabelas e conta os "espinhos" restantes.
**Como conferir.** `py -3 -m f3vb tre-check PASTA` roda o parser em todos os `.8tre` e imprime nós, nós que não fecham, índices fora da caixa e triângulos (esperado: 5925 / 0 / 0 / 1.928.117). **Ainda não há textura na tela**: isso é o v17.

## Scripts, VM e diálogos ligados ao mapa (v17)
**A ideia.** Cada entidade do mapa (NPC, porta...) pode citar um *script* (`script` no `ENTS`). Esses scripts são programas Pawn/Small compilados (`.amx`) que a demo roda numa máquina virtual. A nossa VM (`amx_vm`) executa as instruções; mas um script só "faz" coisas chamando *natives*: funções que a **engine** fornece (`RegisterReply`, `FloatingSpeechText`, `Myself`...). A camada `GameHost` implementa algumas de verdade e deixa as outras só registrando a chamada.
**O elo com o mapa.** `convert-retail --maps` (ou `export-scripts`) copia os 119 `.amx` para `CONVERTED\scripts`. Ao abrir um mapa, o viewer lê os nomes de script das entidades, acha cada `.amx` pelo nome (sem pasta, sem `.amx`, sem maiúsculas) e mostra no log quantos foram achados.
**O diálogo (`V`).** Cria uma instância (VM própria da entidade), chama `OnStartDialogue`, e então age como a engine: chama a *função de texto* do nó para listar as respostas, esconde as que a *condição* barra, e ao escolher chama a *função de resultado*. Falas e respostas aparecem no painel e no log; os números de texto viram frases se o `.stf` estiver ao lado.
**O levantamento (`B`).** Roda todas as publics de cada script (como o `dialog_dump`) e lista erros e natives que ainda faltam; é o mapa do trabalho de implementar a VM de verdade.
**Honestidade.** A ordem exata em que a engine original chama essas funções ainda é dedução; por isso cada passo vai para o log e `docs/VM_E_DIALOGOS.md` lista o que é suposição.

## Texturas e a correção da VM (v18)
**Texturas.** Cada vértice do mapa tem duas coordenadas de textura: uma para a **imagem do material** (difusa) e outra para o **lightmap** (a luz "assada" do mapa). O `.f3map` v3 leva as duas. O `export-textures` lê os nomes das texturas citadas pelos mapas (`TEXN`), acha cada uma no RETAIL (recurso `TGA_Image` com o mesmo nome), decodifica e grava `.f3tex` (um formato cru: "F3TX", largura, altura, pixels RGBA) em `CONVERTED\textures`, para o viewer carregar sem decodificar TGA. O viewer sobe cada textura para a placa de vídeo com **mipmaps** (versões menores para objetos distantes) e desenha por lista de triângulos (cada lista já tem seu material, difusa e lightmap). Com a tecla `X`: só difusa (modulada pela cor do vértice e pela luz do OpenGL) ou difusa × lightmap × 2 (multitextura; a luz já vem no lightmap, então a luz do OpenGL é desligada). Materiais `ALPHABLEND` saem por último com mistura por alfa; nomes com `IMAGE_ALPHA` recortam pixels transparentes. Esses dois são palpites até vermos as telas.
**O bug da VM.** O compilador do jogo, nos scripts de objetos e criaturas, deixou no código o nome de cada variável e função (símbolos de depuração) como instruções especiais de tamanho variável. A VM medi-as como "uma célula" e executava o texto como código: tudo falhava. Agora ela lê o tamanho gravado na própria instrução e pula. Por isso os scripts de diálogo (compilados sem depuração) rodavam e os de objetos não.
**Diálogo de NPC.** O script que o mapa cita é o de comportamento; o diálogo está em outro. A engine original liga os dois de um jeito que ainda não conhecemos; o viewer tenta por semelhança de nome (`V`) ou deixa você folhear todos (`K`).
**Interface do jogo.** Ainda não existe: primeiro precisamos ler os layouts (`.int`), as fontes (`.fnt`) e saber quais natives de UI os scripts chamam. O `ui-info` e o levantamento (`B`) são o primeiro passo.

## A segunda engine (v13)
`engine_test/rpggl/` guarda o RPGGL v0.2 (mini-RPG do autor, OpenGL 1.x, sem terceiros). É referência de mecânicas (ragdoll XPBD, IA, veículo, saves, pool de threads, UI) e a base do visualizador de mapa. Análise módulo a módulo e marcos: `docs/RPGGL_ANALISE.md`.

## Testes
`cd tools\py` e `py -3 -m unittest discover -s tests -t .` (123 testes; compila e roda o núcleo C++ se houver `g++`). No Windows: `viewer\testar_nucleo.bat`.
Os testes usam **dados sintéticos**: não precisam de `original_assets/`.

## Rótulos de confiança
Cada afirmação nos documentos é **medida** (rodamos nos seus arquivos), **testada** (teste automático) ou **hipótese**. Quando um doc não disser, assuma hipótese.

## Limites honestos
Projeto grande (meses em meio período), dividido em marcos pequenos e testáveis. O v13 pretende abrir um mapa ou a intro/menu 3D numa segunda engine de teste.
