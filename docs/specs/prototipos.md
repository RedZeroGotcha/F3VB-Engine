# Protótipos de entidade (.CRT, .ARM, .USE, .DOR, .WEA...) (v20)

Leitor: `viewer/src/proto.cpp`. Cada protótipo é uma sequência de chunks (`tag[4]`, `u32 versão`, `u32 tamanho com cabeçalho`).
O que o F3VB usa (lê 106 de 108 `.CRT` da demo):

| Chunk | Conteúdo usado |
|---|---|
| `EEN2` | nome do modelo base (ex.: `HumMNor`), pele (`Skin_HumM_Black01_LG`), retrato |
| `EEOV` | strings com papel: inventário, script, nome... (`at` + tipo + texto) |
| `GCRE` | SPECIAL (7 × u32, a partir do offset 0xb4 nas fichas salvas), idade, perícias (v4 protótipo: só as ≠ 0; v5 ficha salva: as 13) |
| partes | partes de aparência `<Encaixe><Nome>` (`HaiBald`, `BodPrisonSuit`...) e texturas próprias de parte (`BacPrisonSuit_13_LG`) |
| `GWAM` | armadura e arma equipadas (protótipos `.ARM` / `.WEA`) |
| `GCHR` | nome do personagem |

Encaixes das partes (3 letras): `Hai Pon Mus Bea Bod Fee Bac Van Hea Han Sho Eye`. A parte vira o modelo `<base>_<parte>` (ex.: `HumMNor_HaiBald`);
se não existir para aquele corpo, usa a do `HumMNor`. As partes da **armadura** substituem as do mesmo encaixe. Uma parte esconde as regiões
do corpo base que ela cobre: `Bod` esconde peito, cintura e braços; `Fee` esconde pernas, canelas e pés; `Han` esconde mãos e antebraços.
Ela também esconde a região cuja pele ela mesma traz.

Atlas de textura dos humanos: o motor compõe uma textura de 512. A pele ocupa [0;0,5]², o `Bod` [0;0,5]×[0,5;1] e as outras partes células de
0,25. O F3VB desfaz isso refazendo as UVs de cada malha (`unatlas_human` em `actor.cpp`) e usa as texturas separadas.
