# Arquitetura técnica

```
src/common.h   matemática, hash/ruído determinístico, RNG local, cores
src/sys.h      Mutex/CondVar/Thread (Win32 puro) + ThreadPool (fila ALTA e fila BAIXA, ParallelFor)
src/world.*    terreno e POIs (funções puras da semente), geração de chunk, streaming, colisão, raycast
src/ragdoll.*  ragdoll de corpos rígidos: cápsulas + juntas (esfera/dobradiça), XPBD
src/game.*     itens, jogador, NPCs + IA, combate, veículos, fissuras, quests, diálogo, comércio, câmera
src/save.cpp   saves .sav (cabeçalho+miniatura+seções+CRC32), gravação assíncrona/atômica, settings.cfg
src/ui.cpp     lógica de menus (sem desenho)
--- camada Windows/OpenGL ---
src/render.cpp céu, neblina, luz, chunks (vertex arrays), personagens articulados, veículos, efeitos
src/hud.cpp    HUD, compasso, menus, Diário, mapa, diálogos (fontes do Windows via wglUseFontBitmaps)
src/audio.cpp  sons 100% sintetizados + mixer waveOut em thread própria (+ motor com pitch variável)
src/platform.cpp janela, contexto GL, teclado/mouse/XInput, laço principal
```
O núcleo (`world/game/save/ui`) **não inclui Windows nem OpenGL**: roda e é testado headless.

## Mundo e chunks
- 256×256 chunks de 64 m = 16 384 m. Cada chunk: 16×16 células (4 m), malha de terreno + construções + vegetação em **um** vertex array.
- `TerrainH(x,z)`, `PoiAt(cx,cz)`, `RoadFactor`, `RadAt` são **funções puras**: o mundo inteiro "existe" sem memória; só ~120 chunks ficam na RAM.
- POIs: aplainamento do terreno por mistura suave com a altura do centro; fixos da história + procedurais com regra de não-adjacência.
- Gerar um chunk custa ≈ 0,8 ms (medido com ASan) e ≈ 2 200 vértices.
- Streaming: `World::Update` pede chunks em ordem de proximidade ao pool (fila BAIXA), publica com `state=1` (atômico), descarrega a >R+1.

## Multi-thread
| Thread | Trabalho |
|---|---|
| principal | entrada, jogador, veículos, aplicação dos resultados da IA, GL |
| workers (N-1, 2..8) | geração de chunks (BAIXA); IA dos NPCs via `ParallelFor` (ALTA); gravação de saves |
| áudio | mixer de 24 vozes + vento + motor |
A IA só **lê** o mundo e **escreve no próprio NPC**; efeitos (tiro, dano) saem em campos `out/fire*` e a thread principal aplica depois do *join*. Validado com **ThreadSanitizer: 0 avisos**.

## Saves
`SaveNNN_Nome_Local_LvX_HHHhMMm.sav`, `Quicksave1-3` e `Autosave1-3` rotativos. Cabeçalho legível sem carregar o jogo (nome, nível, local, tempo, data, miniatura 256×128 capturada **antes do HUD**).
Corpo em seções etiquetadas (`PLYR INVT QSTS TIME WRLD NPCS CAMR VEHS`), seções desconhecidas são ignoradas (compatibilidade futura). CRC32; gravação em `.tmp` + rename atômico numa thread do pool.
Só o **delta** é salvo (mortos, saqueados, NPCs feridos, veículos, locais descobertos): o mundo é regenerado da semente. Mortos/saqueados **respawnam** após 72 h/96 h de jogo.

## Veículos
Modelo arcade: aceleração/freio/arrasto, esterço dependente da velocidade (`tan(steer)·v/L`), 4 pontos de amostra do terreno (arfagem/rolagem), saltos quando o chão some, colisão por 2 círculos contra os colisores, dano por impacto/queda, atropelamento, combustível, explosão. Persistem em `VEHS` e despawnam por distância.

## Ragdoll (corpos rígidos, XPBD)
Sem partículas e sem Verlet: cada osso é um **corpo rígido** (posição, quaternion, massa, tensor de inércia) com **colisor de cápsula**.
- **Humano:** 13 corpos (pélvis, tronco, cabeça, braço/antebraço ×2, coxa/canela/pé ×2) e 12 juntas. **Criaturas (cães, rastejantes):** 12 corpos e 11 juntas (coluna, pescoço, cauda, 4 patas).
- **Juntas:** *esfera* (âncora posicional + limite de **cone** + limite de **torção**) em coluna, pescoço, ombros, quadris e tornozelos; *dobradiça* (alinhamento de eixo + limites angulares) em joelhos e cotovelos, que só dobram no sentido certo.
- **Solver XPBD em subpassos** (8 por quadro, 2 iterações): constraints posicionais e angulares com *compliance*; correção angular limitada por passo para não "estalar"; âncoras fechadas ao fim de cada iteração e contatos por último, então nada termina penetrado.
- **Contatos:** pontos da cápsula (extremidades + meio) contra o terreno e contra os colisores do mundo, com atrito e inelasticidade; amortecimento extra ao assentar e **sono** automático (~1,5 s).
- **Impacto:** a morte aplica impulso linear e angular (braço de alavanca) conforme a origem do dano: tiro, golpe, atropelamento ou explosão.
- **Render:** cada corpo vira uma malha de cápsula orientada pelo quaternion.
- **Paralelismo:** cada ragdoll só lê o mundo, então vários são simulados juntos via `ParallelFor`. O cadáver continua revistável (a posição do NPC segue a pélvis).

## Testes
`tests/headless.cpp` (15 blocos, inclui ragdoll: streaming, IA, combate, quests, saves/CRC, veículos, fissuras, sobrevivência, geração) · ASan+UBSan, TSan · `render_smoke.cpp` executa render/HUD contra stubs de GL.
> **Limite honesto:** a camada Win32/OpenGL/áudio foi **compilada e executada contra stubs**, não em um Windows real.
