# engine_test: a "segunda engine" de teste (v13)

Aqui fica o **RPGGL v0.2**, o mini-projeto de RPG em C++/OpenGL 1.x que o autor do F3VB escreveu (com outro Claude) para testar
mecânicas: mundo em chunks com streaming multi-thread, IA de NPCs, combate, veículos, ragdoll de corpos rígidos (XPBD), saves com CRC32,
menus/HUD e áudio sintetizado. **O código em `rpggl/` está exatamente como foi enviado** (nada foi alterado; mudanças viram patches documentados
em `docs/RPGGL_ANALISE.md`).

| Pergunta | Resposta |
|---|---|
| De quem é? | Do autor do F3VB (escrito com ajuda de IA). Sem arquivo de licença no zip original. |
| Tem código de terceiros? | Não: só APIs do Windows (`opengl32`, `gdi32`, `user32`, `winmm`, XInput carregado dinamicamente). Ver `docs/LICENCAS_E_DIREITOS.md` |
| Tem asset do jogo? | **Não.** Mundo, sons e texturas são procedurais/sintetizados |
| Como testar sem janela? | `py -3 -m unittest tests.test_engine_test` (em `tools\py`) ou `rpggl\tests\run_tests.sh` (ASan/UBSan/TSan, Linux/macOS) |
| Como compilar o jogo (Windows)? | `rpggl\build.bat` (Developer Command Prompt). A camada Win32/OpenGL/áudio foi só compilada contra *stubs*: nunca rodou em Windows real |
| Para que serve no F3VB? | Fonte de mecânicas para o motor (ragdoll, veículo, IA, saves, UI, pool de threads) e **base do visualizador de mapa do v13**. Plano: `docs/RPGGL_ANALISE.md` |

Pendente do autor: **escolher e adicionar um arquivo `LICENSE`** (hoje vale "todos os direitos reservados", o que não impede o autor de usar, mas
impede qualquer outra pessoa de reaproveitar). A lore (`rpggl/docs/LORE.md`) é propriedade intelectual do autor e **não** faz parte do F3VB: não usar
nomes, personagens ou textos dela em nada que seja distribuído como F3VB sem decisão do autor.
