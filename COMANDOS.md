# F3VB: comandos prontos para colar (PowerShell)

> Troque `C:\CAMINHO\...` pelos seus caminhos. Se `py -3` não funcionar, use `python`.
> Tudo que for gerado cai em `private\` (o git ignora). **Nunca commite nem publique o que sair de lá.**

## 0) Instalar o Python (uma vez só)
```powershell
winget install Python.Python.3.12
```
Feche e abra o PowerShell de novo. Confira:
```powershell
py -3 --version
```

## 1) Descompactar o kit e entrar na pasta
```powershell
Expand-Archive -Path "$HOME\Downloads\f3vb-starter-v11.zip" -DestinationPath "C:\F3VB" -Force
cd C:\F3VB\f3vb
```

## 2) Testar se está tudo certo (não precisa da demo)
> Faça isto a partir da pasta `...\f3vb` (a raiz do projeto). Se o prompt já mostrar `...\tools\py`, **não** rode o `cd tools\py` de novo (dá erro, mas é inofensivo).
```powershell
cd tools\py
py -3 -m unittest discover -s tests -t .
cd ..\..
```
Esperado no final: `OK` (34 testes).

## 3) O jeito fácil: um comando faz tudo
> Rode a partir da pasta `...\f3vb` (raiz). Se o prompt estiver dentro de `scripts`, troque `.\scripts\rodar_tudo.ps1` por `.\rodar_tudo.ps1`.
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\rodar_tudo.ps1 -PastaDemo "C:\CAMINHO\DA\DEMO"
```
Com amostras binárias e análise profunda de `.gr2`, `.b3d`, `.tga`, `.amx`:
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\rodar_tudo.ps1 -PastaDemo "C:\CAMINHO\DA\DEMO" -IncluirAmostras
```
Resultado: `private\ENVIAR_PARA_ANALISE.zip` (e a pasta `private\relatorio\` com os `.txt`).
Se o zip ficar grande, envie só `private\relatorio\03_resumo.txt` e `private\relatorio\catalogo\catalogo_resumo.txt`.

> **Sobre os zeros no log:** `codigo: 0` é o código de saída do programa: **0 = deu certo**. Qualquer outro número é erro.
> Também aparece `lumps=0` em 6 `.grp` (`_CHM`, `_Dev`, `_ESF`, `_MSC`, `_RLZ`, `Templates`): são `.grp` vazios de verdade (12 bytes, só o cabeçalho).

## 4) O jeito manual, passo a passo
```powershell
cd tools\py
$env:PYTHONUTF8 = "1"
$demo = "C:\CAMINHO\DA\DEMO"

# 4.1 inventário (tudo fora dos .grp)
py -3 -m f3vb inventory $demo

# 4.2 resource.rht (troque o caminho se ele estiver numa subpasta)
py -3 -m f3vb rht "$demo\resource.rht" --entries 60

# 4.2b catálogo: cruza o .rht com os .grp (nome, tipo e classe de cada recurso) e valida o mapeamento
py -3 -m f3vb catalog $demo --out ..\..\private\catalogo
type ..\..\private\catalogo\catalogo_resumo.txt

# 4.3 relatório completo + resumo curto em texto
py -3 -m f3vb report $demo -o ..\..\private\report.json --resumo ..\..\private\resumo.txt
py -3 -m f3vb resumo ..\..\private\report.json

# 4.4 conteúdo de UM .grp (troque NOME.grp)
py -3 -m f3vb grp "$demo\NOME.grp"
py -3 -m f3vb grp "$demo\NOME.grp" --list > ..\..\private\lumps_NOME.txt

# 4.5 amostras (2 de cada classe). --so-texto = zip só com .txt (sem os binários do jogo)
py -3 -m f3vb sample "$demo\NOME.grp" --out ..\..\private\amostra --classes gr2,b3d,tga,amx --per-class 2 --so-texto
# versão com os binários (para análise profunda):
py -3 -m f3vb sample "$demo\NOME.grp" --out ..\..\private\amostra --classes gr2,b3d,tga,amx --per-class 2

# 4.6 analisar um arquivo da amostra (troque o nome do arquivo)
py -3 -m f3vb gr2 ..\..\private\amostra\gr2_000123.gr2
py -3 -m f3vb inspect ..\..\private\amostra\b3d_000456.b3d
py -3 -m f3vb inspect ..\..\private\amostra\tga_000789.tga

# 4.7 sondar um lump direto do .grp, sem extrair (o número é o índice do lump)
py -3 -m f3vb inspect --grp "$demo\NOME.grp" 123

# 4.8 extrair tudo de uma classe (cuidado: pode ser grande)
py -3 -m f3vb extract "$demo\NOME.grp" --classes gr2 --out ..\..\private\gr2
cd ..\..
```

## 4b) Scripts AMX (os 119 scripts do jogo)
```powershell
cd tools\py
# desmontar um script (troque 123 pelo índice do lump em Scripts.grp; veja o 04_grp_data_Scripts.txt)
py -3 -m f3vb amxdis --grp "$demo\data\Scripts.grp" 123 > ..\..\private\script_123.txt
# só validar (decodifica o código todo, confere publics e natives)
py -3 -m f3vb amxdis --grp "$demo\data\Scripts.grp" 123 --check
cd ..\..
```

## 4c) Falhas do B3D em texto (para decifrar o formato sem enviar arquivos do jogo)
```powershell
cd tools\py
py -3 -m f3vb b3dfail "$demo\data\Props.grp" --rht "$demo\resource.rht" --out ..\..\private\b3d_falhas_Props.txt
cd ..\..
```
O `rodar_tudo.ps1` já faz isso para Tiles, Critters, Props, Items e Effects (`07_b3d_falhas_*.txt`).

## 4d) Gerar a lista das 115 funções nativas a partir do seu relatório
```powershell
py -3 scripts\gerar_natives_md.py private\relatorio\03_report.json
```

## 4e) Exportar um modelo para GLB (abrir no Blender)
```powershell
cd tools\py
py -3 -m f3vb export-list G3D_Model --prefix CR_ "$demo"
py -3 -m f3vb export-glb CR_Ant "$demo" --out ..\..\private\glb\CR_Ant.glb
cd ..\..
```
Mais detalhes e o que está validado: `docs\EXPORTADOR_GLB.md`.

## 4f) Textos dos diálogos (.stf)
```powershell
cd tools\py
py -3 -m f3vb stf "$demo\NOME.stf" --info --id 3113 --id 3123
cd ..\..
```
Se disser "Não parece um .stf", copie a mensagem inteira (mostra os primeiros bytes).

## 4g) EXTRATOR e VISUALIZADOR 3D (v12): `original_assets` com RETAIL e CONVERTED
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v11\f3vb\tools\py"
py -3 f3vb_extractor_gui.pyw                                                        # interface (opcional)
py -3 -m f3vb extract-all "F:\F3VB Engine\Demo\Demo" --plan-only                  # quantos arquivos/bytes (não grava nada)
py -3 -m f3vb extract-all "F:\F3VB Engine\Demo\Demo" --png --f3m                  # demo -> original_assets\RETAIL + CONVERTED
py -3 -m f3vb convert-retail --png --f3m                                            # refaz só CONVERTED a partir de RETAIL (sem a demo)
```
Destino padrão: `<projeto>\original_assets` (outro lugar: `--out "D:\assets"` ou variável `F3VB_ASSETS_DIR`). Detalhes: `docs\ORIGINAL_ASSETS.md`.
Exportar um modelo avulso (teste de escala: `--mesh-scale 1` reproduz o v11):
```powershell
py -3 -m f3vb export-f3m CR_Dog "F:\F3VB Engine\Demo\Demo" --out ..\..\private\cao_teste.f3m --mesh-scale 0.09765625
```
Compilar e usar o visualizador (**Developer Command Prompt for VS, em cmd**):
```bat
cd /d "F:\F3VB Engine\M1\f3vb-starter-v11\f3vb\viewer"
build.bat
type build\exe_path.txt
testar_nucleo.bat
build\F3VBViewer.exe "..\original_assets\CONVERTED\f3m\models\CR_Dog.f3m"
build\f3v_analyze.exe "..\original_assets\CONVERTED\f3m\models\CR_Dog.f3m" --out=relatorio_cao.txt
```
O log fica em `build\logs\f3viewer_log_MM-DD-YY_hh-mm-ss.txt`. Teclas e como ler a análise: `docs\VISUALIZADOR.md`.

## 4g2) Investigar um modelo (v13)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v13\f3vb\tools\py"
py -3 -m f3vb b3dmat HumFStr --out ..\..\private\humfstr_materiais.txt          # materiais, texturas, UVs e ossos (só nomes e números)
py -3 -m f3vb pack-sample HumFStr HumMNor --anims 6                                # zip de amostra em ..\..\private\ (conteúdo do jogo: só para enviar a quem depura)
py -3 -m f3vb export-list GR2_Animation --prefix HumF_ "F:\F3VB Engine\Demo\Demo"  # nomes das animações de uma família
```
Refazer os convertidos depois de atualizar o projeto (o conversor v13 conserta ossos e animações dos humanos), **sem** a demo:
```powershell
py -3 -m f3vb convert-retail --png --f3m
```
Se o projeto novo estiver em outra pasta, aponte para o `original_assets` que você já extraiu: `$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"` antes dos comandos.

## 4g3) Segunda engine (RPGGL) em `engine_test\rpggl`
```powershell
py -3 -m unittest tests.test_engine_test        # compila e roda o teste headless dela (precisa de g++; no Windows rode rpggl\tests\run_tests.sh pelo WSL, ou compile com rpggl\build.bat no Developer Command Prompt)
```
Detalhes e plano: `docs\RPGGL_ANALISE.md`.

## 4g4) Mapas e grades (v14)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v14\f3vb\tools\py"
$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"      # sua extração (ajuste)
py -3 -m f3vb convert-retail --maps                                                       # todos os mapas -> CONVERTED\maps (txt, json, png)
py -3 -m f3vb map-info Tutorial_Vault --png ..\..\private\vault.png --out ..\..\private\vault.txt
py -3 -m f3vb rle-info 04_Tutorial_Vault --png ..\..\private\vault_grade.png
```
`map-info` aceita um trecho do nome (se for único). O resumo de todos os mapas fica em `CONVERTED\maps\maps_summary.txt`. Formatos: `docs\specs\map.md` e `docs\specs\rle.md`.

## 4g5) Abrir um mapa em 3D (lotes, tecla M)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\tools\py"
$env:PYTHONUTF8 = "1"
$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
py -3 -m f3vb convert-retail --maps                      # gera CONVERTED\maps\<mapa>.f3map (+ png, txt, json)
py -3 -m f3vb tre-info Tutorial_Vault                    # resumo da geometria 3D + lotes (materiais, páginas, texturas)
# conferir o parser de lotes em TODOS os .8tre (esperado: nos=5925 nao_fechados=0 fora_bbox=0 triangulos=1928117):
py -3 -m f3vb tre-check "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets\RETAIL\Maps"
```
**Reconverta os mapas** (o `convert-retail --maps` acima): os `.f3map` do v15 são versão 1 e não têm material/página/textura.
```bat
cd /d "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\viewer"
build.bat
testar_nucleo.bat
build\F3VBMapViewer.exe
```
Sem argumento, o `F3VBMapViewer.exe` abre o diálogo para escolher o `.f3map`. Tecla **`M`**: colore a geometria por material / página de lightmap / textura difusa (legenda no canto inferior esquerdo). O log fica em `build\logs\f3mapviewer_log_*.txt`. Detalhes e teclas: `docs\MAPVIEWER.md`.

## 4g6) Scripts, VM e diálogos no mapa (v17)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\tools\py"
$env:PYTHONUTF8 = "1"
$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
py -3 -m f3vb export-scripts                       # copia os 119 .amx do RETAIL para CONVERTED\scripts (segundos; nao refaz mapas)
# a tabela de textos do jogo (.stf, na pasta da demo) vai ao lado dos scripts para as falas virarem frases:
Get-ChildItem "F:\F3VB Engine\Demo\Demo" -Filter *.stf -Recurse | Select-Object -First 1 | Copy-Item -Destination "$env:F3VB_ASSETS_DIR\CONVERTED\scripts\"
py -3 -m f3vb stf (Get-ChildItem "$env:F3VB_ASSETS_DIR\CONVERTED\scripts\*.stf" | Select-Object -First 1).FullName   # confere o layout do .stf (me mande a saida)
```
```bat
cd /d "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\viewer"
build.bat
testar_nucleo.bat
set "F3VB_ASSETS_DIR=F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
build\F3VBMapViewer.exe
rem um script sozinho, sem janela (saida tambem em arquivo):
build\F3VBScript.exe "%F3VB_ASSETS_DIR%\CONVERTED\scripts\10 Mormon.amx" --stf "%F3VB_ASSETS_DIR%\CONVERTED\scripts\SEU_ARQUIVO.stf" --log dialogo_mormon.txt
build\F3VBScript.exe "%F3VB_ASSETS_DIR%\CONVERTED\scripts\10 Mormon.amx" --survey
```
No visualizador: abra um mapa com NPCs, **Tab** seleciona uma entidade (a linha de seleção diz se o script foi achado), **V** abre o diálogo, **1-9** respondem, **B** levanta todos os scripts do mapa. Detalhes: `docs\VM_E_DIALOGOS.md`.

## 4g7) Texturas, VM corrigida e base da interface (v18)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\tools\py"
$env:PYTHONUTF8 = "1"
$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
py -3 -m unittest discover -s tests -t .
py -3 -m f3vb convert-retail --maps          # .f3map v3 (com UVs) + scripts + texturas (a 1a vez demora mais: decodifica os TGA)
py -3 -m f3vb export-textures                # so as texturas (se quiser refazer: --overwrite); lista o que faltar
py -3 -m f3vb ui-info                        # inventario do pack Interface + sonda dos .int + imagens da UI (CONVERTED\ui)
Get-Content "$env:F3VB_ASSETS_DIR\CONVERTED\textures\textures_summary.txt" -TotalCount 40
Get-Content "$env:F3VB_ASSETS_DIR\CONVERTED\ui\ui_report.txt" -TotalCount 80
```
```bat
cd /d "F:\F3VB Engine\M1\f3vb-starter-v18\f3vb\viewer"
build.bat
testar_nucleo.bat
set "F3VB_ASSETS_DIR=F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
build\F3VBMapViewer.exe
build\F3VBScript.exe "%F3VB_ASSETS_DIR%\CONVERTED\scripts\CaptainArmstrong.amx" --survey --log survey_armstrong.txt
build\F3VBScript.exe "%F3VB_ASSETS_DIR%\CONVERTED\scripts\MainMenu.amx" --survey --log survey_mainmenu.txt
```
No visualizador: **X** alterna texturas (sem / difusa / difusa+lightmap), **M** modos de cor, **Tab** seleciona entidade, **V** diálogo (procura o script por nome se o da entidade não tiver), **K** próximo script de diálogo, **B** levantamento de scripts do mapa (agora com natives chamadas e exemplos). Detalhes: `docs\MAPVIEWER.md` e `docs\VM_E_DIALOGOS.md`.

## 4g8) O JOGO: interface, criação de personagem, Junktown e diálogos (v19)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v21\f3vb\tools\py"
$env:PYTHONUTF8 = "1"
$env:F3VB_ASSETS_DIR = "F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
py -3 -m unittest discover -s tests -t .
py -3 -m f3vb convert-retail --maps                          # OBRIGATORIO no v19: .8tre com a gramatica completa + chunks CAMR/ENVI
py -3 -m f3vb tre-check                                      # esperado: nos=5925 gramatica_completa=5925 nao_fechados=0 fora_bbox=0 triangulos=1928117 sem_estado=0
py -3 -m f3vb import-loose "F:\F3VB Engine\Demo\Demo"        # Fonts\*.ttf e English.stf -> original_assets\RETAIL (e CONVERTED\scripts)
py -3 -m f3vb int-info Mainmenu                              # arvore de widgets de uma tela, com os textos do .stf
py -3 -m f3vb int-info CharacterGeneration > chargen_int.txt
```
```bat
cd /d "F:\F3VB Engine\M1\f3vb-starter-v21\f3vb\viewer"
build.bat
testar_nucleo.bat
jogar.bat
:: ou, a mao (sem o jogar.bat):
set "F3VB_ASSETS_DIR=F:\F3VB Engine\M1\f3vb-starter-v12\f3vb\original_assets"
set "F3VB_DEMO_DIR=F:\F3VB Engine\Demo\Demo"
build\F3VB.exe
build\F3VB.exe --map 00_03_Tutorial_Junktown
:: previas das telas em PNG, sem abrir o jogo (vao para a pasta ui_preview)
build\F3VBUiPreview.exe --all --assets "%F3VB_ASSETS_DIR%" --demo "%F3VB_DEMO_DIR%" --out ui_preview
build\F3VBUiPreview.exe Mainmenu --hover NewGameBtn --assets "%F3VB_ASSETS_DIR%" --demo "%F3VB_DEMO_DIR%" --out mainmenu_hover.png
```
Teclas, fluxo das telas e hipóteses: `docs\JOGO.md`. **Mande** o `viewer\build\logs\f3vb_log_*.txt` e prints (menu, criação de personagem, Junktown, um diálogo).

## 4h) Conferir e empacotar o projeto (sem assets)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v11\f3vb"
py -3 scripts\verificar_sem_assets.py          # falha se houver dado do jogo no projeto
py -3 scripts\empacotar.py v19                 # regenera os .md de código, confere e gera ..\f3vb-starter-v21.zip
```

## 5) Testes em C++ (opcional; precisa de g++ ou CMake)
Com CMake (Visual Studio, Ninja ou MinGW):
```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Depois de compilar, ver a árvore de diálogo de um script real extraído (`amx_run`, `dialog_dump`):
```powershell
.\build\Release\dialog_dump.exe "C:\caminho\script.amx"
```
Rodar a VM num script real extraído (troque o caminho do `.amx`; o executável fica em `build\Release\amx_run.exe` no Visual Studio):
```powershell
.\build\Release\amx_run.exe "C:\caminho\script.amx" --list
.\build\Release\amx_run.exe "C:\caminho\script.amx"
```
Com g++ direto:
```powershell
g++ -std=c++20 -o $env:TEMP\t_archive.exe tests_cpp\test_archive.cpp ; & $env:TEMP\t_archive.exe
g++ -std=c++20 -o $env:TEMP\t_chunk.exe tests_cpp\test_chunk.cpp ; & $env:TEMP\t_chunk.exe
```

## 6) Regenerar a versão .md de todo o código (para ler/enviar)
```powershell
py -3 scripts\gerar_codigo_md.py
```
Cria `CODIGO_COMPLETO.md` e `CODIGO_THIRD_PARTY.md` na raiz do projeto.

## 7) O que me enviar
1. `private\relatorio\03_resumo.txt` (o mais importante)
2. `private\relatorio\catalogo\catalogo_resumo.txt` (valida o mapeamento rht x grp)
   `private\relatorio\07_b3d_falhas_Props.txt` e `07_b3d_falhas_Critters.txt` (texto; mostram onde os modelos ainda não decodificam)
3. `private\relatorio\02_rht.txt`
4. As primeiras linhas de um `04_grp_*.txt`
5. Se der: `private\ENVIAR_PARA_ANALISE.zip`
6. Qualquer erro, copiado inteiro

## 5) O jogo (v20): direto da demo original
No **Developer Command Prompt do Visual Studio** (cmd, não PowerShell):
```bat
cd /d "F:\F3VB Engine\M1\f3vb-starter-v21\f3vb\viewer"
build.bat
build\F3VB.exe
```
Na 1ª vez, escolha a pasta da demo (a do `resource.rht`); fica gravada em `build\F3VB.ini`.
```bat
build\F3VB.exe --escolher-pasta                        & rem escolher a pasta de novo
build\F3VB.exe --skip-menu --map 00_03_Tutorial_Junktown & rem direto para um mapa
build\F3VB.exe --chargen                               & rem direto para a criação de personagem
:: conferir a leitura da demo, todos os mapas e todos os modelos 3D (sem abrir janela):
build\F3VBCheck.exe --demo "F:\F3VB Engine\Demo\Demo" --maps --models --tudo
:: v21: o jogo confere tudo sozinho (videos, menu, criacao, todos os mapas, andar, combate) e fecha; relatorio em build\logs\autoteste_*.txt
build\F3VB.exe --autoteste
```
Se algo der errado, mande o log mais novo de `build\logs\`.
