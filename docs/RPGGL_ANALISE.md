# Análise do RPGGL v0.2 (a "segunda engine") e plano de reaproveitamento no F3VB

**Origem:** `RPGGL_v0_2_src.zip`, enviado pelo autor do F3VB (escrito por ele com outro Claude). **Licença:** nenhuma declarada (ver `docs/LICENCAS_E_DIREITOS.md`, seção 5).
**Onde está no projeto:** `engine_test/rpggl/` (cópia fiel, nada alterado) + `engine_test/README.md`.
**Rótulos:** *medido* = rodei aqui; *lido* = li o código/documentação; *hipótese* = ainda a confirmar.

## 1. O que é (lido + medido)
RPG de mundo aberto em C++14 e **OpenGL 1.x (pipeline fixo)**, ~5.200 linhas, **sem dependências externas** (só `opengl32`, `gdi32`, `user32`, `winmm`; XInput via `LoadLibrary`).
Sem assets: mundo procedural a partir de uma semente, sons sintetizados, fontes do Windows. O núcleo (`world`, `game`, `save`, `ui`, `ragdoll`) **não inclui Windows nem OpenGL**.

**Teste (medido):** compilei o núcleo + `tests/headless.cpp` aqui (g++, Linux) e rodei: **68 verificações `ok:`, "TUDO OK (0 falhas)", 8 s**, incluindo streaming de chunks, IA, combate, quests, saves/CRC, veículos e o **ragdoll** (humano e quadrúpede estáveis, adormecem, nenhuma junta atravessa o terreno, desvio máximo das juntas 1,3–1,8 cm). Isso agora roda junto da suíte do F3VB (`tests/test_engine_test.py`).
**Limite herdado (lido):** a camada Win32/OpenGL/áudio foi só compilada contra *stubs*, nunca em Windows real (mesmo status do nosso visualizador antes do v11).

## 2. Módulo a módulo: o que aproveitar
| Módulo (linhas) | O que faz | Valor para o F3VB | Como entra | Riscos |
|---|---|---|---|---|
| `sys.h` (152) | `Mutex`/`CondVar`/`Thread` em Win32 puro + `ThreadPool` com fila ALTA e BAIXA e `ParallelFor` | **Alto** | Copiar para `src/engine/` como base de carregamento assíncrono (modelos, texturas, mapas) e IA paralela | É Win32-only; testes no Linux usam `tests/stubs/windows.h`. Trocar por `std::thread` se quisermos portar |
| `common.h` (96) | matemática (`Vec3`...), hash/ruído determinístico, RNG local, cores | Médio | Unificar com o `vmath.h` do visualizador (hoje há dois `Vec3`/`Mat4` diferentes) | Nomes globais (`Vec3`) colidem com os do viewer se linkar junto: usar namespace |
| `world.*` (790) | terreno e POIs como **funções puras da semente**; chunks 64 m; streaming por proximidade; colisão; *raycast* | Médio | O **mecanismo** (chunks + streaming + pool + colisão) serve; o **gerador procedural** é trocado por um carregador dos dados do Van Buren (mapas/tiles) | Depende de decodificar os mapas `8TRE`/`EMAP` e o terreno dos Tiles (ainda em aberto) |
| `ragdoll.*` (277) | corpos rígidos com cápsulas + juntas (esfera com cone/torção, dobradiça), **XPBD** em 8 subpassos; humano (13 corpos/12 juntas) e quadrúpede (12/11); impulso de dano; sono | **Alto** | Ligar cada corpo a uma junta do esqueleto F3M (`Pelvis`, `Spine_*`, `Head`, `*UpperArm`, `*Forearm`, `*Thigh`, `*Calf`, `*Foot`) para a **malha do personagem** cair em ragdoll, em vez de desenhar cápsulas | O autor disse que ainda está mal implementado; o roadmap dele lista colisão entre membros e hit reactions. Os testes de estabilidade passam; a **qualidade visual** só se vê com a malha |
| `game.*` (1.400) | itens, jogador, NPCs + **IA** em máquina de estados (`IDLE/WANDER/ALERT/COMBAT/FLEE/DEAD`), combate, **veículos** (modelo arcade), fissuras, quests, diálogo, comércio | Médio/Alto | IA, combate e veículo viram **módulos reaproveitáveis** (`src/engine/ai`, `vehicle`). Quests/diálogos/itens do Van Buren vêm dos **scripts AMX** (a VM já existe), não do código novo | O conteúdo (itens, 6 missões, lore) é do RPGGL, não do Van Buren: não misturar |
| `save.cpp` (297) | `.sav` com cabeçalho legível, miniatura, seções etiquetadas, **CRC32**, gravação assíncrona em `.tmp` + rename atômico, `settings.cfg` | **Alto** | Copiar quase direto para os saves do F3VB | Ajustar seções ao estado do F3VB |
| `ui.cpp` (157) + `hud.cpp` (479) | menus (lógica separada do desenho), HUD, compasso, diário, mapa, diálogos; fontes via `wglUseFontBitmaps` | **Alto** | Menus e HUD do motor e **UI do visualizador de mapa** (hoje o nosso HUD é só texto). A lógica já é testável sem janela | Fonte do Windows ("Consolas"), não redistribuída; sem texturas 2D ainda (o próprio autor avisou) |
| `audio.cpp` (157) | sons 100% sintetizados + mixer `waveOut` (24 vozes) em thread própria | Médio | O **mixer** serve; os sons do Van Buren (`Sounds`) precisam de decodificador de `.wav` próprio | Áudio posicional está no roadmap |
| `platform.cpp` (208) + `render.cpp` (572) | janela, contexto GL, teclado/mouse/XInput, laço principal; render em GL 1.x (céu, neblina, chunks em vertex arrays, personagens articulados, veículos) | Médio/Alto | Unificar com o `viewer_main.cpp` numa camada `src/engine/platform`; o render de personagem trocar "articulado" por **malha F3M com skinning** | Duas camadas de plataforma hoje; unificar com cuidado |
| `tests/*` | headless, ASan/UBSan, TSan, fumaça de render contra stubs de GL | **Alto** | Manter o estilo: tudo novo do motor nasce com teste headless | |

## 3. Como o RPGGL ajuda no que o autor quer testar (mapas, NPCs, criaturas, VM)
1. **Visualizador de cena/mapa (v13.3):** o laço de plataforma + câmera + streaming do RPGGL vira o "Scene Viewer": carrega F3M de personagens e props, posiciona numa grade/mapa e liga a física.
2. **Criaturas e humanos:** com o conserto dos nomes de ossos e das famílias de animação (v13), o esqueleto humano de 30 juntas pode ser pareado ao ragdoll humano de 13 corpos.
3. **VM:** a VM AMX (`src/amx/`) roda os scripts do Van Buren; os *hosts* de `natives` (print, diálogo, quests) entram no lugar do `game.cpp` do RPGGL.
4. **Mapa real ou menu 3D da intro:** depende do decodificador de mapas (`8TRE`/`EMAP`) e do terreno dos Tiles. Sem ele o Scene Viewer mostra o personagem e os props, mas não o mapa.

## 4. Marcos do v13 em diante
| Marco | Entrega | Estado |
|---|---|---|
| **13.1** (este pacote) | RPGGL no repositório com teste headless; correções dos humanos (nomes de ossos, famílias de animação); ferramentas `b3dmat` e `pack-sample`; análise e plano; registro de licença | **feito e testado aqui** |
| 13.2 | `src/engine/`: copiar `sys.h`, `common.h`, `save`, `ragdoll` com namespace `f3e`, unificar a matemática com o viewer; testes headless | a fazer |
| 13.3 | Scene Viewer: janela + câmera + F3M com skinning + HUD do RPGGL; humanos com animações `HumM_*` | a fazer |
| 13.4 | Ragdoll no esqueleto F3M (humano/cão) | a fazer |
| 13.5 | Decodificar mapas (`8TRE`/`EMAP`) e terreno dos Tiles com amostras enviadas pelo autor; abrir um mapa | a fazer (depende de arquivos) |
| 13.6 | Hosts dos `natives` da VM no motor; intro/menu 3D | a fazer |

## 5. Pendências e cuidados
- **Licença do RPGGL:** o autor deve adicionar um `LICENSE` ao `engine_test/rpggl/` quando decidir. Enquanto isso é "todos os direitos reservados".
- **Autoria com IA:** o código foi escrito com apoio de IA; a situação de direitos autorais de código gerado por IA varia por país. Não é parecer jurídico: se o projeto for publicado, vale ler a política do serviço usado e a lei local.
- **Lore e conteúdo:** `LORE.md`, nomes de missões, itens e personagens do RPGGL são do autor e **não** são Van Buren. Não misturar nos arquivos do F3VB.
- **Código de terceiros dentro do RPGGL:** nenhum encontrado (varredura por stb/glad/GLFW/SDL/miniaudio/Bullet/Jolt automatizada em `test_engine_test.py`).
