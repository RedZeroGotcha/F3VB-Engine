# Spec: `.b3d` (modelo), versão da sonda

**Status:** derivada de `b3d.pat` (OpenJE/patterns), reescrita; **não verificada em arquivo real**. A sonda (`f3vb.probes.probe_b3d`) só cobre o início do arquivo.

- Começa com `signature[8]` (esperado: `B3D ` + versão).
- Depois vem um fluxo de tokens de 1 byte. Os lineares e usados pela sonda: `0x03` escala global (f32), `0x04` escala de coordenadas (f32), `0x05` sistema de coordenadas (string u16), `0x06` três s32, `0x16` s32, `0x1C` flag de versão (u8), `0x35` flag legado (u8), `0x07` material, `0x08/0x09` textura (u8, nome, arquivo, largura, altura), `0x0E/0x18` osso (nome, u16, flag; translação se flag != 0; rotação se flag == 3; float extra se flag == 2), `0x01` fim.
- `0x0A/0x13` iniciam um SceneNode (nome, transformação, superfícies com vértices de 44 ou 64 bytes, pesos de osso, materiais de superfície). **Não implementado**: o próprio pattern avisa que parte do material de superfície tem cauda variável.
- Strings "u16" = comprimento u16 + bytes. O pattern usa `SizedString` sem tipo em alguns tokens (0x0C, 0x17, 0x1B): o tamanho do prefixo precisa ser confirmado.

Incertezas a fechar com amostras: prefixo das strings sem tipo, tamanho exato de `sizeOfVerts`, cauda variável do material de superfície, ordem dos ossos x nós.

## v20: variante "64 bytes com pesos" e leitor C++
- O leitor C++ (`viewer/src/b3d.cpp`, porte de `b3d.py`) tenta duas leituras de cada superfície. Na **A** (padrão), o byte de flag vem antes da
  contagem de vértices. Na **B**, esse byte faz parte da contagem.
- Variante nova, achada no vagão do menu (`IT_MainMenu_boxcar`): vértices de **64 bytes sem o byte de flag**, cujos 12 bytes finais são
  `(s32 1, s32 osso, f32 peso)`. Esses `.b3d` antigos ("não reconvertidos") estão em escala **1,0**, e não 100/1024; `guess_mesh_scale`
  escolhe comparando com o esqueleto.
- Superfícies da família de 64 bytes herdam o `id` (shader) do material de topo de mesmo nome. Sufixos do id: `*_2X` = modular ×2,
  `*VERTEX_ALPHA*` = mistura pelo alfa do vértice, `*IMAGE_ALPHA*` = corte de alfa.
- Ainda não lida: `DS_Mesa_Console.b3d` ("numVerts" absurdo na superfície em 377; o Python falha igual).
