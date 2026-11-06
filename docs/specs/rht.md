# Spec: `resource.rht` (tabela de recursos)

**Status: VERIFICADA em arquivo real** (demo do usuário: v1.1, 7044 entradas, 346.410 bytes). A hipótese do campo `offset` é validada pelo comando `catalog` (ver "Como validar").

## Layout (little-endian)
Cabeçalho (20 bytes): `u32 vMajor, vMinor, nEntries, offsetPacks, offsetResources`.
Seguem `nEntries` entradas de 20 bytes: `u32 one, number, type, offset, pack`.
Depois: região de **packs** em `[offsetPacks, offsetResources)` e região de **recursos** em `[offsetResources, fim)`.

Confirmado na demo: `offsetPacks = 20 + 20 * nEntries` (140900) e `offsetResources - offsetPacks = 147`.

## Semântica dos campos
| Campo | Significado | Evidência |
|---|---|---|
| `one` | sempre 1 | 7044 de 7044 |
| `pack` | deslocamento dentro da região de packs, onde fica o **nome do `.grp`** (`Tiles`, `Interface`, `Critters`, `Engine`, `Templates`, `Scripts`, `Maps`, `Effects`, `_Dev`, `Props`, `Sounds`, `Items`, `_CON`, `_CHM`, `_ARM`, `_AMO`, `_CRT`, `_DOR`, `_ESF`, `_ITM`, `_MSC`, `_RLZ`, `_USE`, `_WEA`) | região de packs = 24 strings NUL-terminadas nos offsets 0, 6, 16, 25, 32... |
| `number` | **índice do lump** dentro desse `.grp` | contagem de entradas por pack = nº de lumps do `.grp` nos 18 packs não vazios; soma = 7044 = total de lumps; primeiros lumps de `Tiles`, `Interface` e `Engine` batem com o tipo da entrada |
| `type` | id do tipo de recurso (lista em `knowledge.py`) | nenhum id fora da lista na demo |
| `offset` | (hipótese) posição do **nome do recurso** na região de recursos | `offset` máximo ≈ tamanho da região; espaçamento entre offsets ≈ tamanho de um nome |

**Atenção:** o enum `PackOffset` do `rht.pat` usa **rótulos errados** (PSF, INT, RLE...). Só os valores numéricos servem; o nome verdadeiro vem das strings da região de packs.

## Tipos presentes na demo
G3D_Model 3916, TGA_Image 1653, SMA_Script 36, TRE_Octree 39, SKEL_Skeleton 99, GR2_Animation 448, INI_Config 125, WAV_Audio 113, AMX 83, MAP_Map 38, CRT 108, VEG 119, INT 25, RLE 39, ITM 5, PSF 19, GSF 3, GLS 1, SCO 1, PCE 1, WEA 51, ARM 22, DOR 13, USE 18, AMO 20, CON 48, TOK 1.
**Ausentes** nesta demo: SKN (500), RLZ (900), RTD (1000), SST/STR/ENC/WMP/PRO/SPL/FNT. Ou seja, RLZ e RTD não são necessários para esta demo.

## Como validar a hipótese do `offset` e do mapeamento
`python -m f3vb catalog "<pasta da demo>"` cruza cada entrada com o lump real, imprime a concordância tipo x conteúdo e escreve `catalogo.txt` (pack, número, tipo, classe detectada, tamanho, nome do recurso).
