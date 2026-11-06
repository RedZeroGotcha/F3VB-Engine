# Formato `.rle`: grade de tiles do mapa (recurso tipo 1700 "RLE")

**Estado:** decodificado e validado nos **38 arquivos reais** da demo (Maps). Re-codificar a grade reproduz os 38 arquivos **byte a byte** (`tests/test_maps.py` cobre o layout com dados sintéticos; a validação nos arquivos reais foi feita fora do projeto). Código: `tools/py/f3vb/rle.py`.

## Layout (medido)
```
u32 a, u32 b, u32 c     a = largura em celulas; b = numero de linhas + 1; c = a (igual a `a` em todos os 38 arquivos; significado desconhecido)
u8  fill                sempre 0x2F ('/'): o valor "vazio"
N x { u32 contagem, u8 valor }     runs, linha por linha
```
- A grade tem **a colunas e (b − 1) linhas**: a soma das contagens é exatamente `a × (b − 1)` nos 38 arquivos.
- **Nenhum run atravessa o fim de uma linha**; runs vizinhos de mesmo valor na mesma linha são sempre fundidos (por isso o re-encode é idêntico).
- Tamanhos: de 44×43 a 644×643 (33 dimensões distintas). `a` costuma ser `4 + 10·k` (44, 84, 94, 104, 124, 134, 214...), o que sugere uma borda de 2 células de cada lado; não confirmado.

## O que os valores significam
- **Valores usados nos 38 arquivos:** 0, 1, 2, 4, 5, 9, 11, 12, 16, 22, 23, 24, 26, 27, 31, 33, 34, 35, 38, 42, 44 e **47 (0x2F) = vazio**.
- **Hipótese (padrão medido):** `valor = 11 × nível + tipo`, com nível 0..4 e tipo 0..10 (os valores 11, 22, 33, 44 aparecem sempre como "tipo 0" de níveis 1 a 4).
- **Evidência visual (medida):** `convert-retail --maps` desenha a grade com cor por tipo/nível e sobrepõe as entidades do `.map`. Em `00_04_Tutorial_Vault` a grade é **a planta do cofre**: salas e corredores cheios, **paredes = células vazias (47)**, portas e blocos de mobília em tiles de outros valores. As entidades, os nós de navegação, os caminhos e o ponto de entrada caem **dentro das salas**. Conclusão de trabalho: a grade é a **camada de piso/ocupação por célula**, não a geometria 3D (essa fica no `.8tre`).
- Mapas grandes (Mainmenu etc.) têm contorno em losango (arranjo isométrico) com cantos vazios.
- Não sabemos ainda: o nome de cada tipo/nível (ex.: qual valor é "porta"), nem a relação com o conjunto de tiles do `.8tre`.

## Relação com os outros arquivos
O chunk `EMAP` do `.map` cita `<Nome>.rle` (esta grade), `<Nome>.8` (o recurso `TRE_Octree`, `.8tre`) e `<Nome>_AM.tga|dds` (imagem de mapa de área). Nome do `.rle` ≠ nome do `.map` (o `.map` costuma ter prefixo `00_`, `zz_TestMaps`...): sempre ligue pelo nome dentro do `EMAP`. **4 dos 37 mapas** enviados não têm `.rle` correspondente no zip.
