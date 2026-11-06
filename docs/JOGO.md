# F3VB Engine (F3VB.exe): o jogo (v21)

O executável que **é o jogo**. Desde o v20 ele lê a **demo original direto**: o `resource.rht` e os `.grp` inteiros, sem extrair nada e sem
`original_assets`. Ele abre com os vídeos da Interplay e da Black Isle e segue para o menu principal 3D com os personagens e o vagão. Depois vêm a
criação de personagem (com o boneco 3D que muda conforme a aparência) e o Junktown, com o jogador, os NPCs e os objetos em 3D.

Nada da demo está dentro do executável nem do kit (veja `CLEANROOM.md`): tudo é lido da pasta da demo de quem joga.

## Como rodar
```bat
:: compilar (Developer Command Prompt do Visual Studio 2026) e jogar
cd /d "<pasta do kit>\viewer"
build.bat
build\F3VB.exe
```
**Na primeira vez** abre o seletor de pastas do Windows: escolha a pasta da demo, a que tem o `resource.rht`
(no seu PC: `F:\F3VB Engine\Demo\Demo`). A escolha fica gravada no **`F3VB.ini` ao lado do `F3VB.exe`** (`demo=...`). Ela só é perguntada de novo
quando:
- o `F3VB.ini` é apagado;
- o `.exe` é copiado para outra pasta (o `.ini` não vai junto), por exemplo outra pessoa em outro PC;
- a pasta gravada deixou de servir (aparece um aviso e o seletor);
- o jogo é aberto com `F3VB.exe --escolher-pasta`.

O `build.bat` também compila o **`F3VBVideo32.exe`** (32 bits, ao lado do `F3VB.exe`), que decodifica os vídeos de abertura. Sem ele o jogo
funciona normalmente e só pula os vídeos.

O log de cada execução fica em `logs\f3vb_log_<data>.txt`, ao lado do `.exe`. **É ele que eu preciso** quando algo sair errado. Ele lista a
pasta da demo, os recursos, as fontes, as telas e as texturas que faltaram. Também lista os modelos 3D montados, os vídeos, os cliques e o diálogo.

### Linha de comando
| Opção | Para quê |
|---|---|
| `--demo "<pasta>"` | usa esta pasta da demo (e grava no `F3VB.ini`) |
| `--escolher-pasta` (ou `--reset`) | abre o seletor de pastas de novo |
| `--skip-menu` | sem vídeos e sem menu: vai direto para o mapa inicial |
| `--map NOME` | mapa inicial (ex.: `00_03_Tutorial_Junktown`); o padrão vem do `sys.ini` da demo (`[Server] Start map`) |
| `--chargen` | abre direto a criação de personagem (teste) |
| `--check` | só confere a pasta da demo e mostra um relatório |
| `--autoteste` | v21: percorre tudo sozinho (vídeos, menu, criação, todos os mapas reais: andar e combate) e grava `logs\autoteste_<data>.txt` e prints `logs\autoteste_*.bmp` |

### `F3VB.ini` (ao lado do `.exe`; o jogo grava sozinho)
| Chave | Padrão | O que é |
|---|---|---|
| `demo` | (escolhida no seletor) | pasta da demo original |
| `WindowWidth`, `WindowHeight` | 80% da área útil do monitor | tamanho da janela; **nunca passa da área útil** (sem a barra de tarefas) e abre centralizada |
| `Fullscreen` | 0 | tela cheia (`Ctrl+F` ou `Alt+Enter`) |
| `StartupMovies` | 1 | vídeos de abertura |
| `MSAA`, `Anisotropy`, `VSync` | 8, 16, 1 | gráficos modernos (antisserrilhamento, filtro anisotrópico, sincronia vertical) |
| `MasterVolume`, `MusicVolume`, `SfxVolume`, `AmbientVolume` | 100 | volumes (sliders das opções) |
| `DemoFonts` | 0 | 1 = usa as fontes da pasta `Fonts` da demo também no lugar da Arial |
| `ChargenActorYaw` | 200 | giro do boneco da criação de personagem, em graus |
| `SkyScrollU`, `SkyScrollV` | 0,0054, 0 | velocidade das nuvens do menu (coordenada de textura por segundo) |
| `CameraFollow` | 1 | a câmera acompanha o jogador enquanto ele anda |
| `VideoWaveOut` | 0 | 1 = som dos vídeos pelo waveOut do Bink, em vez do DirectSound preso à janela do jogo |

## Fluxo das telas
```
vídeos (IPlogo, BISlogo) ──> Mainmenu 3D ──New Game──> CharacterGeneration ──Done──> Loading ──> mapa (Actionbar embaixo)
   │  Load Game / Options ──> Options_* (abas)                                     │  Esc ──> EscapeMenu
   │  Cinematics ──> os dois vídeos de novo                                        │  I / P ──> Inventory / PipBoy
   └─ Quit                                                                         │  C ──> CharacterGeneration (só ver)
                                                                                   └─ clique num NPC ──> Dialog (respostas 1-9)
```

| Tela | O que funciona no v20 |
|---|---|
| Vídeos | `Movies\IPlogo.mve` e `BISlogo.mve` (são Bink 1, 640×320, som 22050 Hz) **com som**, decodificados pela **`binkw32.dll` da própria demo** no ajudante `F3VBVideo32.exe`; `Esc`/`Espaço`/`Enter`/clique pulam |
| `Mainmenu` | mapa `Mainmenu` com câmera do `sys.ini` (`[Mainmenu]`: na origem, altura 5,5, olhando +Z, FOV 68). Também tem os **três modelos 3D da demo**: o jogador (`Default_PC`, macacão da prisão), o Cpl. Armstrong (armadura de força, minigun) e o vagão. Medido no original rodando: os personagens repetem a animação parada, **as nuvens do céu andam devagar para a esquerda** (~34 px em 7,5 s a 800×600; aqui 34,7) e o vagão e a câmera ficam parados. Brilho calibrado pelo print do original |
| `CharacterGeneration` | SPECIAL, perícias (3 marcadas, +20), traços (até 2), sexo, idade, nome, derivados com as **fórmulas medidas nos prints do original** (abaixo). O **boneco 3D** fica no quadro escuro à direita das estatísticas e muda com a **aparência** (corpo, pele, cabelo, rabo de cavalo, bigode, barba e cor do cabelo); na aba THRESHOLDS ele não aparece, como no original |
| `Loading` | a moldura com a imagem "LOADING!" centrada, fundo preto |
| Mapa | o mapa com **todas as entidades em 3D** (34 de 35 no Junktown) e o **jogador no ponto de entrada** do `sys.ini` (`Start map entry point = 0`), com a aparência escolhida. A câmera começa olhando para onde o jogador está virado |
| `Actionbar` | como no print do original: só o 1º quadro do grupo (nome do jogador), arma 1 "PUNCH"/"AP3", arma 2 apagada em cinza, HEALTH/FATIGUE/EVASION em `000`, "25" pontos de ação, registro de mensagens com "-Welcome to Fallout" |
| `Options_Load` | **Save**, **Load** e **Delete** funcionam (um jogo salvo; ver abaixo) |
| `EscapeMenu`, `Dialog`, `MessageBox`, `Options_*` | como no v19 (com os sliders gravados no `F3VB.ini`) |

## Teclas (as do `[HotKeys]` do `F3.ini` original, mais alguns extras)
| Onde | Tecla | Ação |
|---|---|---|
| Sempre | `Ctrl+F` / `Alt+Enter` | tela cheia / janela |
| Sempre | `F1` (ou `F3`) | estatísticas (FPS, triângulos, tela sob o mouse) |
| Sempre | `F8` | liga/desliga o som (extra) |
| Vídeo | `Esc` / `Espaço` / `Enter` / clique | pular |
| Menu | `Enter` | New Game |
| Mapa | setas `←` `→` | girar a câmera (azimute) |
| Mapa | setas `↑` `↓` | câmera mais de cima / mais de lado (38,5° a 89°, `sys.ini`) |
| Mapa | clique esquerdo no chão | andar até lá (caminho pelo chão; corre se for longe) |
| Mapa | `A` ou clique na arma da barra | modo de ataque (AttackMode do original); depois clique num personagem |
| Mapa | clique num personagem hostil | atacar (combate em tempo real) |
| Mapa | numérico `8 2 4 6` | rolar a câmera. Desde o v21, `W`/`S`/`A`/`D` não rolam mais: no `[HotKeys]` do `F3.ini` original W é a arma, S furtivo e A ataque |
| Mapa | `PgUp` / `PgDn`, roda do mouse | zoom (FOV de 6 a 15, `sys.ini`) |
| Mapa | `X` ou botão direito arrastando | girar com o mouse |
| Mapa | `Home` | volta a câmera para o início |
| Mapa | `I`, `P`, `C` | inventário, Pip-Boy, ficha do personagem |
| Mapa | `G`, `V`, `L`, `H`, `M` | abas de opções: jogo, vídeo, salvar/carregar, teclas, multijogador |
| Mapa | `F5` / `F9` | salvar / carregar rápido (extra) |
| Mapa | `F2`, `F4` | marcadores das entidades, névoa (extras) |
| Mapa | `Ctrl+L`, `Ctrl+W`, `Ctrl+N` | lightmaps, wireframe, novo jogo |
| Mapa | `Esc` | fecha painel / menu de pausa |
| Diálogo | `1`-`9`, clique | escolher resposta; `Esc` encerra |

## Jogo e combate em tempo real (v21)
Regras do F3VB, porque a lógica do `F3.exe` não foi decifrada. Os números marcados com (H) são hipótese, até haver um print ou arquivo
da demo que diga o contrário.
- **Chão:** `ground.cpp` lê os triângulos do `.8tre`.
  - Altura no ponto: a superfície mais alta até 2 unidades acima dos pés, então sobe calçada mas não "sobe" no telhado.
  - Paredes (inclinação acima de ~60°) e vidros (`ALPHABLEND`) não contam como chão.
  - Ao carregar, todo personagem e criatura é colado no chão. No Junktown eles estavam 1 unidade acima.
- **Andar:** A* numa grade de 1,5 unidade. Velocidade 5 un/s andando e 12 un/s correndo, a partir de 25 unidades de caminho (H: humano de
  6 unidades ≈ 1,8 m). O jogador não atravessa outros personagens vivos. A sombra é redonda, com raio de 28% da altura.
- **Animações:** os clipes de cada papel vêm da demo.
  - Humanos: `HumM_Com_<Una|1H|2HH>_Walk_01`, `HumM_Com_Una_Run_01`, ataque `Punch_Jab` / `Melee_Slash` / `Gun_Shoot` / `Gun_Burst`,
    `DmgFr_01`, `ProneB_Una_Death_01`.
  - Criaturas: `<modelo>_Com_Una_Walk_01`, `Attack1_01`, `DmgFr_01`, `ProneB_Una_Death_01`.
- **Combate:**
  - O golpe sai no meio da animação de ataque. Intervalo entre ataques = duração da animação + 0,35 s.
  - Acerto do jogador = perícia da ficha / 2 + 20, entre 5% e 95% (H). A perícia é Unarmed sem arma, Melee com faca etc., e Firearms com
    arma de fogo.
  - Dano do soco: 1 a 4 + metade do Melee Damage (H).
  - NPCs (H):
    | Tipo | Vida | Dano | Alcance | Acerto |
    |---|---|---|---|---|
    | Humano | 50 | | | |
    | Cachorro | 25 | 3-6 | 3,5 | 65% |
    | Faca | | 4-8 | 3,5 | |
    | Pistola | | 5-10 | 35 | |
    | Minigun / lança-chamas | | 8-15 | 40 | |
  - Ninguém é hostil até ser atacado. O atacado revida, e os do mesmo modelo num raio de 40 entram junto.
  - Morte: animação até o último quadro; "-<nome> died." no registro.
  - Morte do jogador: "Você morreu." e volta ao menu.

## Câmera do jogo (v20)
- Distância 350, elevação de 38,5° a 89°, FOV de 6 a 15 e as velocidades vêm do **`sys.ini` da demo** (`[Camera]`).
- Começa no **meio das faixas**: elevação 63,75° e FOV 10,5. O print do original mostra a câmera bem inclinada, sem céu. O "50° / FOV 10" do v19
  era chute.
- O **azimute inicial é o ângulo do ponto de entrada** (para onde o jogador olha). No Junktown o ponto 0 fica na borda norte, virado para a cidade.
  Com o azimute 0 fixo do v19, a câmera olhava para fora do mapa e mostrava o céu, que era o bug do seu print.
- O alvo fica preso no retângulo `ECAM` do mapa.
- v21: a **área visível** também fica dentro do mapa. Os 4 cantos da tela são projetados no chão, e o alvo é empurrado até a pegada caber
  na caixa da geometria. Isso acaba com o fundo sem mapa nas bordas.
- v21: o **menu** mantém a abertura horizontal do 4:3 em tela larga ("Vert-"), então o enquadramento é o do original, sem a faixa cinza.

## Personagens e objetos 3D (v20)
Montados **direto dos `.grp`**, em C++, sem o conversor Python:
- `gr2.cpp`: leitor de Granny 2 (versão 6, seções Oodle0, curvas `LegacyCurve32f`). É um porte C++ do parser MIT do blendergranny e deu
  **resultado idêntico ao Python em 183 arquivos**.
- `b3d.cpp`: malhas `.b3d`, incluindo a variante antiga "64 bytes com pesos" do vagão.
- `model_build.cpp`: une malha e esqueleto, como o conversor Python. Ficou **byte a byte igual** ao `.f3m` do Python no `HumMNor` e em 184/184
  modelos testados.
- `proto.cpp`: protótipos `.CRT`/`.ARM`/`.USE`/`.DOR`... (modelo, partes, pele, armadura, arma, script).
- `actor.cpp`: monta o personagem com corpo e partes `HumMNor_<Parte>`. A armadura troca as partes do mesmo encaixe, e as regiões cobertas do
  corpo somem. Também aplica a pele e a textura própria de cada parte, põe a arma no osso `R Handle` e escolhe a animação parada
  `HumM_Def_<Una|1H|2HH>_Idle_01`.
- `actor_gl.cpp`: desenho com pele por CPU e luz por vértice:
  - sol = a luz do mapa mais distante;
  - ambiente = `clamp(cor A × 1,25 / 255; 0,35; 1)`;
  - materiais `*_2X` multiplicam ×2, `*VERTEX_ALPHA*` misturam pelo alfa do vértice e `*IMAGE_ALPHA*` cortam em 0,5.
  - Calibrado contra o print do menu original (macacão 80 × 78, vagão 128 × 121).
- Conferência: `F3VBCheck --demo "<pasta>" --models` monta todas as entidades de todos os mapas. Na demo deu 155 de 175 montadas, 18 sem modelo
  desenhável e 2 falhas conhecidas:
  - `DS_Mesa_Console.b3d` é uma variante de `.b3d` que nem o leitor Python entende;
  - `Cow`, num mapa de teste.

### Aparência na criação de personagem → peças da demo
| Coluna | Vira |
|---|---|
| Body Type (Normal, Heavy, Strong, Wiry) | `HumMNor`, `HumMFat`, `HumMStr`, `HumMWir` (FEMALE: `HumF*`) |
| Skin Color ("Tan 1"...) | `Skin_HumM_Tan01_LG.tga` |
| Hairstyle, Ponytail, Mustache, Beard | `HumMNor_HaiAfro`, `PonPonytail1`, `MusMuttonChops`, `BeaGoatee`... O nome sai do texto do `.stf`; uma peça que não existe para aquele corpo usa a do `Nor` ou fica de fora (aparece no log) |
| Hair Color (Brown, Black, Blonde, Red, White, Grey Brown) | texturas `<Peça>_<cor>_LG.tga` (`brown`, `black`, `blonde`, `red`, `white`, `brownGrey`) |
A mulher não tem bigode nem barba na demo. A ligação "texto da lista → arquivo" é a óbvia pelos nomes dos arquivos; o original não guarda
essa tabela num lugar que eu tenha achado.

### Animação (v20)
A animação parada **repete** (o tempo dá a volta no fim do clipe). Antes da correção, o relógio do jogo já tinha passado dos 6,67 s do clipe
quando o menu aparecia, e os personagens ficavam **congelados** na última pose. Conferido no wine: entre dois quadros, 7.670 pixels do
jogador mudam em 1,5 s.
O céu do menu é uma "parede" de 188 × 74 unidades em z = 100, com a metade de nuvens do `PS_MainMenu01.tga`. Os campos do material são
iguais aos de outro `BASE_2X`, então ela é reconhecida pela geometria (lote plano e enorme; o log diz quantos achou), e a coordenada U anda.

## Criação de personagem: fórmulas medidas
Dois prints seus do original: todos os atributos em 5; e ST, PE, EN, CH, IN em 6 com AG e LK em 5.
| Derivado | Fórmula | 5/5 → 6/5 no print |
|---|---|---|
| Action Points | 20 + AG | 25 → 25 |
| Carry Weight | 25 + 25 × ST | 150 → 175 |
| Combat Sequence | 2 × PE | 10 → 12 |
| Critical Chance | LK % | 5% |
| Healing Rate | EN | 5 → 6 |
| Melee Damage | + ST | +5 → +6 |
| Perception Range | 40 + 3 × PE | 55 → 58 |
| Skill Rate | 5 + 2 × IN | 15/1 → 17/1 |
| Throwing Range | 3 × ST | 15 → 18 |
| Hit Points | 100 fixo no início | 100/100 nos dois |

Perícias:
- Marcar uma soma **+20 na base**.
- Firearms e Melee ganham 10 × AG.
- As outras somam três atributos. Os dois prints batem exatamente, mas não bastam para separar **quais** são os três de cada perícia
  (`chargen.cpp` diz o que é hipótese).
- A fórmula de AP (20 + AG) também precisa de um print com AG diferente de 5.

## Salvar e carregar (v20)
Um jogo salvo, `saves\quicksave.f3vbsave` ao lado do `.exe`, em formato **próprio** do F3VB (texto `chave=valor`). O `.sav` do original ainda
não foi decifrado. Ele guarda o personagem inteiro (com a aparência), o mapa e a câmera. Use Save/Load/Delete em `Options_Load` (tecla `L` no
mapa) ou `F5`/`F9`. Ainda não guarda a posição do jogador (ele ainda não anda), os itens nem o estado dos scripts.

## Como a interface é desenhada
- **Tela virtual de 1024×768**, escalada para a janela mantendo 4:3. O **3D ocupa a janela inteira** em qualquer proporção ("Hor+": em 16:9
  aparece mais dos lados, nada é cortado).
- Nove fatias por widget. Textura que não existe não é desenhada. Cor do fragmento: RGB ×2, alfa como está.
- **Cor dos textos:** D3DCOLOR (bytes B, G, R, A) com RGB ×2 e alfa como está. Medido: o "None" da criação (`18687c60`) sai (134, 114, 47),
  e o print mede (135, 114, 47).
- **Ordem dentro de um botão (v20):** primeiro as imagens de estado (`BtnUp`/`BtnDown`), depois os rótulos. Na ordem do arquivo o
  "None / AGE 21 / MALE" da criação ficava escondido debaixo do botão.
- Rótulos de uma linha são **cortados** no retângulo, como no original ("DEFAUL", "CANCE").

## Como o mapa é desenhado
Como no v19 (luz toda pré-calculada: `textura × cor de vértice × 2 × lightmap × 2`, `ALPHABLEND` numa segunda passada). Uma correção do v20: o
×2 da primeira unidade só vale para materiais com `_2X` no nome. `NO_TEXTURE` fica ×1, e as alavancas do menu deram 75 contra 78 no print.

## Verificação (v21)
- `F3VB.exe --autoteste` dá **OK em tudo** no wine:
  - vídeos;
  - menu: modelos, animação, vagão chegando, nuvens e música com pico ~2650;
  - criação de personagem;
  - 8 mapas reais: modelos, pés no chão, andar e combate.
- `F3VBCheck --demo <pasta> --tudo` roda:
  - 119 scripts na VM, com 13.028 funções (9 dão erro quando chamadas fora do diálogo, nos scripts 10 Krieger e 13 Ike; a investigar);
  - 40 diálogos percorridos, com 2.675 falas;
  - todos os personagens de todos os mapas com as 6 animações e com chão;
  - 113 sons válidos.
- O relatório também lista as **natives que ainda não fazem nada no jogo**. É a lista do que falta para os NPCs "viverem" pelos scripts:
  `A_PlayAnim`, `A_MoveToPosition`, `A_FloatingSpeechText`, `AttachSound`, times/esquadrões, `SetValue`...

## O que ainda NÃO existe (honesto)
- Combate por turnos (o original alterna; o v21 tem o tempo real que você pediu) e pontos de ação gastos de verdade.
- Itens no inventário e Pip-Boy funcional.
- Fala flutuante dos scripts (`A_FloatingSpeechText`) e scripts de comportamento rodando a cada quadro (o nome sob o mouse já existe).
- O formato `.sav` original.
- Sons posicionais.
- A variante de `.b3d` do console da Mesa.

## Hipóteses a conferir com os seus prints
1. RESOLVIDO no v21 com o seu print AG/LK 6: AP = 20 + AG (26), Crítico = LK (6%). Security, Sneak e Steal batem (16, 16, 17), e
   Firearms/Melee = base + 10 × AG (140). Ainda não dá para separar **quais** de ST, PE, EN, CH e IN entram em cada perícia diplomática
   ou científica, porque os prints só mudam esses cinco juntos.
2. Os mostradores HEALTH/FATIGUE/EVASION marcam `000` como no print, então não mostram os valores de verdade.
3. Texto da lista de aparência → arquivo da peça (nomes óbvios).
4. `ECAM` = (xmin, zmax, xmax, zmin) do alvo da câmera.
