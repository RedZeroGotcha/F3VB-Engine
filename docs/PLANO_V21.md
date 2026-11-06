# Plano do v21: bugs do v20 + gameplay de verdade

Base: as suas 11 imagens e os dois arquivos de 03/11/2026 (`build_v20.txt` e `f3vb_log_11-03-26_19-54-28.txt`). Também entra o que ficou
faltando no v20 (`docs/CONFERENCIA_V20.txt`). Regra: tudo é testado primeiro na sandbox (wine) e só vem para o PC no fim.

## 0. Achados lendo o log e o build
- **O build no MSVC deu certo**: 62/62 alvos, mais o `F3VBVideo32.exe`. É a primeira compilação do v20 no MSVC.
- **O log é do pacote ANTERIOR**, o primeiro zip do v20. Ele não tem a linha "lote(s) ... parede de céu", que só existe no zip corrigido.
  O zip novo foi gravado em `M1\f3vb-starter-v20.zip`, mas a pasta `M1\f3vb-starter-v20\` foi extraída do primeiro. Por isso as animações
  aparecem paradas: a correção (animação repetindo + nuvens andando) estava no zip, não no `.exe` que rodou.
  - No v21: **cada pacote com nome próprio** (`f3vb-starter-v21.zip`, `-v21b`...) e **nunca** sobrescrever um zip já entregue.
  - A 1ª linha do log vai trazer a **data/hora da compilação** e um identificador do pacote, para sabermos qual `.exe` rodou.
- **Título da janela quebrado:** "demo tÃ©cnica". O texto com acento no código-fonte foi compilado sem `/utf-8` no MSVC. Corrigir com `/utf-8`
  no CMake (e conferir os outros textos com acento).
- **Acentos quebrados no log:** "n�o", "Voc�". O log recebe texto cp1252 (o das mensagens) misturado com UTF-8. Gravar tudo em UTF-8.
- `build.bat` ainda fala "v19" e manda usar o `jogar.bat` (velho). Atualizar as mensagens.

## 1. Bugs do v20 (dos seus prints)
| # | Bug | Causa provável | Correção planejada | Como conferir |
|---|---|---|---|---|
| 1 | **Som dos vídeos de abertura não sai** no Windows (no wine sai) | O DirectSound do Bink precisa de uma janela (`SetCooperativeLevel`); o ajudante é um processo sem janela, e passamos 0 | O ajudante cria o `IDirectSound` com a janela do jogo (HWND passado na linha de comando) e entrega ao Bink (`BinkSetSoundSystem(BinkOpenDirectSound, lpDS)`). Plano B: o Bink entrega o som decodificado e ele sai pelo **nosso mixer** (sem DirectSound) | log do ajudante: sistema de som, volume, amostras tocadas; teste no wine com o som gravado em arquivo |
| 2 | **Nenhuma animação no menu** (3 fotos iguais) | Era o `.exe` antigo (ver item 0). No zip corrigido: animação repete e nuvens andam (medido no wine) | Conferir de novo com o pacote v21. O log passa a dizer "animação: N atores, tempo do clipe X s, laço sim" | dois quadros comparados no wine e no log |
| 3 | **Faixa cinza à esquerda do menu** (o "limbo" fora do mapa) | Em 16:9 o 3D abre mais para os lados ("Hor+") e mostra o que o original (4:3) nunca mostra | No menu, **o FOV horizontal fica igual ao do 4:3** (corta em cima e embaixo em telas largas, "Vert-"), ou a câmera recua/fecha até não sair do mapa. Escolher o que ficar igual ao seu print do original | print do F3VB em 16:9 e 4:3 ao lado do original |
| 4 | **Limbo também no jogo** (print do Junktown com a área azul-cinza à esquerda) | O alvo da câmera fica no `ECAM`, mas a borda da **imagem** passa do mapa | Limitar a câmera pela área **visível** (os 4 cantos da tela projetados no chão têm de cair dentro do mapa), não só pelo ponto central | andar até as 4 bordas e capturar |
| 5 | **"Você não removeu esse cinza"** na arma 2 da barra | No v20 eu imitei o cinza que aparece no print do original (`9db4e32a`) | **Remover** o cinza, como você pediu; a arma 2 fica normal (apagada só se não houver arma, sem a camada cinza) | print da barra |
| 6 | **Personagens flutuando / sem pisar no chão** (suspeita) | Entidades e jogador usam o `y` gravado no `.map` (quase sempre 0) e não a altura real do chão | **Altura do chão por raio** contra os triângulos do mapa (`.8tre`), feito ao carregar e a cada passo; os pés (osso mais baixo da pose) encostam no chão. Medir a diferença pé/chão de cada entidade | log com "entidade X: chão y=..., pés y=..., diferença ..."; aviso acima de 0,1 |
| 7 | Laterais 16:9 da criação de personagem mostram o menu 3D atrás | A tela é 4:3 e o fundo 3D continua | Faixas pretas (ou o fundo da tela estendido) nas laterais das telas 2D cheias | print |

## 2. Logs avançados (pedido seu)
- **Relatório de carga por mapa**, no fim de cada carregamento. Cada linha diz `OK`/`FALHA` com o motivo:
  - geometria (nós, triângulos, lotes sem estado);
  - texturas (ok/faltando, com nomes);
  - lightmaps;
  - entidades (modelo, partes, animação, altura do chão);
  - scripts (abriram? publics ok?);
  - sons e música (achados? tocando?);
  - gatilhos e pontos de entrada.
- **Relatório de render a cada N s:**
  - quadros por segundo e tempo do quadro;
  - triângulos e chamadas de desenho;
  - atores animando: quantos e se o tempo andou;
  - erros do OpenGL (`glGetError`).
- **Áudio:** cada música/efeito/ambiente pedido. O log diz se o arquivo foi achado, o formato, se tocou e o volume final, e mostra o
  **pico do mixer** (para provar que sai som).
- **Física/chão:** por ator, chão, pés e diferença; caminhos de navegação válidos.
- **Vídeos:** o ajudante escreve o próprio log (sistema de som, quadros, tempo, erros do Bink).
- Comando **`F3VB.exe --autoteste`**: sem ninguém clicar, abre vídeos → menu → criação → cada mapa da demo. Ele grava prints e mede
  automaticamente (animação mexendo? som com pico? personagens no chão?), escreve `autoteste_<data>.txt` e sai. É o mesmo teste que eu rodo
  no wine, e no seu PC ele vira a prova.

## 3. Gameplay de verdade
1. **Jogador anda**:
   - clique no chão → caminho pela rede de navegação do mapa (`nav_nodes`) ou pela grade do `.rle`, com A*;
   - animação de andar/correr (`HumM_Def_Una_Walk_01`... da demo), girando para a direção do passo e sempre no chão (item 6);
   - câmera seguindo (opção).
2. **Colisão**: grade de bloqueio do `.rle`, mais as entidades sólidas; não atravessar paredes nem NPCs.
3. **NPCs vivos**:
   - nome sob o mouse;
   - **fala flutuante** (o "barks" dos scripts, `FloatingText`/`DisplayStringRef`);
   - scripts de comportamento rodando a cada quadro (`OnUpdate`, `OnCreate`, `OnLoad`, `SecondsPassed`);
   - natives que faltam (as do levantamento: `SetValue`, `GetCurrentHP`, `AttachSound`, `A_PlayAnim`...).
4. **Portas e gatilhos**: abrir portas (`.DOR`), gatilhos de região/script, **viagem entre mapas** (gatilho tipo 2 → outro mapa / ponto de
   entrada).
5. **Inventário com itens**: o que o protótipo de cada um carrega (`EEOV` inventário), arrastar entre mãos/corpo, armas na barra de ações
   (troca o "PUNCH" pelo modo da arma).
6. **Combate básico por turnos (AP)**: atacar com o desarmado/arma da barra, chance de acerto pelas perícias, dano, HP real nos mostradores
   (hoje fixos em 000). Seguir o que os scripts e o `.stf` dizem, sem inventar regra de Fallout 2.
7. **Pip-Boy**: telas com dados reais (hora do `[World]` do `sys.ini`, mapa da área, anotações).
8. **Sons posicionais** (`MapSound` do mapa) e ambiente por mapa.

## 4. O que faltou no v20 (entra no v21)
- `DS_Mesa_Console.b3d`: variante de `.b3d` ainda não lida (e `Cow`, num mapa de teste).
- Opções da tela de vídeo de verdade: monitor, sombras, luzes dinâmicas, filtro, detalhe de textura e efeitos.
- Fórmula de AP e quais 3 atributos entram em cada perícia. **Preciso de um print da criação do original com AG e LK diferentes de 5.**
- Formato `.sav` do original. O nosso jogo salvo vai guardar também a posição do jogador, os itens e o estado dos scripts.
- Destaque dos botões (aditivo), ainda sem print com o mouse em cima para medir.
- Mostradores HEALTH/FATIGUE/EVASION com os valores reais (junto com o combate).

## 5. Ordem de trabalho
1. Item 0, os bugs 1 a 7 e os logs avançados com `--autoteste`. Tudo conferido no wine, comparando lado a lado com o original rodando.
2. Chão e colisão, jogador andando e câmera seguindo.
3. NPCs (nomes, fala flutuante, scripts de comportamento), portas, gatilhos e viagem entre mapas.
4. Inventário e armas; combate por turnos.
5. Pip-Boy, sons posicionais e opções de vídeo.
6. Docs, `CONFERENCIA_V21.txt` (cada pedido e imagem com o status), testes e pacote `f3vb-starter-v21.zip` com nome novo. Backups no PC
   durante o trabalho, em `M1\v21-trabalho\`.
