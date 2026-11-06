# Formato `.map`: descrição de um mapa (recurso tipo 1300 "MAP_Map")

**Estado (medido em 37 arquivos reais, 49.914 bytes):** o enquadramento em chunks cobre 100% dos arquivos e **todos os 14 tipos de chunk têm decodificador cujo layout consome o payload inteiro (0 byte sobrando)**. Atenção ao que isso quer dizer: dentro das sobrescritas de entidade (`EEOV`) ficam **277 lacunas numéricas (4.800 bytes) guardadas em hexadecimal, sem nome de campo**. Código: `tools/py/f3vb/mapfile.py` (+ `mapreport.py`). Alguns nomes de tag já constavam nas listas públicas do OpenJE (EMAP, EME2, EMTR, EPTH...; só fatos, nada copiado); o significado de cada tag foi **inferido do conteúdo** e os campos foram medidos nos arquivos.

## Enquadramento
Cada chunk de topo = `tag[4]` + `version u32` + `size u32` (o `size` inclui os 12 bytes do cabeçalho). Strings = `u16 comprimento` + bytes ASCII. Posições = `f32 x, y, z` (**y = altura**; o plano do mapa é x,z). Ordem usual: `EMAP`, `EME2`×N, `EMEP`, `ECAM`, gatilhos (`EMTR` + `EBTR`/`ESTR`/`ETTR`), `EPTH`, `EMSD`, `EMNP`, `EMNO`, `EMEF`, `2MWT`.

## Tags (contagem nos 37 mapas) e layouts
| Tag | Qtd | Layout | Confiança |
|---|---|---|---|
| `EMAP` v5 | 37 | `u32 0`, `str` nome do .8tre, `str` nome do .rle, `str` imagem de mapa de área (`_AM.tga/.dds`), depois 27 bytes: `u16 0`, **`u8[4]` cor A**, **`u8[4]` cor B**, `u8 flag`, `u32 0`, `f32 250.0`, `f32 500.0`, `f32 1.0` | strings e tamanho: **medido**. Cores e floats: hipótese (cor de fundo e de névoa; 250/500 = névoa perto/longe; o Mainmenu tem 0/0) |
| `ECAM` v0 | 37 | `f32[4]`, padrão (0, 1000, 1000, 0) | tamanho medido. **v19, hipótese:** limites da câmera `(xmin, zmax, xmax, zmin)`; o `F3VB.exe` prende o alvo da câmera neles. Vai para o `.f3map` no chunk `CAMR` |
| `EME2` v0 | 175 | `str` protótipo (`Nome.CRT/.WEA/.AMO/.DOR/.CON/.ITM/.USE/.ARM`), `f32 x,y,z`, `f32 yaw` (rad), `f32[2]` (quase sempre 0), `u8 1`, e um chunk filho `EEOV` até o fim | **medido nos 175** (o `EEOV` sempre termina no fim exato) |
| `EEOV` v1/v2 | 175 (62 v1, 113 v2) | `str` instância, `u8[3]` flags (quase sempre 1,1,1), depois uma sequência de **strings** (nome exibido, script `.amx`, textura de pele `.dds`, efeito `.veg`, inventário `.AMO/.WEA/...` precedido de `u32` contagem) e **números sem nome** (veja abaixo) | strings: medido. Números: **não decifrados** |
| `EMEF` v0 | 35 | `str` instância, `f32 x,y,z,yaw`, `f32[2]`, `u8 1`, `str` efeito `.veg`, `u8 0` | medido |
| `EMEP` v0 | 13 | `u8 id`, 6 × `vec3` (posições de entrada do grupo), 6 × `f32` (ângulos). Variante de 17 bytes: `u8 id`, `vec3`, `f32` (1 ponto) | medido |
| `EMNP` v0 | 37 | `u32 N`, N × 30 bytes: `u8 tipo`, `vec3 pos` (y=2,0), `vec3` (zeros), `u8[5]` vizinhos (`0xFF` = nenhum). **Grafo de navegação** | medido (14 nós, 28 arestas direcionadas no total) |
| `EMTR` v0 | 28 | `u32 tipo` (6), `u32 N`, N × `vec3`: **polígono de região** (y = 9,25) | medido |
| `EBTR` | 23 | `u32 id`, 3 bytes: **vínculo** da região anterior | medido (o significado do vínculo, não) |
| `ESTR` | 3 | `str` script `.amx`, `u32 0`: região que dispara script | medido |
| `ETTR` v1 | 2 | `str` mapa de destino, `u8` ponto de entrada (provavelmente o `id` de um `EMEP` do mapa de destino; não confirmado), `u8 1`: **região de viagem** (Junktown→Vault, entrada 1; Vault→Junktown, entrada 2) | medido |
| `EPTH` v0 | 9 | `str` nome, `u32 N`, N × 24 bytes: `vec3`, `f32 yaw`, `f32[2]`: **caminho de patrulha** | medido |
| `EMSD` v0 | 23 | `str` nome, `vec3`, `str` som `.psf` (às vezes ausente), 2 bytes de flags | medido |
| `EMNO` v0 | 5 | `vec3`, `str` ícone (`AreaMapIcon01.dds`), `u32` id de texto (1906...): **marcador de mapa** | medido |
| `2MWT` v1 | 2 | `str` "EFF_Water", **140 bytes** de parâmetros (35 floats, sem nome), `u32 N`, N × {`vec3`, `str` textura `.TGA`, `f32 u`, `f32 v`}: remendos de água a cada 10 unidades | medido (56 remendos) |

**Gatilhos:** `EMTR` abre um grupo; o chunk seguinte (`EBTR` ou `ESTR` ou `ETTR`) é o vínculo, o script ou a viagem dessa região (confirmado pela ordem nos arquivos).

## EEOV: o que falta decifrar
Padrões de numeração vistos (hex das lacunas, por tipo): CRT v2 com nome e pele `01 00 00 00 00 00 00 00 [nome] [script] 00000000 01 0000803f [pele] 00 01 00 [u32 n][inventário]`; CRT sem nome: 10 zeros no lugar do nome vazio; DOR/CON/USE v2: igual, com o `01` virando `00`; itens v1: 16 zeros. A reconstrução provável é: `u8 ?`, 7 bytes, `str` nome, `str` script, `u32 0`, `u8 ?`, `f32 1.0` (escala?), `str` pele, `u8 ?`, `u8 ?`, `u8 ?`, `u32` contagem de inventário, strings. **Não confirmado**: o primeiro `u8` (0..3, talvez facção/equipe), os três `u8` finais e o `f32 1.0`. O parser guarda tudo (strings classificadas + lacunas em hex), então nada se perde.

## Coordenadas mundo → grade `.rle` (corrigido no v15)
**Regra única, medida com os `.8tre`:** as posições estão no mesmo espaço de mundo da geometria do `.8tre` e `célula = (posição − origem) / 0,5`, com a origem no canto mínimo (x, z) dos vértices do `.8tre`. Resultado: 186 de 197 pontos em células não vazias (Vault 41/41, Spelunking 37/37, Chiefs 10/10, Mainmenu 3/3). Detalhes em `docs/specs/tre.md`.
> **Correção:** no v14 eu escrevi que o fator mundo→célula "não é único" (×1, ×2, ×0,5 por mapa) e que o `Mainmenu` ficava fora da grade em qualquer escala. Era um artefato: sem o `.8tre` eu não conhecia a **origem** da grade. Com a origem, o fator é sempre 2 células por unidade (célula de 0,5).

## Tipos de entidade (prefixo do protótipo)
`CRT` criatura/NPC (85), `WEA` arma (33), `AMO` munição (18), `DOR` porta (16), `CON` container (13), `ITM` item (5), `USE` objeto usável (3), `ARM` armadura (2).

## v19: chunks novos no `.f3map` (`mapexport.py`, lidos por `f3map.cpp`)
| Chunk | Conteúdo | De onde vem |
|---|---|---|
| `CAMR` | `f32[4]` | os 4 floats do `ECAM` (copiados sem interpretar) |
| `ENVI` | `u8[4]` cor A, `u8[4]` cor B, `f32[3]` | cores e floats do `EMAP` (ex.: cor A 83,88,108; floats 250, 500, 1) |

O `F3VB.exe` usa `ENVI` como **névoa linear** (cor A, início = 1º float, fim = 2º float; desliga se os dois forem 0, como no Mainmenu) e `CAMR` como limite da câmera.
As duas leituras são **hipóteses** (tecla `N` liga/desliga a névoa para comparar). Mapas convertidos antes do v19 não têm esses chunks: rode `convert-retail --maps` de novo.

