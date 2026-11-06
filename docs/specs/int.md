# Formato `.int`: layout de uma tela da interface (recurso `INT_Interface`)

**Estado (v19, medido nos 25 arquivos reais do pack Interface):** os 25 são lidos **até o último byte** e **regravados byte a byte idênticos** pelo escritor de teste (`tools/py/f3vb/intfile.py::build_int`), 1.596 widgets no total. Leitores: `tools/py/f3vb/intfile.py` (Python) e `viewer/src/gui_int.h/.cpp` (C++). Testes sintéticos: `tools/py/tests/test_intfile.py` e `viewer/tests/test_ui.cpp`.
Versões vistas: `GUI V1.1` (Loading), `GUI V1.2` (AreaMap, Debug, DebugTools, Dialog, Feedback) e `GUI V1.3` (as outras 19).

## Ideia geral
Cada tela é uma **árvore de widgets serializada em profundidade**. Cada widget tem um retângulo **absoluto em pixels numa tela de 1024×768**, uma *skin* (`.ini`), e **9 fragmentos** (as "nove fatias" de uma moldura: cantos, bordas e meio), cada um com textura, cor, modo (esticar/repetir) e o seu próprio retângulo absoluto. O comentário do `AreaMap.ini` da própria demo descreve esse sistema (objetos, fragmentos, estados, skins); o `.int` é a "fotografia" de uma tela montada nesse sistema.

## Layout (little-endian; `str` = `u32` comprimento + bytes, **sem** terminador)
```
"GUI V1.x"              8 bytes
str  nome do arquivo    (ex.: "Mainmenu.INT")
u8   ?                  (0 nos 25)
u32  N raízes           (1 em quase todos; 2 no Dialog)
N × objeto

objeto =
  u32 tipo              0 janela, 1 botão, 2 imagem, 3 rótulo (texto), 4 campo de edição
  prefixo(tipo)         só o botão tem: u32 ?; em V1.3 também u8 ? e 3 × str sons (apertar, soltar, soltar; ex. "f3_button_down_01.wav")
  i32[4] retângulo      ABSOLUTO (x0, y0, x1, y1) na tela de 1024×768
  str  nome             (ex.: "NewGameBtn")
  u8 a, u8 b, u8 c      sem significado conhecido
  str  skin             (ex.: "zzmainmenu.ini"; vem sempre, mesmo quando c = 0)
  u8[4] cor             (rgb 0x80 0x80 0x80 + 1 byte variável)
  i32[4] retângulo 2    relativo a algo (pai/âncora); não é preciso para desenhar
  u8   ?
  u32  interativo?      1 em janelas, botões e campos; 0 em imagens e rótulos (hipótese; NÃO é visibilidade: imagens visíveis têm 0)
  [V1.2+] u32 ?, i32 id A, u32 ?, i32 id da dica, str textura da dica   (ex.: dica 72 + "f3_tooltip_skills.tga" na criação de personagem; -1 = nenhuma)
  9 × fragmento         ordem: upperleft, uppermiddle, upperright, left, middle, right, lowerleft, lowermiddle, lowerright
  i32  -1               (sempre -1 nos 25 arquivos)
  u32  N filhos
  N × objeto            (filhos, recursivo)
  sufixo(tipo)          só rótulo e campo de edição, e DEPOIS dos filhos (um rótulo pode ter filhos: Inventory.lblHand1)

fragmento =
  u32  flags            0 esticar, 1 repetir em x, 2 repetir em y, 3 repetir em x e y (mesmos valores do campo "flags" das skins .ini)
  i32  largura, altura  do recorte da textura (ex.: 32×32 para zzmain-*.tga; 192×276 para f3_front_end_buttons.tga)
  i32 ?, i32 ?          (0 em quase todos)
  u32  ?                normalmente 1; às vezes é LIXO de memória do editor original (ex.: 0x70682804), por isso NÃO é "tem textura"
  str  textura          ("" = sem textura)
  f32  ?                (valor por objeto, ex. 0,0867; sem uso conhecido)
  u8[4] cor RGBA        alfa 0 = invisível nesse estado (os botões do menu principal têm alfa 0 no estado "up")
  i32[4] retângulo      ABSOLUTO na tela de 1024×768

sufixo do rótulo / campo de edição =
  u32  id do texto      1-BASED no .stf (id N = entrada N−1; 0 = sem texto). Medido: rótulo ao lado de QuitBtn = 16 -> entrada 15 "Quit"
  str  fonte            "Fallouty", "fallouty", "Gothic 821 Condensed BT", "Arial", "Lucida Console"
  u32  ?                (0)
  u32  tamanho          em pontos (9, 10, 12, 16, 18, 20, 22, 24, 30)
  u32  ?                (1)
  u8[4] cor RGBA do texto
  22 bytes ?            (quase sempre zero; os 2 primeiros variam: 00 01, 00 02...)
  u32  alinhamento      0, 1 ou 2 (hipótese: esquerda, centro, direita)
```

## O que foi medido e como
- **Tipos 0–4 bastam** para os 25 arquivos (628 rótulos, 576 imagens, 247 botões, 141 janelas, 4 campos de edição).
- **Versões:** a V1.2 não tem o `u8` e os 3 sons do botão; a V1.1 também não tem os 5 campos da dica no corpo. Com essas duas regras os 25 fecham.
- **Filhos antes do sufixo:** só assim `Inventory.int` fecha (um rótulo com um filho).
- **Ids de texto 1-based:** conferido em todos os rótulos com sentido evidente (botões do menu de pausa, abas das opções, botões da MessageBox, cabeçalhos da criação de personagem).
- **Retângulos absolutos:** os fragmentos de cada botão do menu principal caem exatamente dentro do retângulo do botão; a imagem `f3_front_end_buttons.tga` (192×276) cobre o painel (24,24)-(216,300).

## Fontes e skins
- **Fontes:** `fontlist.ini` lista Arial, Arial Black, Lucida Console, Times New Roman, Hellbound Smooth, Fallouty e Gothic 821 Condensed BT. A demo traz `Fonts/r_fallouty.ttf`, `Fonts/JH_FALLOUT.TTF` e `Fonts/TT0807M_.TTF`; o F3VB rasteriza esses TrueType com código próprio (`viewer/src/ttf.cpp`). Arial e Lucida são do Windows: o F3VB troca por uma fonte da demo.
- **Skins (`*.ini`):** seções `[objeto.estado.fragmento]` (ex.: `[button.selected.middle]`) com `flags`, `texture`, `color = r,g,b`, `width`, `height`, `rate`. Os botões têm 4 estados (`up`, `selected`, `down`, `disabled`). O `.int` guarda só o estado de repouso; os outros vêm da skin (`viewer/src/skin.h/.cpp`).

## O que ainda não se sabe
Os campos marcados com `?`, o significado exato do retângulo 2, do `id A`, dos 22 bytes do rótulo e do `f32` dos fragmentos. Nada disso é necessário para desenhar e usar as telas.
