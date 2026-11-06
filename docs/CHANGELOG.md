# Histórico de versões do kit

## v21 (2026-11-05): F3VB Engine
- **Nome e logo:** o executável agora é o **F3VB Engine**.
  - Título da janela "F3VB Engine v21".
  - Versão do arquivo e nome do produto no `F3VB.exe` (`res/f3vb.rc`).
  - **Ícone com a logo do projeto** (arte do autor, `res/f3vb.ico`, `docs/img/f3vb_engine_logo.png`).
- **Gameplay de verdade (combate em tempo real):**
  - Clique no chão: o jogador anda pelo caminho (A* no chão do mapa) com a animação de andar ou correr da demo. A câmera acompanha.
  - Tecla **A** (AttackMode do `F3.ini` original) ou clique na arma da barra liga o modo de ataque; clicar num personagem ataca. O jogador
    vai até ele, ataca no ritmo da animação, e o acerto sai pela perícia da ficha. O atacado revida, e os do mesmo tipo por perto também.
  - Números de dano e "Errou" flutuando, barra de vida dos feridos, nome sob o mouse (verde neutro, vermelho hostil).
  - Animações de dano e de morte; HEALTH de verdade na barra.
  - Quando o jogador morre: "Você morreu." e volta ao menu.
  - Os NPCs perseguem com caminho e respeitam o alcance da arma: os de minigun e pistola atiram de longe, o cachorro e a faca chegam perto.
- **Animações por papel** para todo personagem e criatura (parado, andar, correr, ataque, dano, morte), dos `.gr2` da demo, com cache:
  - todos os personagens de todos os mapas têm as 6 animações (Junktown 10/10, Spelunking 37/37, Vault 4/4, menu 2/2);
  - a parada repete; ataque, dano e morte param no último quadro.
- **Personagens no chão:** chão do mapa por raio contra os triângulos do `.8tre` (`ground.cpp`).
  - Medido: no Junktown os personagens estavam 1 unidade (~30 cm) acima do chão.
  - Agora todos ficam colados no chão, ao carregar e a cada passo, com **sombra redonda** sob cada um.
  - Passo bloqueado em degrau acima de 2 unidades.
- **Menu principal igual ao original:**
  - o vagão "FALLOUT 3" **chega da direita para a esquerda** e para (clipe Walk de 9,33 s, script `00_MainMenu_Train`);
  - personagens animando e nuvens andando;
  - em tela larga o enquadramento fica o do 4:3, **sem o "limbo" cinza** da esquerda.
- **Cor "escura e envelhecida" dos personagens corrigida.** As nuvens do v20 andavam pela matriz de textura do OpenGL, e na placa NVIDIA
  isso deslocava as texturas dos personagens: medido no seu print, 0,55× mais escuros, com o chão e o céu iguais. Agora as UVs do céu
  andam na CPU, e os personagens sempre desenham com a matriz identidade.
- **Câmera do jogo limitada pela área visível:** os 4 cantos da tela ficam dentro do mapa, sem o limbo nas bordas.
- **Arma 2 sem a camada cinza** (pedido do autor).
- **Telas 2D** (criação de personagem) com faixas pretas nas laterais em tela larga.
- **Som dos vídeos de abertura:** o DirectSound do Bink agora é criado com a **janela do jogo**. No v20 ele ficava preso a uma janela
  inexistente do ajudante, e o Windows silenciava o som. Se falhar, ele cai para o waveOut do Bink; `VideoWaveOut=1` no `F3VB.ini` força
  o waveOut. O ajudante grava o próprio log (`logs\video32_log.txt`).
- **Jogador em mapas sem ponto de entrada** (Spelunking, Furnace): nasce no chão do meio do mapa.
- **Logs e conferência:**
  - `F3VB.exe --autoteste` percorre vídeos → menu → criação → **todos os mapas reais**. Ele mede animação, vagão, nuvens, pico do som,
    pés no chão, andar e combate. Salva prints (`logs\autoteste_*.bmp`), escreve `logs\autoteste_<data>.txt` e sai.
  - `F3VBCheck --tudo` roda sem janela:
    - os 119 scripts (`.amx` + `.sma`) na VM, com 13.028 publics;
    - os **40 diálogos** percorridos, com 2.675 falas;
    - as animações e o chão de cada personagem de cada mapa;
    - os 113 sons;
    - e lista as natives que ainda não têm efeito.
  - A cada 30 s o log mostra fps, pico do som, atores animados e vivos, e erros do OpenGL.
  - O log é todo UTF-8 (acabou o "n�o").
- **Build:** `/utf-8` no MSVC (título "tÃ©cnica" corrigido), recurso `.rc`, `ground.cpp` no núcleo, `test_v21` (criação contra os **3 prints
  do original**, incluindo o novo com AG/LK 6: AP 26 = 20+AG, Crítico 6% = LK, Security/Sneak/Steal). Mensagens do `build.bat` atualizadas.
- **Pacote com nome próprio**, `f3vb-starter-v21.zip`. Nenhum pacote entregue é sobrescrito; a próxima correção será `-v21b`.

## v20 (2026-11-03)
- **O `F3VB.exe` lê a demo ORIGINAL direto**: o `resource.rht` e os `.grp` inteiros, sem extrair e sem `original_assets` (`vfs.cpp`).
  - Na 1ª vez abre o **seletor de pastas do Windows**, e a escolha fica no `F3VB.ini` ao lado do `.exe`. Ela vale até apagar o `.ini`,
    copiar o `.exe` para outro lugar (outra pessoa, outro PC) ou abrir com `--escolher-pasta`. Não há caminho fixo.
  - Mapas montados em memória a partir de `.map`, `.8tre` e `.rle` (`tre.cpp`, `map_build.cpp`).
  - `F3VBCheck --demo <pasta> [--assets ...] [--maps] [--models]` confere tudo.
- **Personagens e objetos 3D direto dos `.grp`** (`gr2`, `b3d`, `model_build`, `proto`, `actor`, `actor_gl`):
  - Leitor Granny 2 em C++ (porte do parser MIT do blendergranny), idêntico ao Python em 183 GR2. Malha + esqueleto saem byte a byte iguais
    ao conversor Python em 184/184 modelos.
  - Personagens humanos compostos de corpo + partes, com armadura, pele, arma na mão e animação parada.
  - Junktown com 34 de 35 entidades em 3D e o **jogador no ponto de entrada**. Menu principal com o jogador, o Cpl. Armstrong e o vagão,
    como no original.
  - Achados que causavam os personagens escuros e "vistos por dentro":
    - o atlas de textura de 512 que o motor compõe (UVs refeitas por malha);
    - os materiais `_2X`, `VERTEX_ALPHA` e `IMAGE_ALPHA`;
    - a variante antiga de `.b3d` com pesos (vagão, escala 1,0).
  - Brilho calibrado contra o print do menu original.
- **Vídeos de abertura com som** (`IPlogo.mve`, `BISlogo.mve`; também no botão Cinematics):
  - Os `.mve` são **Bink 1**, decodificados pela `binkw32.dll` da própria demo num ajudante de 32 bits (`F3VBVideo32.exe`,
    `video_helper32.cpp`).
  - O som vai pelo DirectSound do Bink; no teste, o waveOut ficava mudo.
  - Nenhum decodificador de terceiros.
- **Suposições do v19 trocadas por medições** (prints do original, `sys.ini` e `F3.ini` da demo):
  - Câmera do jogo pelo `[Camera]` (elevação 38,5-89, FOV 6-15, distância 350), começando no meio das faixas (63,75° / 10,5).
  - **Azimute inicial = para onde o jogador olha no ponto de entrada.** Corrige o céu aparecendo no seu print.
  - Câmera do menu pelo `[Mainmenu]`.
  - Mapa e ponto de entrada iniciais pelo `[Server]`.
  - Teclas do `[HotKeys]` do `F3.ini`.
  - Fórmulas dos atributos derivados e das perícias tiradas dos dois prints da criação de personagem (tabela em `docs/JOGO.md`); HP 100 fixo.
  - Cor dos textos = D3DCOLOR ×2.
  - ×2 do mapa só em materiais `_2X`.
- **Barra de ações igual ao print do original**: só o quadro do jogador, "PUNCH"/"AP3", arma 2 em cinza, mostradores `000`, registro de
  mensagens "-Welcome to Fallout". A **janela nunca passa da área útil do monitor**, o que corrige a barra cortada embaixo no seu print.
- **Criação de personagem**:
  - Boneco 3D no quadro da direita, como no print.
  - **A aparência muda o modelo de verdade**: corpo, sexo, pele, cabelo, rabo de cavalo, bigode, barba e cor do cabelo, e o jogador entra
    no mapa com ela.
  - "None / AGE 21 / MALE" agora aparece: na ordem do arquivo, o texto ficava debaixo da imagem do botão.
- **Menu que se mexe, como no original** (medido com o original rodando no wine):
  - os personagens repetem a animação parada. A versão anterior deste pacote congelava na última pose depois de 6,67 s;
  - **as nuvens andam para a esquerda**: 34,7 px contra 34 px do original em 7,5 s.
  - O vagão e a câmera ficam parados, como lá.
- **Salvar e carregar** (um jogo, formato próprio `saves\quicksave.f3vbsave`): Save/Load/Delete em `Options_Load`, `F5`/`F9`.
- **Gráficos modernos:** MSAA, filtro anisotrópico, vsync, qualquer resolução/proporção ("Hor+") e tela cheia `Ctrl+F`. **Som** com mixer
  próprio (música, efeitos, ambiente, volumes).
- **Build:** `CMakeLists.txt` com os fontes novos; `image.cpp` passou para o núcleo (os modelos precisam). O `build.bat` agora também
  compila o `F3VBVideo32.exe` com o compilador x86 (`vcvarsall x86`). Checado com `zig c++` (MinGW, 64 e 32 bits) e rodado no wine;
  **ainda não compilado no MSVC**.
- **Falhas conhecidas:**
  - `DS_Mesa_Console.b3d` (variante de `.b3d` que nem o Python lê) e `Cow` (mapa de teste) não montam.
  - O jogador ainda não anda.
  - Inventário/Pip-Boy sem itens.
  - Sem combate.

## v19 (2026-10-29)
- **Primeiro executável jogável: `F3VB.exe`** (`viewer/src/game_main.cpp`; guia completo em `docs/JOGO.md`). Abre com a **interface original** da demo e segue o fluxo real: menu principal (fundo 3D do mapa `Mainmenu`, música e sons de botão) → **criação de personagem** funcional (SPECIAL, perícias marcadas, traços, sexo, idade, nome, atributos derivados, validação) → tela de loading → **Junktown** (`00_03_Tutorial_Junktown`) com câmera livre, barra de ações, menu de pausa, inventário/Pip-Boy (só as telas), opções com sliders gravados e **diálogo com NPCs por clique** (retrato, fala, respostas 1-9). `jogar.bat` abre com as suas pastas; o log fica em `viewer\build\logs\f3vb_log_*.txt`.
- **Formato `.int` decifrado** (25 de 25 arquivos, 1.596 widgets; spec em `docs/specs/int.md`): árvore de widgets (janela, botão, imagem, rótulo, campo), retângulos absolutos em 1024×768, 9 fatias por widget, diferenças entre as versões 1.1/1.2/1.3, sons dos botões, dicas e o sufixo de texto dos rótulos (id do `.stf`, fonte, tamanho, cor, alinhamento). Python: `intfile.py` (leitor e escritor) e o comando **`int-info NOME`**; C++: `gui_int.*`.
- **Sistema de interface em C++ puro** (`viewer/src`, sem bibliotecas): decodificador **TGA** (cru/RLE, paleta, 15-32 bits) e **BMP** (`image.*`); skins `.ini` com estados de botão (`skin.*`); **leitor e rasterizador TrueType próprio** (`ttf.*`: cmap 0/4/6, glifos simples e compostos, `kern`, antialiasing analítico) com atlas; montagem da tela (`gui.*`: nine-slice, estados, filhos por nome, clique, deslocamentos, quebra de linha); desenho OpenGL com tela virtual 4:3 (`gui_gl.*`); som com `winmm` (`audio.*`).
- **Ferramenta `F3VBUiPreview`** (linha de comando, Windows e Linux): desenha qualquer tela `.int` num PNG sem abrir o jogo (`--all`, `--hover`, `--down`, `--bg`). Bom para comparar com prints da demo.
- **Manchas brancas do mapa: causa achada e corrigida.** Os 12 bytes iniciais do `IDXS` de cada nó do `.8tre` são o **cabeçalho do primeiro lote** (`[0x8000|mat][A][0x4000|página][B][k][C]`); o v16-v18 os tratava à parte e o "lote inicial" ficava sem material. Agora o parser lê a **gramática completa desde o byte 0**: nos 2 mapas medidos, 100% dos nós fecham, os triângulos são os mesmos (24.168 e 157.602) e **0 ficam sem estado**. `tre-check` ganhou as colunas `gramatica_completa` e `sem_estado`. Spec atualizada em `docs/specs/tre.md`.
- **Fórmula de cor do mapa refeita** (no jogo e no `F3VBMapViewer`): a luz da demo é toda pré-calculada; `(textura ou branco) × cor de vértice × 2`, depois `× lightmap × 2`; sem luz do OpenGL; `ALPHABLEND` numa segunda passada. O viewer liga em "difusa + lightmap" e sem o piso quando acha texturas.
- **`.f3map`: chunks `CAMR` (do `ECAM`) e `ENVI` (cores e floats do `EMAP`)**, usados como limite da câmera e névoa (hipóteses; `N` liga/desliga). `CONVERTER_VERSION = 19`: **rode `convert-retail --maps` de novo**.
- **Diálogos corrigidos** (medido no `00 Corporal Armstrong`): ids do `.stf` são **1-based**; `RegisterReply(id, texto, ação, destino)`: a condição é testada pelo script antes, então **todas** as respostas registradas aparecem (no v18 sumiam); `RegisterNodeText` = fala do NPC; códigos de cor `{cNNN}` viram cor. Detalhes em `docs/VM_E_DIALOGOS.md`.
- **Comando `import-loose`**: copia `Fonts\*.ttf` e `English.stf` (soltos na pasta da demo, fora dos `.grp`) para `original_assets\RETAIL` (e o `.stf` também para `CONVERTED\scripts`). O `extract-all` agora faz isso sozinho.
- **Build:** novos alvos `F3VB` (WIN32; `opengl32`, `user32`, `gdi32`, `winmm`), `F3VBUiPreview`, `test_ui` e a biblioteca `f3v_ui`. Checado compilando de verdade para Windows 64 bits com os headers do MinGW (via `zig c++`): `F3VB.exe`, `F3VBMapViewer.exe` e `F3VBUiPreview.exe` sem avisos. **Ainda não compilado no MSVC** (é o seu `build.bat`).
- Testes: **137** em Python (novos: `.int` leitor/escritor, cabeçalho do 1º lote do `.8tre`, `import-loose`, o `test_ui` C++ compilado e rodado, sintaxe do jogo com stubs) + **`test_ui.cpp`** (imagens, `.int` das 3 versões com truncamento, skin, fonte TrueType gerada no teste, montagem/clique/estados/deslocamentos, criação de personagem), limpo com ASan/UBSan. `testar_nucleo.bat` roda o `test_ui`.
- **Não verificado no Windows:** a aparência real no seu PC (escala, fontes, destaque aditivo dos botões), névoa/`ECAM`, as fórmulas de perícias e atributos derivados (provisórias, estilo Fallout 2).

## v18 (2026-10-29)
- **Bug da VM corrigido (causa dos "instrução inválida" no v17):** scripts compilados com símbolos de depuração têm `FILE`/`SYMBOL` (tamanho variável) no meio do código e a VM executava o nome dos símbolos como instruções; agora pula o payload. Também: natives importadas duas vezes são registradas nas duas entradas; o levantamento passa 8 argumentos zero (evita falsos "acesso inválido"). Teste de regressão sintético que falha sem a correção. Detalhes em `docs/VM_E_DIALOGOS.md`.
- **Texturas no F3VBMapViewer:** `.f3map` **versão 3** (UVs por vértice: difusa e lightmap), `py -3 -m f3vb export-textures` (e `convert-retail --maps`) gravam `CONVERTED/textures/<nome>.f3tex` (RGBA cru; difusas e lightmaps citados pelos mapas), o viewer carrega com mipmaps. Tecla **`X`**: sem textura / **difusa** / **difusa + lightmap** (multitextura, lightmap ×2 como `_LM_2X`). Mistura por alfa nos materiais `ALPHABLEND` e corte de alfa nos `*IMAGE_ALPHA*` são **suposições**. Liga sozinho em modo difusa quando acha texturas.
- **Diálogos:** `V` usa o script da entidade e, se ele não tem `OnStartDialogue` (o de comportamento não tem), procura **por nome** um script de diálogo; **`K`** passa para o próximo script de diálogo da biblioteca em qualquer entidade. O painel mostra o script em uso.
- **Levantamento (`B`, `F3VBScript --survey`):** agora lista todas as natives chamadas e até 2 exemplos de argumentos (com textos) das que faltam implementar: é o mapa do que a UI/menus precisam.
- **Base para a interface do jogo:** `py -3 -m f3vb ui-info` (inventário do pack Interface, sonda dos 25 `.int` com enquadramento de chunks e strings, imagens TGA/BMP da UI em `CONVERTED/ui/*.f3tex`). Ainda **não decifra** o `.int`.
- **.stf verificado** no seu `English.stf` (3.281 textos). `tre-check` reconferido por você: 5925 nós, 0 não fechados, 0 fora da caixa, 1.928.117 triângulos (igual ao esperado), então o modo estrito de enchimento do v16 está validado.
- **Build:** corrigido o aviso C4244 do `draw_dialog`; stubs e CMake atualizados (`f3tex.cpp` no núcleo).
- Testes: **123** (novos: regressão do `FILE`/`SYMBOL`, duplicatas, casamento de diálogo por nome, `.f3map` v3, `.f3tex`/mipmaps com fuzz, `export-textures`, `ui-info`).
- **Não verificado em dado real:** a aparência com texturas/lightmaps (nunca rodou no Windows), o nome do `.tga` dos lightmaps (`.ctx` -> TGA_Image pelo nome; se faltar, `textures_summary.txt` lista), o elo entidade -> script de diálogo.

## v17 (2026-10-26)
- **VM ligada ao mapa:** no `F3VBMapViewer`, `Tab` seleciona uma entidade e **`V` roda o diálogo do script dela** (VM AMX própria + camada de natives), com painel de falas/respostas na tela e **`1`-`9`** para responder; **`B`** roda todas as publics de cada script do mapa e escreve um **levantamento** no log (erros da VM e natives que faltam). Detalhes e suposições: `docs/VM_E_DIALOGOS.md`.
- **Novo executável `F3VBScript`** (linha de comando, sem janela): `F3VBScript ARQ.amx [--stf T.stf] [--choices 1,2] [--survey] [--log X.txt]` mostra falas, respostas, passos da VM e chamadas de natives; é o jeito mais simples de depurar um script e me mandar a saída.
- **`convert-retail --maps` agora também grava os scripts** em `CONVERTED\scripts\<nome>.amx` (tipos AMX e SMA_Script; nomes repetidos ganham sufixo). Comando novo **`export-scripts`** (só os scripts, em segundos, sem refazer mapas).
- **Tabela de strings (`.stf`) em C++** (`stf.*`): layout hipotético do OpenJE, **não verificado**; sem ela as falas aparecem como `[texto #id]`. Coloque o `.stf` do jogo em `CONVERTED\scripts` ou use `F3VB_STF`.
- **GameHost:** guarda as falas (`FloatingSpeechText`, `PCSpeaker` com 1º argumento = id do texto, suposição) e permite reconstruir os nós do diálogo; API nova `speeches()`, `nodes_rw()`, `reset_dialog()`.
- **Build:** o projeto do viewer passa para **C++20** (a VM usa `std::span`); nova biblioteca `f3v_script`, alvos `F3VBScript` e `test_script`; `testar_nucleo.bat` roda o `test_script`.
- **Ainda sem texturas na tela:** fica para o **v18** (TGA, difusas, lightmaps).
- Testes: **120** no total (novos: scripts/diálogo/STF/levantamento/fuzz em C++ com scripts sintéticos; `export-scripts` e scripts no `convert-retail`; o `F3VBScript` de ponta a ponta). Rodei os testes C++ novos também com ASan/UBSan: limpos.
- **Não verificado em dado real:** formato do `.stf`, nome do script citado pela entidade, e a semântica do fluxo de diálogo (é a deduzida de `docs/DIALOGOS.md`).

## v16 (2026-10-26)
- **Lotes de índices do `.8tre` (`tre.py`):** o fluxo de índices de cada nó tem **cabeçalhos de lote** no meio (`0x8000|material`, `0x4000|página de lightmap`, sublotes `[k][C]` com textura difusa). O v15 lia essas palavras como índices de vértice: daí os **espinhos** longos no Mainmenu. Agora cada triângulo sabe **material, página de lightmap e textura difusa**. Medido nos 38 arquivos reais (no chat anterior): 5.925 nós, 0 que não fecham, 1.928.117 triângulos, 0 índices fora da caixa da folha. Gramática e evidências: `docs/specs/tre.md`.
- **Modo estrito de enchimento (novo, só testado com dados sintéticos):** o leitor tenta primeiro exigir que os `u16` de enchimento sejam 0 (elimina um fechamento falso que os testes sintéticos revelaram) e cai no modo frouxo se nada fechar. **Reconfira nos arquivos reais:** `py -3 -m f3vb tre-check PASTA_DOS_8TRE` (esperado: nos=5925, nao_fechados=0, fora_bbox=0, triangulos=1928117).
- **`.f3map` versão 2** (`mapexport.py`, `f3map.h/.cpp`): listas de triângulos por (bloco, material, página, textura) + chunks `MATL` (materiais) e `TEXN` (nomes das texturas). O leitor lê **v1 e v2**; `build_f3map(..., version=1)` ainda grava o formato antigo. **Reconverta os mapas** (`convert-retail --maps`) para ter a v2.
- **F3VBMapViewer v16:** tecla **`M`** alterna a cor da geometria entre cor de vértice, **material**, **página de lightmap** e **textura difusa**, com **legenda** no HUD (cor, nome, nº de triângulos). O log ao abrir o mapa e o `F2` trazem as tabelas de materiais/páginas/texturas e a linha **"espinhos"** (triângulos com aresta maior que 20% da diagonal do mapa; use-a para comparar com o v15).
- **Novos comandos:** `tre-check PASTA` (confere o parser de lotes em todos os `.8tre`); `tre-info` e `map-info` agora mostram os lotes e a tabela de materiais com triângulos por material.
- **Correções de teste:** o teste de sintaxe do `mapviewer_main.cpp` agora exige só código de saída 0 (o MinGW emitia ~45 KB de avisos por causa dos stubs; era a única falha no seu Windows); arquivo fechado em `test_extractor.py`.
- **Ainda sem texturas na tela:** desenhar com `TXUV`/`LMUV` e carregar os `.tga` fica para o **v17** (os dados necessários já estão no `.f3map` v2).
- Testes: **118** (11 novos em `test_tre.py`: gramática completa, enchimento, bits altos em blocos grandes, nó que não fecha, ponta a ponta, `.f3map` v1/v2, `tre-check`; C++: v1, v2, lotes e fuzz em 3 arquivos).

## v15 (2026-10-19)
- **`F3VBMapViewer.exe` (novo, ao lado do `F3VBViewer.exe`):** escolher um mapa (dialogo ou `PgDn`/`PgUp`) e abri-lo em 3D: geometria do `.8tre`, planta do `.rle`, entidades, navegação, gatilhos, caminhos, sons, marcadores, efeitos, entradas e água; seleção de entidade; log `f3mapviewer_log_*.txt`. **Só compilado contra stubs; execução no Windows aguarda o seu teste.** Ver `docs/MAPVIEWER.md`.
- **`.8tre` decodificado (38/38 até o último byte):** `tre.py`; 1.946.527 triângulos; 99,06% dos índices dentro da caixa da folha. Spec: `docs/specs/tre.md`. Comando `tre-info`.
- **Formato `.f3map`** (`mapexport.py` + `f3map.h/.cpp`): mapa decodificado num arquivo só; `convert-retail --maps` grava um por mapa. Leitor C++ com checagem de limites, fuzz e validação nos 34 mapas reais.
- **Correção do v14:** a regra mundo → célula é única (`(pos − origem) / 0,5`, origem no canto mínimo do `.8tre`); o "fator por mapa" do v14 era um artefato. Corrigido nos docs e no relatório.
- Log com prefixo configurável (`f3mapviewer_log`); `map-info --tre/--f3map`.
- Imagens `_AM` (automap) e páginas de lightmap `.ctx` → `<nome>.tga` identificadas (555 TGA decodificam).
- Testes: **107** no total (`test_tre.py` com 9 novos, 2 do C++ `f3map` e a sintaxe do `mapviewer_main.cpp` na suíte).

## v14 (2026-10-14)
- **`.rle` (grade de tiles) decodificado e validado:** `tools/py/f3vb/rle.py` (leitor + escritor; re-codificação byte a byte nos 38 arquivos reais), imagem PNG da grade. Spec: `docs/specs/rle.md`.
- **`.map` (descrição do mapa) decodificado:** `mapfile.py` lê os 14 tipos de chunk (entidades com inventário/script/pele, efeitos, pontos de entrada, grafo de navegação, gatilhos e viagens, caminhos, sons, marcadores, água), todos com o payload inteiro explicado (0 byte sobrando nos 37 mapas); os números ainda sem nome do `EEOV` ficam preservados em hex. Spec: `docs/specs/map.md`.
- **Relatório, JSON e imagem de visão superior por mapa** (`mapreport.py`): grade + entidades por tipo + navegação + gatilhos + caminhos + entradas + sons + água.
- **Comandos novos:** `map-info`, `rle-info`; opção `--maps` em `extract-all` e `convert-retail` (grava `CONVERTED\maps\<mapa>.txt/.json/.png` e `maps_summary.txt`; `convert-retail` sem flags agora inclui mapas).
- Testes: **107** no total (13 novos em `tests/test_maps.py`) + correção do teste da RPGGL no Windows (agora é ignorado lá: usa POSIX).
- Documentação: especificações dos formatos, achados dos mapas, comparação com o OpenJE (`docs/COMPARACAO_OPENJE.md`).

## v13 (2026-10-13)
- **Humanos e "esqueleto 3D" (HumFSkl):** ossos do B3D casados com as juntas do GR2 por sufixo (`L Clavicle` ↔ `L Clavicle_1`) no **conversor** e no **visualizador** (`Scene::find_joint`); osso realmente ausente deixa o peso no bind em vez de colapsar a malha para a origem. Causa dos braços "em lâmina", medida no log do usuário.
- **Animações de humanos por família:** `HumM_*`/`HumF_*` (antes: `NomeDoModelo_*`, que não existe para humanos, por isso `animacoes=0`). O F3M de esqueleto leva todas as animações da família.
- **Conversor v13** (`CONVERTER_VERSION = 13`): refaça `CONVERTED` com `py -3 -m f3vb convert-retail --png --f3m` (os de v12 abrem, mas sem estes consertos).
- **Falhas da extração legíveis:** o relatório traz o resumo por causa e por pack e a lista completa em `FALHAS_COMPLETO.txt`.
- **Ferramentas novas:** `b3dmat NOME` (materiais, texturas, UVs e ossos de um modelo) e `pack-sample NOME...` (zip de amostra b3d + esqueleto + texturas + animações para enviar para análise).
- **Segunda engine:** RPGGL v0.2 em `engine_test/rpggl/` (inalterado), teste headless dentro da suíte (68 verificações, 8 s), análise e plano em `docs/RPGGL_ANALISE.md`, licença registrada (pendente do autor).
- Achados: escala 1/10,24 confirmada em 7 modelos (cão, Eyebot e 5 humanos, todos dentro de ±5%). Ver `docs/ACHADOS_DEMO.md`.
- Visualizador v13 (mesmas teclas do v12). Testes: 96 (Python, inclui C++ e o RPGGL) + `test_v12` com os testes novos de apelido de osso e osso ausente.

## v12 (2026-10-12)
- **Pasta `original_assets/` com `RETAIL/` (originais) e `CONVERTED/` (convertidos)**; `convert-retail` refaz CONVERTED sem os `.grp`; versão do conversor registrada; trava anti-asset (`scripts/verificar_sem_assets.py`, `scripts/empacotar.py`, `.gitignore`). Ver `docs/ORIGINAL_ASSETS.md`.
- **Causa provável das animações quebradas corrigida no conversor:** a geometria do B3D está ~10,24× maior que o esqueleto do GR2 (`DEFAULT_MESH_SCALE = 100/1024`, medido em CR_Dog e CR_FloatingEyeBot). Pose de bind parecia certa, animação não.
- **Posicionamento inicial do GR2:** o Eyebot tem `mundo × InverseWorld` = translação constante (0,0117; 0; 0,0087) em todas as juntas; o conversor compensa nas raízes (o "deslocamento de ~0,012" que estava sem explicação). Pose de bind skinada = malha (erro ~1e-7).
- **Visualizador v12:** log automático `logs\f3viewer_log_MM-DD-YY_hh-mm-ss.txt` (teclas, comandos, carregamentos, análise completa de malha/textura/ossos/animação); `P` agora liga/desliga a pose de bind; `K` lista os ossos no log; `F2` análise completa; `N`/`M`/`,`/`.` escala extra da malha; `f3v_analyze` gera o mesmo relatório sem janela. Ver `docs/VISUALIZADOR.md`.
- Documentação reescrita (README, COMO_TUDO_FUNCIONA, VISUALIZADOR, PARA_ONDE_VAMOS) e inventário de licenças (`docs/LICENCAS_E_DIREITOS.md`).
- Testes: 73 de Python (inclui compilar e rodar o núcleo C++) + `test_v12` (log, análise, escala, fuzz da análise).

## v11.1 (2026-10-11)
- `build.bat` corrigido para o Visual Studio 2026 (Ninja sem `-A x64`; fallback para o gerador do VS); `.bat` em CRLF; `testar_nucleo.bat` acha o `test_core.exe` nos dois layouts. Confirmado pelo usuário: compila, abre o Eyebot, 60 FPS.

## v11
- Extrator com interface (tkinter), formato F3M (`docs/VISUALIZADOR.md`), visualizador 3D em C++/OpenGL, exportador GLB com esqueleto e animação.

## v8 a v10 (resumo)
- v8: relatórios e catálogo `rht` x `grp`. v9: terreno de 64 bytes decodificado nas amostras. v10: leitor completo de B3D, exportador GLB com esqueleto/animação, natives de diálogo.
