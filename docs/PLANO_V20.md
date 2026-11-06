# Plano do v20 (escrito em 2026-10-29, depois do primeiro teste do v19 no Windows)

Base: o seu teste do v19 (`build_v19.txt` e `f3vb_log_10-29-26_21-35-21.txt`) e as 5 imagens do jogo original que você mandou.
**Mínimo para o v20 estar bom (combinado):** (1) bugs corrigidos com comandos que funcionam de verdade e (2) o `F3VB.exe` lendo a **demo original** (`resource.rht` + `data\*.grp` inteiros, sem extrair).
Os itens 3 e 4 entram na medida do possível; o resto vai para o v21.

---

## 0. O que deu errado no v19 (diagnóstico pelo seu log)

A compilação no MSVC **funcionou** (46/46, sem erros). O jogo abriu (GL 4.6, 60 FPS). O problema foi **a pasta de dados errada**, por uma sequência de três falhas minhas:

| # | O que aconteceu | Prova no log/saída | Culpa |
|---|---|---|---|
| 1 | Os comandos que eu passei **não definiam `F3VB_ASSETS_DIR`**. Sem ela, o Python usa `<projeto>\original_assets`, que no v19 **não existe** (os seus dados estão em `f3vb-starter-v12\f3vb\original_assets`). | `convert-retail`: "index.csv nao existe"; `tre-check`: traceback `FileNotFoundError` | instruções erradas + Python sem mensagem amigável |
| 2 | O `import-loose` **criou** essa pasta vazia (`f3vb-starter-v19\f3vb\original_assets`) só com `Fonts` e `English.stf`. | "copiado F:\...\f3vb-starter-v19\f3vb\original_assets\RETAIL\Fonts\..." | o comando deveria recusar uma pasta sem `index.csv` |
| 3 | O `F3VB.exe` foi aberto direto (não pelo `jogar.bat`), procurou `original_assets` subindo pastas, **achou a pasta falsa** e aceitou só porque ela existe. Resultado: fontes e textos ok, mas **0 imagens, 0 telas `.int`, 0 sons, 0 mapas** → tela preta. | linhas 2, 4, 7-24 do log | o exe não valida a pasta nem mostra um erro na tela |
| 4 | Na tela preta, cada `Enter` tentou "New Game" e repetiu o aviso. | linhas 27-35 | menu sem tela não deveria aceitar atalhos |
| 5 | `build.bat` não existe na raiz `f3vb\` (só em `viewer\`). | "'build.bat' não é reconhecido" | faltou um atalho na raiz |

**Para testar o v19 hoje, se quiser (antes do v20):** apague a pasta falsa `F:\F3VB Engine\M1\f3vb-starter-v19\f3vb\original_assets` e rode:
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v19\f3vb\tools\py"
$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
py -3 -m f3vb convert-retail --maps
py -3 -m f3vb tre-check
py -3 -m f3vb import-loose "F:\F3VB Engine\Demo\Demo"
```
Depois abra pelo `viewer\jogar.bat` (ele define a pasta certa), não pelo `F3VB.exe` direto.

---

## 1. Correção dos bugs e comandos certos de verdade (OBRIGATÓRIO)

### 1.1 Uma configuração só, gravada, para todas as ferramentas
- Novo arquivo **`f3vb.ini`** na raiz do projeto (fora do git), com `demo=` (pasta da demo original) e `assets=` (pasta `original_assets`, opcional).
  Lido pelo Python (`layout.py`), pelo `F3VB.exe`, pelos visualizadores e pelos `.bat`. Ordem: argumento de linha de comando > variável de ambiente > `f3vb.ini` > procura automática.
- Comando **`py -3 -m f3vb setup "<pasta da demo>" [--assets "<original_assets>"]`**: confere as duas pastas e grava o `f3vb.ini`. Depois disso **nenhum comando precisa de `$env:`**.
- O `F3VB.exe`, sem configuração, abre uma **janela de escolha da pasta da demo** (diálogo nativo do Windows, `shell32`, nada externo) e grava no `f3vb.ini`.

### 1.2 Validar as pastas, nunca aceitar uma pasta "parecida"
- Uma pasta `original_assets` só vale se tiver `index.csv` **e** `RETAIL\Interface\INT_Interface\*.int`. Uma pasta de demo só vale com `resource.rht` **e** `data\*.grp`.
- O exe registra no log **cada candidata e o motivo da recusa**.
- `import-loose`, `convert-retail`, `tre-check`, `ui-info` e os outros: mensagem clara em vez de traceback, dizendo qual pasta foi usada e como corrigir. Nunca criam um `original_assets` novo sem querer: só o `extract-all` cria.

### 1.3 Tela de erro dentro do jogo
- Se faltar algo essencial (telas, imagens, mapa), o jogo mostra **um painel legível na própria janela**: o que faltou, em qual pasta procurou, e o comando exato para resolver. Nada de tela preta.
- Atalhos (`Enter` = New Game e outros) só funcionam se a tela correspondente existir. Os avisos repetidos são agrupados.

### 1.4 Comandos e atalhos
- `build.bat` também na **raiz** `f3vb\` (chama o de `viewer\`). `jogar.bat` na raiz.
- **`py -3 -m f3vb doctor`** e **`F3VB.exe --check`**: verificam tudo (pastas, `index.csv`, telas, sons, mapas convertidos, versão do conversor, fontes, `.stf`) e imprimem **OK/FALTA com o comando de correção**. É a primeira coisa a rodar e a me mandar.
- Revisar **cada comando de cada doc** (README, COMANDOS, PASSO_A_PASSO, JOGO) **rodando-os de verdade**, num teste automático que monta pastas falsas nos dois layouts (demo e `original_assets`) e executa as linhas exatas dos docs.

### 1.5 Testes novos que teriam pego o v19
- Teste de "pasta errada": `original_assets` vazio, sem `index.csv`, sem `.int` → o exe precisa recusar e explicar (testado em modo `--check`, sem janela).
- Teste de que os comandos dos docs rodam (item acima).

---

## 2. O `F3VB.exe` lendo a demo ORIGINAL (`.grp` inteiros) (OBRIGATÓRIO)

Objetivo: quem tem a demo aponta para a pasta dela e joga. Sem `extract-all`, sem Python, sem `original_assets`. A sua cópia extraída (`original_assets` do v12) continua sendo **minha bancada de análise e de conferência** (pelo acesso à pasta `F:\F3VB Engine`), nunca uma dependência do jogo.

### 2.1 O que já sabemos (os formatos são simples)
- **`.grp`** (verificado em 24 arquivos): cabeçalho `u32 2, u32 1, u32 nFiles`, depois `{u32 offset, u32 length}` por lump. **Sem compressão, sem nomes.** (`docs/specs/grp.md`)
- **`resource.rht`** (verificado, 7.044 entradas): nome, tipo e `(pack, índice do lump)` de cada recurso. (`docs/specs/rht.md`)
- Ou seja: para ler `Mainmenu.int` basta achar a entrada no `.rht`, abrir `data\Interface.grp`, ir ao offset e ler `length` bytes. É o que o OpenMW faz com os `.bsa` do Morrowind.

### 2.2 Peças novas em C++ (portáveis, sem dependências)
| Arquivo | O que faz |
|---|---|
| `vfs.h/.cpp` | **Sistema de arquivos virtual**: `Vfs::open_demo(pasta)` (lê `.rht` + `.grp`), `Vfs::open_extracted(original_assets)` (lê RETAIL) e `Vfs::open_loose(pasta)`. Mesma interface: `find(nome, tipo)`, `read(...)`, `list(tipo)`. Tudo que o jogo lê passa por ela. |
| `rht.cpp`, `grp.cpp` | Leitores portados do Python (`rht.py`, `grp.py`), com os mesmos limites de sanidade. |
| `mapload.cpp` | Monta o mapa **em memória** direto de `.map` + `.8tre` + `.rle` + texturas: porta para C++ o `mapfile.py`, o `tre.py` (gramática completa do v19) e o `rle.py`. O `.f3map` deixa de ser obrigatório para jogar. |
| `texcache` | Decodifica os TGA/BMP direto do `.grp` (o decodificador do v19 já existe) e mantém um **cache em disco** opcional (`%LOCALAPPDATA%\F3VB\cache`) para abrir mais rápido na segunda vez. |
| `audio` | Toca os `.wav` direto da memória (`PlaySound` com `SND_MEMORY`, `winmm`). |
| `.int`, skins, `.amx`, fontes, `.stf` | Já leem de memória no v19; só trocam a origem para a `Vfs`. Fontes e `English.stf` ficam soltos na demo e são lidos de lá. |

### 2.3 Como fica para você
```bat
build.bat
build\F3VB.exe --demo "F:\F3VB Engine\Demo\Demo"      :: ou só F3VB.exe: na 1a vez pergunta a pasta e grava
```
No menu Opções aparece **"Pasta da demo"** (trocar depois). O log diz de onde veio cada arquivo (`grp:Interface#12` ou `RETAIL\...`).

### 2.4 Como garantir que ficou igual (conferência com a sua cópia extraída)
- **`F3VBCheck`** (linha de comando): para cada recurso, compara os bytes lidos do `.grp` com o arquivo correspondente em `original_assets\RETAIL` (tamanho e hash). Esperado: **7.044 de 7.044 iguais**.
- Para os mapas: a geometria montada em C++ comparada com o `.f3map` do Python (triângulos, materiais, caixa). Esperado: os números do `tre-check` (5.925 nós, 1.928.117 triângulos, 0 sem estado).
- Testes com `.rht`/`.grp` **sintéticos** no projeto (nada da demo entra no git), incluindo arquivos truncados e lixo.

---

## 3. Corrigir as suposições (com base no jogo original)

### 3.1 Destaque dos botões
- **Hoje:** a imagem do estado `selected` é somada por cima (aditivo). Suposição minha.
- **Evidência nova:** a sua imagem do menu original mostra os botões como **placas sólidas amarelo-oliva com texto amarelo**, sem brilho somado.
- **v20:** trocar para **substituição** (desenha a textura do estado no lugar da `up`, mistura normal) e usar a cor da skin como multiplicação. O modo antigo fica atrás de uma opção de depuração, para comparar.
- Conferir também se o texto do botão vem da textura (como no print) ou de um rótulo filho.

### 3.2 Câmera
- **Evidência:** o print de jogo (o carro pegando fogo) mostra **câmera tática alta, inclinação fixa (~45-50°), visão de cima e de lado**, sem câmera livre.
- **v20:** câmera do jogo no estilo do original: inclinação fixa, giro em passos (ou suave com `Q`/`E`), zoom limitado, alvo preso ao `ECAM`, rolagem pela borda da tela e pelas setas. A câmera livre fica no `F3` (depuração).
- Conferir a hipótese do `ECAM` com essa câmera: o alvo nunca deve mostrar fora do mapa.

### 3.3 Perícias e atributos (deixar de copiar o Fallout 2)
- **Evidência forte:** o seu print do inventário traz um personagem real com **SPECIAL e as 13 perícias**:
  ST 7, PE 6, EN 5, CH 4, IN 6, AG 7, LK 5 → Barter 18, Deception 17, Firearms 110, Mechanics 15, Medic 16, Melee 116, Outdoors 15, Persuasion 18, Science 16, Security 17, Sneak 17, Steal 18, Unarmed 79.
  As fórmulas do Fallout 2 **não batem** (Barter no FO2 seria 4×CH = 16, aqui é 18; Science seria 4×IN = 24, aqui é 16). Então o Van Buren usa fórmulas próprias.
- **v20:**
  1. Procurar as fórmulas **nos dados da demo**: os 125 `INI_Config`, o `Player.amx`/scripts de criação e os protótipos `.CRT` (que têm SPECIAL e perícias de NPCs prontos para cruzar).
  2. Montar uma tabela de casos reais (este print, os NPCs dos `.CRT`) e **testar** as fórmulas contra ela (teste automático).
  3. Se faltar dado, as fórmulas que não se confirmarem ficam marcadas na tela como "provisório".
- **Peço a você (se puder):** prints da tela de criação de personagem do jogo original **com números visíveis** (o seu terceiro print é pequeno demais para ler). Cada print vira um caso de teste.

### 3.4 Layout das telas conferido com os prints
- **Criação de personagem:** o original mostra o **modelo 3D do personagem** à direita e o painel "Stats" embaixo; conferir posições e fontes com a nossa prévia.
- **Inventário:** grade de itens à esquerda, slots HEAD/BODY/HAND1/HAND2, painel de atributos e modelo 3D. A barra de ações tem **log de mensagens** (vermelho/verde) à esquerda, contadores, arma no centro e os 6 botões à direita.
- **Menu principal:** o print tem **7 botões** (PLAY TUTORIAL, START NEW GAME, LOAD SAVED GAME, PLAY WITH A FRIEND, GAME OPTIONS, PLAY MOVIES, EXIT GAME). O nosso `Mainmenu.int` tem **6** (`NewGameBtn`, `LoadGameBtn`, `MultiBtn`, `OptionsBtn`, `CinematicsBtn`, `QuitBtn`). Investigar se o "Play Tutorial" vem do `MainMenu.amx` ou de outra versão da demo. Pelo print, os textos dos botões fazem parte das texturas.
- **Fundo do menu:** o print mostra o trem e **dois personagens** (soldado de armadura com minigun e o prisioneiro de macacão). São as entidades do mapa `Mainmenu`: viram modelos no item 4.

---

## 4. Recursos novos: os mais fáceis primeiro (escolha minha)

| Recurso | Entra no v20? | Por quê |
|---|---|---|
| **Salvar/carregar** (personagem, mapa, câmera, variáveis dos scripts) em formato próprio (`saves\slotN.f3sav`), pela tela `Options_Load` | **Sim** | Só depende de dados que já temos; a tela já existe |
| **Fala flutuante dos NPCs** (`FloatingSpeechText` em 3D, em cima da cabeça) e **nome ao passar o mouse** (nome do `EEOV`) | **Sim** | As natives já guardam a fala; é só desenhar na posição projetada |
| **Log de mensagens da barra de ações** (o quadro vermelho/verde do print) | **Sim** | Texto e fonte já funcionam |
| **Inventário só de leitura**: itens iniciais do jogador e dos containers (`EEOV` + protótipos `.ITM/.WEA/.ARM/.AMO`), nomes e ícones na grade da tela original | **Talvez** (se sobrar tempo) | Precisa decifrar os protótipos de item; arrastar/equipar fica para o v21 |
| **Modelos 3D estáticos** (cenário e NPCs parados, sem animação) direto do `.grp` | **Talvez**: começar pelos 2 personagens do menu e pelos NPCs de Junktown | O leitor de B3D existe em Python (1.472 de 3.915 modelos convertem); portar para C++ é médio. Pele, esqueleto e pose dependem do GR2 |
| **NPCs animados** (GR2 + esqueleto), **personagem do jogador andando**, combate, Pip-Boy funcional | **v21** | O GR2 precisa de um descompressor portado para C++; é o maior pedaço que falta |

Ordem de trabalho no v20: **1 → 2 → 3.1/3.2 → salvar/carregar → fala flutuante e log → 3.3 → inventário/modelos se der tempo.**

---

## 5. Como vou trabalhar e conferir (para não repetir o v19)

1. Desenvolvo e testo aqui: compilação para Windows de 64 bits com os headers reais do MinGW, testes C++ com ASan/UBSan, testes Python.
2. **Leio a sua cópia extraída e a demo pela pasta `F:\F3VB Engine`**, como no v19, para:
   - conferir os leitores de `.grp`/`.rht` byte a byte (`F3VBCheck`);
   - cruzar as fórmulas com os `.CRT`;
   - gerar prévias das telas com os seus arquivos.
3. Antes de entregar, rodo **exatamente os comandos dos docs** em pastas montadas como as suas (a seção 1.4).
4. Entrego um `.zip` só com código (`verificar_sem_assets.py`) e gravo ao lado do v19 no seu PC.
5. **O teste no Windows continua sendo seu.** O que eu preciso de volta: a saída do `F3VB.exe --check` (ou `doctor`), o `logs\f3vb_log_*.txt` e prints.

## 6. Critério de "v20 pronto"
- [ ] `build.bat` na raiz funciona; `F3VB.exe --demo "F:\F3VB Engine\Demo\Demo"` abre o menu **sem nenhuma extração**.
- [ ] O mesmo jogo também abre com a cópia extraída (`--assets`), para depuração.
- [ ] `F3VBCheck`: 7.044/7.044 recursos iguais aos extraídos; mapas com os números do `tre-check`.
- [ ] Pasta errada → mensagem clara (log e tela), nunca tela preta.
- [ ] `doctor`/`--check` cobre todos os pré-requisitos; os comandos dos docs são testados automaticamente.
- [ ] Botões sem brilho aditivo; câmera tática no estilo do original.
- [ ] Salvar/carregar e fala flutuante funcionando.
- [ ] CHANGELOG, README, COMANDOS, JOGO e as specs atualizados.
