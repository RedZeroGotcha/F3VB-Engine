# GR2 (Granny 2) na demo: o que o F3VB lê (v20)

Leitor: `viewer/src/gr2.cpp` (porte C++ do parser MIT do blendergranny; Python em `third_party/blendergranny`).
Resultado **idêntico ao Python em 183 arquivos** (dump de ossos, curvas e nós comparado linha a linha: 13.426 linhas, sem diferença).

- Versão 6, 32 bits little-endian. **Todas** as seções de dados da demo vêm comprimidas com **Oodle0** (descompressor aritmético; usamos
  inteiros de 64 bits onde o Python não tem limite e mascaramos como ele).
- Ponteiros: tabela de correções de **12 bytes** por entrada (offset na seção de origem, seção de destino, offset no destino).
- Definições de tipo: **32 bytes** por membro (tipo, nome, ponteiro para subtipo, contagem...); structs aninhadas são lidas recursivamente.
- Curvas: só `LegacyCurve32f` aparece na demo. Ela tem grau 0 a 3 e o layout `{i32 grau; u32 nº de nós; ptr nós; u32 nº de controles;
  ptr controles}`. A avaliação é em `double`, como no Python.
- Esqueleto (`RT_SKEL`) e animações (`RT_GR2`) ficam em recursos diferentes do `.rht`. Animação parada dos humanos:
  `HumM_Def_<Una|1H|2HH>_Idle_01` (ou `HumF_...`); criaturas e objetos: `<modelo>_Com_Una_Idle_01`.
- Escala: unidades do mapa = esqueleto × 10,24; a malha `.b3d` usa 100/1024 (ou 1,0 nos `.b3d` antigos "não reconvertidos", ver `b3d.md`).
