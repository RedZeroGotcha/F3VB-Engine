# Formato `.8tre`: geometria 3D de um mapa (recurso tipo 400 "TRE_Octree")

**Estado (medido em 38 arquivos reais, 106 MB, enviados pelo autor):** os 38 arquivos são lidos **até o último byte** (a árvore tokeniza exatamente até o fim). **v16:** o fluxo de índices de cada nó é um fluxo de **lotes** (material, página de lightmap, textura difusa; gramática abaixo): 5.925 nós com índices, **0 que não fecham**, **1.928.117 triângulos**, **0 índices fora da caixa da folha** (no v15 eram 1.946.527 triângulos e 0,94% fora da caixa: ~18.400 "triângulos" eram palavras de cabeçalho lidas como índices, os *espinhos* que cruzavam o mapa). Código: `tools/py/f3vb/tre.py`. Testes com dados sintéticos: `tests/test_tre.py`.
O OpenVB/OpenJE (zip enviado, lido em 2026-10-17) **não** ajuda neste formato: os cabeçalhos `OCTREE_*` são esqueletos (membros por deslocamento, métodos por endereço), sem nomes de campo; nada foi copiado.

## Enquadramento
Chunks `tag[4]` + `u32 tamanho do PAYLOAD` (cabeçalho de 8 bytes, aninhados; tags de A–Z, 0–9, `_`). Raiz `8TRE`; o tamanho dela cobre o arquivo menos 8 menos **4 bytes por folha** (veja abaixo).

| Chunk | Conteúdo | Confiança |
|---|---|---|
| `HEAD` | 4 × `u32`: (2, 4–14, 2–6, N). O primeiro é a versão; os outros sem nome | versão e tamanho: medido |
| `LGTD` | `INFO{u32 N}` + N × `LGTR` de 64 bytes: `u32 tipo`, `u32`, cor `f32×3`, posição `f32×3`, direção `f32×3`, `f32×2`, cor 2 `f32×3` (opcional) | layout medido; significado de cada campo, hipótese |
| `LVLD` | `INFO` de 16 bytes: `u32 largura`, `u32 altura`, `f32 0,5`, `f32 0,5` + `LVL0`... (grade de níveis, **não decodificada**) | `INFO` medido; **tamanho da célula = 0,5 em todos os 38** |
| `MATD` | `INFO{u32 N}` + N × `MATR` de 196 bytes: `u32`, nome[80], RGBA, mistura[16] (`OPAQUE`, `ALPHABLEND`...), superfície[16] (`METAL`, `CONCRETE`, `SILENT`...), floats, `u32` flags | strings e cor: medido |
| `TXTD` | `u32`, `u32`, `TXTR{nome\0}`... (`.tga` e `.ctx`, este último provavelmente lightmap) | medido |
| `VTXD` | `u32 N` blocos + N × `VTXB`: `u32 nv` e fluxos `XYZ ` (3 f32), `NRML` (3 f32), `DIFU` (RGBA8), `TXUV` (2 f32), `LMUV` (2 f32), cada um `tag[4]` + `u32 bytes` + dados | **medido** (blocos de até 63.887 vértices) |
| `TREE` | sequência **plana** de `NODE` e `LEAF` em busca em profundidade | **medido** |

### `NODE` e `LEAF`
- `INFO` do `NODE` (64 bytes): caixa mín/máx (6 f32), `u32 0`, `u32` (máscara/contagem, sem significado confirmado), 4 × `u32` que são **endereços de memória do jogo** (lixo), zeros.
- `INFO` da `LEAF` (32 bytes): caixa mín/máx (6 f32), **`u32` = índice do bloco de vértices** (0..N−1), `u32 1`.
- Opcional `IDXD`, contendo `IDXS` (**v19:** os 12 bytes iniciais NÃO são lixo: são 6 × `u16` = o cabeçalho do primeiro lote, veja abaixo; depois o fluxo de `u16`, **ver "Fluxo de índices em lotes"**) e `FLGS` (**1 byte por triângulo**, valores 0, 1, 4 vistos; o **significado** não é conhecido, mas a **contagem** vale: total de índices dos lotes = 3 × `len(FLGS)`). `NODE` internos podem ter só o `INFO`.
- **O tamanho declarado de cada `LEAF` vem 4 bytes menor que o real** (os 4 bytes finais pertencem à folha). A próxima ficha começa em `início + 8 + declarado + 4`. Para `NODE` o tamanho é exato. Prova: com essa regra os 38 arquivos terminam exatamente no EOF; o total de bytes "sobrando" depois da raiz é 4 × número de folhas (ex.: `Test_Int10x10_Sewers1`, 1.311 folhas = 5.244 bytes).
- A hierarquia (qual nó é filho de qual) **não é necessária para desenhar** e não foi decifrada: os nós têm até 8 filhos e o número de filhos varia (tentamos 4 fixo, 8 fixo e popcount da máscara: nenhum fecha em todos os arquivos). As caixas dos filhos ficam dentro da do pai e se sobrepõem um pouco (octree "frouxa").
- Folhas de mapas maiores têm, depois do `IDXD`, uma matriz extra de bytes (0, 4, 5; 3 bytes por triângulo no exemplo medido) **sem significado conhecido**.

### v19: os 12 bytes iniciais do `IDXS` são o cabeçalho do primeiro lote
Do v15 ao v18 esses 12 bytes eram lidos como "3 × `u32` de lixo" e o começo do fluxo virava um **lote inicial implícito** (material 0, sem página, sem textura). Isso deixava sem textura e sem lightmap os primeiros triângulos de cada nó (2.577 no Mainmenu, 18.367 no Junktown): eram o paredão cinza atrás da estação e parte das manchas claras do chão.
Lidos como 6 × `u16`, os 12 bytes são `[0x8000|material][A] [0x4000|página][B] [k][C]`, exatamente a gramática abaixo. Exemplos reais: `(0x8000, 8, 0x4001, 6, 1, 3)`; `(0x8005, 40, 0x4000, 38, 3, 36)`; Junktown `(0x8000, 28, 0x4020, 26, 1, 24)`.
**Prova (medida em `Mainmenu.8tre` e `03_Tutorial_Junktown.8tre`):** `C` = tamanho do "lote inicial" do v16 em **60/60 e 339/339 nós**; lendo o fluxo inteiro (cabeçalho + índices) pela gramática desde a posição 0, **todos os nós fecham** (com o total = 3 × `len(FLGS)`), os triângulos são os mesmos (24.168 e 157.602), 0 fora da caixa, e **0 triângulos ficam sem material/página/textura**. Os casos em que `B > C + 2` são páginas com mais de um sublote (o cabeçalho só mostra o primeiro).
**Leitor:** `tre.py::split_full` tenta primeiro a gramática completa; se não fechar (ou se o cabeçalho não começa com `0x8000`, como nos testes sintéticos antigos), cai no `split_batches` do v16. `tre-check` mostra quantos nós fecharam pela gramática completa e quantos triângulos ficaram "sem estado" (esperado: todos e 0, nos 38 arquivos; **reconfira no seu PC**).

### Fluxo de índices em lotes (v16)
O `IDXS` (depois dos 12 bytes de cabeçalho) é uma sequência de `u16`, alinhada a 4 bytes (se a posição em `u16` fica ímpar há 1 `u16` de enchimento, esperado = 0):

```
fluxo   = [lote inicial, sem cabeçalho] [grupo]*
grupo   = [0x8000 | material][A] página+     ou     página+      (sem o token de material: continua o anterior)
página  = [0x4000 | página][B] sublote+
sublote = [k][C] índices[C]   (+1 enchimento se C é ímpar)        C é múltiplo de 3 (C/3 triângulos)
```
`A` = total de `u16` das páginas seguintes do grupo; `B` = total de `u16` dos sublotes da página (com enchimento). **material** = índice 0-based em `MATD` (o lote inicial é o material 0, implícito); **página** = índice **1-based** na lista `.ctx` do `TXTD` (0 = nenhuma; é a página de lightmap `<nome>.tga`); **k** = índice 1-based na lista `.tga` do `TXTD` (0 = nenhuma; textura difusa; *forte, mas não 100% provado*). Pode haver um sublote "nu" `[k][C]` sem cabeçalho de página (herda material e página). O lote inicial tem `L0 = 3·len(FLGS) − soma dos lotes seguintes` índices.

**Ambiguidade e como o leitor a resolve.** Os 2 bits altos dos cabeçalhos (`0x8000`, `0x4000`) colidem com índices de vértice válidos de blocos com mais de 16.383 vértices (chegam a 63.887). `tre.py::split_batches` valida cada cabeçalho por **intervalo** (material < nº de `MATR`; página ≤ nº de `.ctx`; k ≤ nº de `.tga`), por **fechamento exato** das contagens (`B`, `A`, `C`) e pelo **total = 3 × len(FLGS)**; testa cada posição par como início do primeiro cabeçalho e aceita o primeiro tiling exato. **Novo no v16 final:** tenta primeiro o modo *estrito* (os `u16` de enchimento têm de ser 0), que elimina fechamentos falsos (um par (k, C) dentro do lote inicial que "engolia" uma palavra de cabeçalho como enchimento); se nada fechar, cai no modo frouxo (o do v16 inicial). O modo estrito foi validado só com dados sintéticos: reconfira nos arquivos reais com `py -3 -m f3vb tre-check PASTA`.

**Evidências (medidas nos 38 arquivos, no v16 inicial):** o maior valor de página == nº de `.ctx` em todos os mapas grandes (Furnace 81, Spelunking 78, Junktown 34, Vault 13...); k ≤ nº de `.tga`; material < nº de `MATR`; 5.925 nós com índices e 0 não fechados; 1.928.117 triângulos; 0 índices fora da caixa da folha; 0 triângulos com material fora da tabela.

### Índices (rede de segurança)
`TreFile.triangles()`/`triangles_ex()` ainda aceitam um índice só se ele cai no bloco e dentro da caixa da folha; senão tentam `& 0x3FFF`, `& 0x7FFF`, `& 0xBFFF`. Com os lotes isso não deveria mais ser necessário (0 corrigidos esperados); fica como proteção contra arquivos de outra versão.

## Relação com a grade `.rle` e com o `.map` (regra única, medida)
- Posições do `.map` (entidades, navegação, entradas...) estão **no mesmo espaço de mundo** da geometria do `.8tre`.
- **`célula do .rle = (posição − origem) / 0,5`**, com a origem = canto mínimo (x, z) dos vértices do `.8tre`. Resultado: **186 de 197 pontos caem em células não vazias** (Vault 41/41, Spelunking 37/37, Chiefs 10/10, Mainmenu 3/3). Isso **substitui** o "fator por mapa" do v14 (ele era um artefato de não conhecer a origem).
- `LVLD.INFO`: largura × 0,5 e altura × 0,5 dão a extensão do mundo (ex.: `Junktown` 384 × 624 células = 192 × 312 unidades; a caixa dos vértices tem 190 × 310). Em `Junktown` e `Vault` o `.rle` tem `(largura, altura − 1)` do `LVLD`; em outros mapas de teste as dimensões não batem (o `.rle` citado pelo `.map` parece ser de outro mapa).

## Texturas e imagens dos mapas (medido com `TGA_Image.zip`, 555 arquivos)
- **Todos os 555 TGA decodificam** (24 bits, sem compressão): 498 de 256×256, 17 de 512×512, 1 de 128×128 e 39 `_AM`.
- **`<Mapa>_AM.tga` = mapa de área (automap):** 39 imagens, preto = piso e **verde = paredes** (fundo verde puro fora do mapa), no mesmo desenho da grade `.rle`. **Dimensões = `LVLD.INFO (largura, altura)`** em 32 de 36 pares conferidos (1 pixel por célula de 0,5 unidade; as 4 exceções são os mapas de teste cujos `.rle` também não batem). Ex.: `Junktown` 384×624, `Vault` 384×334.
- **`TXTD`: nomes `.ctx` = páginas de lightmap, guardadas como `<nome>.tga`:** das 494 referências `.ctx` nos 38 `.8tre`, **494 (100%) existem como `<nome>.tga`** no zip (`MarkTest_0.ctx` → `MarkTest_0.tga`, 256×256 na maioria). Os 84 nomes `.tga` citados (32 distintos: `Int_Cave1_Rock_Brown`, `PS_Town`...) são texturas difusas que **não** estão neste zip (vêm de outros packs). 516 dos 555 TGA são essas páginas.
- **v16:** cada lote diz a página de lightmap (índice na lista `.ctx`) e a textura difusa (índice na lista `.tga`) que seus triângulos usam; `LMUV`/`TXUV` dão as coordenadas. Falta *desenhar* com elas (v17).

## O que ainda não se sabe
Os `FLGS` (só a contagem é conhecida), o significado exato de `k` (índice de textura difusa, forte mas não 100% provado) e do material do lote inicial (assumido 0), os bytes extras das folhas, a máscara do `NODE`, a grade `LVL0`, a origem exata da grade (a caixa dos vértices acerta 94,4%, não 100%).
