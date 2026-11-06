# `original_assets/`: onde ficam os dados do SEU jogo (v12)

Tudo que sai da demo vai para **uma só pasta**, dentro do projeto (`f3vb/original_assets/`), e **nunca** é versionado nem empacotado.

```
f3vb/
  original_assets/                 <- criada pelo extrator; ignorada pelo git (tem .gitignore com "*" dentro)
    RETAIL/                        arquivos ORIGINAIS, byte a byte, como saíram dos .grp
      Critters/G3D_Model/CR_Dog.b3d
      Critters/SKEL_Skeleton/CR_Dog.skl
      Critters/GR2_Animation/CR_Dog_Com_Una_Idle_01.gr2
      Scripts/AMX/00_MainMenu.amx ...
    CONVERTED/                     arquivos CONVERTIDOS pelas nossas ferramentas (podem ser refeitos a qualquer hora)
      f3m/models/CR_Dog.f3m          modelo para o visualizador (malha + textura + esqueleto + animações)
      f3m/skeletons/CR_Dog.f3m       esqueleto + animações ("A" no visualizador)
      png/<Pack>/<Nome>.png          texturas
      glb/<Pack>/<Nome>.glb          modelos para o Blender (só com --glb)
      CONVERTER_VERSION.json         versão do conversor que gerou estes arquivos
    index.csv                      pack, numero, tipo, nome, caminho (relativo a RETAIL/), tamanho
    EXTRACAO_RELATORIO.txt         o que foi gravado, convertido e o que falhou
    LEIA-ME.txt                    aviso: conteúdo do jogo, não publicar
```
O nome da subpasta de tipo (`G3D_Model`, `SKEL_Skeleton`, `GR2_Animation`, `TGA_Image`, `AMX`...) vem da tabela de tipos do `resource.rht`.

## Comandos (PowerShell, na pasta `tools\py`)
```powershell
cd "F:\F3VB Engine\M1\f3vb-starter-v11\f3vb\tools\py"      # ajuste para a pasta do projeto

# 1) UMA vez: extrai os .grp da demo -> original_assets\RETAIL e converte -> original_assets\CONVERTED
py -3 -m f3vb extract-all "F:\F3VB Engine\Demo\Demo" --png --f3m
#    (sem --out, o destino é <projeto>\original_assets; para outro lugar: --out "D:\meus_assets" ou a variável F3VB_ASSETS_DIR)

# 2) Quando o CONVERSOR mudar (nova versão do projeto): refaz só CONVERTED, a partir de RETAIL, sem a demo e sem os .grp
py -3 -m f3vb convert-retail --png --f3m
```
O extrator avisa "CONVERTED veio de um conversor antigo" quando a versão registrada em `CONVERTER_VERSION.json` é menor que a do projeto
(ou não existe, como nos arquivos do v11). **Os `.f3m` do v11 têm a malha ~10,24× maior que o esqueleto: refaça com `convert-retail`.**

## Se você já extraiu com o v11 (pasta `F:\F3VB Engine\Extraido`)
Os originais dessa pasta estão no formato antigo (sem `RETAIL/`). O caminho mais simples é extrair de novo (≈ 2 minutos): `extract-all` já cria o layout novo.
Depois pode apagar a pasta antiga.

## Regras
- Nada dentro de `original_assets/` entra em commit, `.zip` do projeto, release, issue ou chat público.
- `py -3 scripts\verificar_sem_assets.py` confirma que o projeto está limpo; `scripts\empacotar.py` roda essa checagem sozinho.
- Para os testes automáticos não é preciso ter `original_assets/`: eles usam dados sintéticos.
