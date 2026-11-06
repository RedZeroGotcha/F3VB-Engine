# RPGGL — build de teste 0.2  ·  *Depois da Queda do Céu*

RPG pós-apocalíptico de mundo aberto, 3D, 100 % C++ e OpenGL 1.x (pipeline fixo).
**Sem dependências externas**: só DLLs que já vêm no Windows (`opengl32`, `gdi32`, `user32`, `winmm`; XInput é carregado dinamicamente).

## Compilar (Windows)
```
build.bat            :: Release  ->  dist\RPGGL.exe  (+ dist\saves\)
build.bat debug
build.bat clean
```
Funciona no *Developer Command Prompt* (Ninja) ou em prompt comum (gerador do Visual Studio). Runtime estático (`/MT`).
Os saves ficam em **`saves\` ao lado do .exe**; opções em `settings.cfg` (mesma pasta).

## Controles (F1 mostra a lista no jogo)
| Ação | Teclado / Mouse | Controle (XInput) |
|---|---|---|
| Mover / olhar | WASD / mouse | analógicos |
| Atirar / mirar | botão esq. / dir. | RT / LT |
| Pular / correr / furtivo | Espaço / Shift / C | B / clique L3 / D-Pad ↓ |
| Interagir (falar, abrir, dirigir) | E | A |
| Recarregar / coronhada / injetor | R / F / H | X / D-Pad → / D-Pad ↑ |
| Armas | 1-4 ou roda | LB / RB |
| Câmera 1ª ↔ 3ª pessoa | V | clique R3 |
| Diário (status, itens, missões, mapa) | Tab / I | Y |
| Salvar rápido / carregar último | F5 / F9 | D-Pad ← |
| No veículo | W/S acelera-freia, A/D esterça, Espaço = freio de mão, E sai | RT/LT + analógico, B freio |
| Pausa / tela cheia / debug | Esc / F11 / F3 | Start |

## O que tem na 0.2
Mundo de 16,4 × 16,4 km (256×256 chunks) em streaming multi-thread · ciclo dia/noite · saves `.sav` com miniatura ·
NPCs com IA paralela · 3 tipos de humanos hostis/neutros + 5 criaturas · 2 veículos dirigíveis · fissuras e estilhaços ·
fome/sede/radiação · 6 missões · diálogos · comércio · mapa · 1ª e 3ª pessoa · gamepad ·
**ragdoll de corpos rígidos** (cápsulas + juntas, XPBD) na morte de humanos e criaturas.

## Testes (qualquer SO, sem janela)
```
cmake -S . -B build -DRPGGL_BUILD_TESTS=ON && cmake --build build && ./build/rpggl_headless
bash tests/run_tests.sh      # ASan+UBSan, TSan e fumaça do render/HUD (usa tests/stubs)
```
