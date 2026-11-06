# Extrator com interface + Visualizador 3D (OpenGL, 64 bits) + log e análise (v12)

1. **F3VB Extractor** (Python + tkinter, ou linha de comando): extrai a demo para `original_assets/RETAIL` (originais) e converte para
   `original_assets/CONVERTED` (`.f3m`, PNG, GLB). Ver `docs/ORIGINAL_ASSETS.md`.
2. **F3VB Viewer** (C++14, Win32 + OpenGL 1.x, 64 bits, sem bibliotecas externas): abre um `.f3m`, mostra malha + textura, toca animações, e
   **grava um log completo** de tudo que acontece e de uma análise técnica do modelo.
3. **f3v_analyze** (C++, linha de comando): gera o MESMO relatório do log sem abrir janela.

## Passo a passo (Windows)
```powershell
# 1) UMA vez: extrair a demo (PowerShell, pasta tools\py). Sem --out, o destino é <projeto>\original_assets
cd "F:\F3VB Engine\M1\f3vb-starter-v11\f3vb\tools\py"
py -3 -m f3vb extract-all "F:\F3VB Engine\Demo\Demo" --png --f3m
# (ou a interface:  py -3 f3vb_extractor_gui.pyw)
# Se o conversor mudou (ex.: v11 -> v12), refazer só CONVERTED, sem a demo:  py -3 -m f3vb convert-retail --png --f3m
```
```bat
:: 2) Compilar (Developer Command Prompt for VS, em cmd). Gera F3VBViewer.exe, f3v_analyze.exe e os testes
cd /d "F:\F3VB Engine\M1\f3vb-starter-v11\f3vb\viewer"
build.bat
type build\exe_path.txt
```
```powershell
# 3) Abrir o cão e o Eyebot (caminho do .exe: o que build\exe_path.txt mostrou)
cd "F:\F3VB Engine\M1\f3vb-starter-v11\f3vb\viewer"
.\build\F3VBViewer.exe "..\original_assets\CONVERTED\f3m\models\CR_Dog.f3m"
```
Também dá para **arrastar** `.f3m` para a janela.

## Teclas
| Tecla | O que faz |
|---|---|
| `O` / `A` / `X` | abrir modelo / adicionar animações ou esqueleto (`.f3m` sem malha) / limpar animações |
| `Espaço`, `[` `]`, `←` `→`, `↑` `↓` | tocar/pausar, animação anterior/próxima, tempo (1/30 s), velocidade |
| **`P`** | **liga/desliga a pose de bind** (liga: mostra o modelo sem animação; desliga: volta à animação em que estava) |
| **`K`** | liga/desliga o desenho dos ossos; **ao ligar, a lista completa de ossos (posição, deslocamento, comprimento) vai para o log** |
| **`F2`** | grava no log a análise completa agora (modelo + pose atual + ossos) |
| **`N`** | aplica a **escala extra estimada** da malha (ajusta a malha ao esqueleto) |
| **`M`** | alterna a escala extra entre 1 e 1/10,24 (para `.f3m` gerados pelo conversor v11) |
| **`,` `.`** | ajuste fino da escala extra (×0,98 / ×1,02) |
| `T` `W` `G` `C` `R` `F1` | texturas, wireframe, grade, culling, câmera, ajuda |
| Mouse | esquerdo gira, direito/meio move, roda dá zoom |
O menu tem os mesmos comandos. O HUD mostra a escala extra atual e o caminho do log.

## O log automático
- **Arquivo:** `logs\f3viewer_log_MM-DD-YY_hh-mm-ss.txt`, na pasta do `.exe` (por exemplo `viewer\build\logs\f3viewer_log_10-12-26_14-30-05.txt`). Criado ao abrir o programa, gravado linha a linha (se fechar de repente, o log até ali está salvo). Se a pasta não puder ser criada, tenta a pasta do `.exe` e depois a pasta temporária.
- **Cabeçalho:** versão, data de compilação, caminho do `.exe`, linha de comando, placa/driver OpenGL (fornecedor, renderizador, versão), tamanho da janela, lista de teclas.
- **A cada ação:** `[hh:mm:ss.mmm] comando pose-bind(P) | Pose de bind LIGADA ...`, mais um resumo da pose (caixa, esticamento das arestas, deslocamento das juntas). Abrir arquivo, adicionar animação, trocar de animação, mudar a escala, `K`, `F2` etc.
- **A cada 30 s:** uma linha "estado" (modelo, animação, tempo, FPS, escala). No fim, "janela fechada" e "sessão encerrada normalmente". Se faltar a linha final, o programa caiu.
- **A cada carregamento:** a **análise completa** (abaixo).

### O que a análise completa contém
1. **[RESUMO]** no topo: lista de avisos automáticos (ou "nenhum problema detectado").
2. **[TEXTURAS]** nome, tamanho, potência de 2, alfa mín/máx, consistência dos pixels. **[MATERIAIS]**: textura, cor, modo de alfa, dupla face.
3. **[MALHAS]** por malha: vértices, triângulos, material, caixa; índices inválidos e triângulos degenerados; UV (faixa, fora de 0..1); normais (comprimento); **pesos** (soma, vértices sem influência, máximo de influências), **ossos citados sem junta** correspondente, juntas dominantes.
4. **[ESQUELETO]** por junta: pai, translação/quaternion/escala locais, posição no mundo do bind, e `mundo × ibm` (deve ser a identidade). Detecta o **deslocamento constante** (posicionamento inicial do GR2).
5. **[ESCALA MALHA x ESQUELETO]** tamanho da malha × caixa dos ossos e a **estimativa por ajuste** do fator extra `f` (1 = correto), com erro antes/depois e aviso se estiver fora de 0,85–1,18.
6. **[ANIMACOES]** por clipe: duração, canais T/R/S, chaves, ossos sem junta, juntas sem canal, e a pose a 25/50/75%: crescimento da caixa, **razão dos comprimentos das arestas pose/bind** (p50/p95/p99) e deslocamento máximo de junta, com veredito `ok` / `SUSPEITO` / `ERRO`.

### Como ler uma animação quebrada
- Arestas **p95 > 2** (ou p99 > 4): o modelo está sendo esticado. Causas conhecidas: malha em escala diferente do esqueleto (aviso na seção 5), pesos errados, ossos sem junta.
- **Pose de bind certa mas animação errada** = escala/pivôs. **Pose de bind já errada** = esqueleto, ibm ou pesos.
- Peças que ficam soltas (Eyebot) com arestas ok = cada peça é rígida; olhe o deslocamento das juntas e a seção de escala.

## `f3v_analyze` (sem janela)
```bat
build\f3v_analyze.exe "..\original_assets\CONVERTED\f3m\models\CR_Dog.f3m" --out=relatorio_cao.txt
build\f3v_analyze.exe modelo.f3m "..\original_assets\CONVERTED\f3m\skeletons\CR_Dog.f3m" --scale=0.0977
```
Primeiro arquivo = modelo; os seguintes = esqueleto/animações a adicionar; `--scale=` testa uma escala extra. O relatório pode ser enviado como texto (não contém dados do jogo além de nomes).

## Como "adicionar modelo + animações"
- `O` abre um modelo; `A` abre `.f3m` **sem malha** (os de `CONVERTED\f3m\skeletons\`): fornecem esqueleto se o modelo não tem, ou só animações (casadas por NOME de osso). A ordem não importa.

## Testado e não testado
**Testado automaticamente (Linux):** 107 testes Python (extrator, layout, `convert-retail`, F3M, GLB, B3D, AMX, trava anti-asset) + núcleo C++ (leitor F3M, skinning idêntico ao Python de referência, **log com nome/hora/sufixo**, **análise de modelo**, **estimativa de escala** em modelo sintético, `Scene::mesh_scale`, 1.020 arquivos F3M corrompidos analisados sem quebrar).
**Medido com os arquivos reais do cão e do Eyebot (Linux, Python + `f3v_analyze`):** a análise acusa a malha do v11 (`f ≈ 0,097`) e aprova a do v12; esticamento de arestas nas animações do cão caiu de p95 3,7–8,7× para 1,25–1,7×.
**NÃO executado:** a janela Win32/OpenGL do v12 (log, teclas `P K N M , . F2`) e a interface tkinter. Só a sintaxe de `viewer_main.cpp` foi checada contra headers *stub*. O v11 do visualizador foi confirmado por você no Windows; o que mudou no v12 precisa do seu teste. Se algo falhar, mande a mensagem inteira e o log.

## O que mudou no v13 (humanos)
- Ossos do modelo e do esqueleto casam por variação de nome (`X` ↔ `X_1`); a análise lista os casados em "ossos casados por variação de nome".
- Osso sem junta: o peso daquela parte fica no bind (a malha não colapsa para a origem).
- Humanos: use o F3M de esqueleto da família (`A` no visualizador) para ver as animações `HumM_*`/`HumF_*`; os `.f3m` de modelo trazem só 8.
- Os avisos do [RESUMO] não se repetem mais por malha.

## Limitações conhecidas
- Texturas de lightmap não são desenhadas; `ADD`/`ALPHABLEND` são aproximados; cor de vértice é ignorada; luz direcional simples.
- A escala 1/10,24 é **hipótese** apoiada em dois modelos (ajuste 10,2–10,4); a análise estima o valor de cada modelo. Ver `docs/ACHADOS_DEMO.md`.
- Skinning na CPU (OpenGL 1.x): suficiente para 1 a 3 mil vértices.
- Terreno (tiles) e Effects ainda não decodificam: ver `docs/ACHADOS_DEMO.md`.

## Compilação
- Developer Command Prompt for VS (cmd), não PowerShell: o `cl.exe` precisa estar no PATH. O `build.bat` tenta Ninja (sem `-A x64`) e depois `Visual Studio 18 2026 -A x64`; apaga `build` a cada execução.
- `testar_nucleo.bat` roda `test_core` e `test_v12` (acha os `.exe` nos dois layouts de pasta).
