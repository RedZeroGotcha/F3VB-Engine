# Changelog
## 0.2
- **Ragdoll** de corpos rígidos (cápsulas + juntas de esfera/dobradiça, XPBD) quando NPCs e criaturas morrem; renderizado com malhas de cápsula.
- **Corrigido (build no MSVC):** faltava `#include <mmsystem.h>` (waveOut e timeBeginPeriod), o que quebrava `audio.cpp` e `platform.cpp`; bibliotecas do Windows agora também são pedidas via `#pragma comment(lib, ...)`.
- Corrigido: variável `far` (macro do Windows) em `game.cpp`; `_WIN32_WINNT` definido para `CONDITION_VARIABLE`.
- Corrigido: entrega de Sementes Vivas já na primeira conversa com o Ancião; mensagens repetidas não se acumulam mais ("Sem munição.").
- Corrigido: o mundo não roda mais a IA com o jogo pausado.
- Testes: bloco de ragdoll (estabilidade, juntas, sono, impulso extremo, humano e criatura).
## 0.1 (primeira build)
- Conversão completa do projeto Futebol3D para RPG de mundo aberto (RPGGL).
- Mundo 16 km em chunks com streaming multi-thread; ciclo dia/noite; estradas, POIs, ruínas caminháveis.
- Saves `.sav` estilo RPG de mundo aberto (quick/auto rotativos, miniatura, CRC, assíncrono).
- Jogador 1ª/3ª pessoa, gamepad, 4 armas + coronhada, furtividade, fome/sede/radiação, atributos próprios (6).
- NPCs (colono, mercador, guarda, ancião, mecânico) e inimigos (saqueador, cão, rastejante, ressecado, brutamonte); IA paralela.
- Veículos (buggy, caminhonete), combustível, atropelamento, farol, som de motor.
- Fissuras e estilhaços; 6 missões; diálogos; comércio; mapa; Diário.
