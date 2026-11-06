# Decisão de abordagem (F3VB) e revisão do openvb

Data: 03/10/2026. Baseada em MEDIÇÕES da demo real (relatórios do usuário) e na leitura do `openvb.zip` (OpenJE).

## 1. Verificação dos resultados (o que está certo e o que não está)

**Certo (reproduzido em duas rodadas):**
- 24 `.grp` únicos, `issues=0`; catálogo `.rht` x `.grp` resolveu 7044/7044 entradas, com nomes; todos os comandos saíram com código 0.
- GR2: **547/547 lidos a fundo sem erro** (Granny v6, 32 bits, Oodle0). Somando os arquivos: 106 esqueletos, 1176 ossos, 451 animações, ~847 s de animação (maior: 11,7 s), 12.331 trilhas de transformação; curvas `LegacyCurve32f` (orientação e posição) e poucas `scale_shear`.
- Scripts: 119 AMX (Pawn/Small), 300 públicas, 115 natives.

**Corrigido ou a tratar com cautela:**
- As estatísticas de B3D do `03_resumo.txt` v6 NÃO são confiáveis para os `.b3d` de terreno (Tiles). Aquele resumo tratava como "pesos de osso" bytes de uma cauda de layout diferente (1821 "superfícies com pesos" em Tiles é artefato da sonda; os tiles são estáticos). O v7 separa as famílias de superfície e só conta o que foi decodificado por completo.
- `Effects`: 12 de 13 `.b3d` ainda param no topo (tokens `0x00`, `0xE1`, `0x99`), e `CR_Badger/CR_Bear/CR_Sheep` param em `0x2F`.
- O catálogo está certo, mas lembre: é só estrutura; o conteúdo de INI, VEG, INT, RLE e `.stf` ainda não foi lido.

## 2. Estado do B3D depois da análise das amostras (10 arquivos reais)
- **Família de 44 bytes/vértice (personagens, itens, props): decodificada por completo em 6/6 arquivos**, consumindo o arquivo exatamente até o `0x0B 0x0B 0x01` final. Layout observado (a confirmar em escala com o relatório v7):
  `tag 0x0F/0x19, u8, u8, u8 flag=1, s32 numVerts, s32 sizeOfVerts(=44)`, vértices de 44 bytes (pos 3f, normal 3f, cor BGRA 4B, uv 2f, 8 bytes sem uso), 6 floats (caixa envolvente), `u8 isBones` (se 1: `s32 bonesPerVertex` e `numVerts * bonesPerVertex * (s32 osso, f32 peso)`), `s32 numMaterials`, por material: `str16 nome, str16 id, 17 floats, str16 blend, str16 tipo, 16 x s32 (o último = nº de texturas), str16 por textura, u32 extra`; depois `str16 nomes[n], s32 listaIndices[n], s32 contagens[n]`, e `listaIndices[i]` índices u16 por material; fecha com `0x0B` (por nível de nó) e `0x01`.
- **Família de 64 bytes/vértice (terreno com lightmap, 75% dos `.b3d`)**: cabeçalho e vértices leem certo; a cauda tem outro layout (nome do lightmap, `1`, nome da textura, bloco de s32 e uma corrida final de bytes `0x01`). Ainda NÃO decodificada.
- Vértice de 64 bytes (observado): pos 3f, normal 3f, cor 4f (0,5/0,5/0,5/1,0 no terreno), 2 s32 e 2 floats de UV, mais bytes por entender.

## 3. Revisão do openvb / openje (zip enviado)
- **Tamanho real do código: 998 linhas** de C++ com corpo (`F3.cpp` 416, `Display.cpp` 188, `tracing.cpp` 131, `JE.cpp` 69, `ActionAnim.cpp` 61, `Chunk.cpp` 58). Os outros ~1050 arquivos são cabeçalhos com membros e métodos nomeados por endereço (`mbr_0x18`, `meth_0x4ccc10`).
- **Não tem** carregador de B3D, GR2, AMX, `.grp` nem `.rht`. `Chunk.cpp` só lê 3 dwords (tag, versão, tamanho), o que coincide com o nosso enquadramento.
- Objetivo declarado: reescrever o código "o mais próximo possível do original", compilado com VS .NET 2003; só depois trocar Win32/DX8. É o caminho oposto ao nosso.
- **Sem arquivo LICENSE** no zip (o README diz "no commercial use is intended", o que não é licença). Uso: somente leitura e documentação. Não copiamos código.
- Valor real: a lista de classes (G3D_Mesh, G3D_Node, OCTREE_*, VFX_Model, GameScripting, GameAction*...) como mapa dos componentes do jogo, a ideia do módulo `tracing` e o contato dos autores. Nossos achados (catálogo, formatos, scripts Pawn) podem ajudá-los em troca.

## 4. Abordagem escolhida

**Importador em Python + formatos abertos + engine C++ no seu renderer + VM Pawn embutida.**

1. **Importador (Python), já em andamento:** lê `.grp/.rht` e converte, só localmente, para: **GLB** (malha, esqueleto, pesos, animação amostrada; metadados do original em `extras`), **PNG/TGA** (texturas), **JSON** (entidades, catálogo, mapas), WAV como está. A saída nunca entra no repositório.
2. **Engine (C++, seu renderer Vulkan/DX11):** carrega GLB + JSON. O formato original só é lido pelo importador, não pelo runtime.
3. **Scripts: VM própria no instruction set clássico (decisão revista após testar o Pawn 4.1).** O Pawn 4.1 (build 7487) NÃO serve: seu `amx_Init` devolve `AMX_ERR_VERSION` para os scripts reais (exige `file_version` >= 11; os da demo são 6) e, segundo o próprio `history.txt` do Pawn, a versão 4.0 reformulou o conjunto de instruções e não roda arquivos antigos. Então escrevemos a VM (`src/amx/`, C++20, ~600 linhas), com a numeração clássica de opcodes que **só ela** decodifica o código real por inteiro. Resultado: os 27 publics de dois scripts reais executam sem falhas, a VM passa em 10 grupos de testes sintéticos e em 60 mil mutações de fuzz com ASan/UBSan. Faltam as 115 natives (implementação em C++ no motor) e a confirmação nos 119 scripts (relatório v8).
4. **Fidelidade visual:** modo "Fiel" por padrão (materiais, blend OPAQUE/ALPHABLEND/ADD, lightmaps, câmera), com "Modernizado" opcional. Validação por screenshots lado a lado.
5. **Marco Alpha (fatia vertical):** 1 mapa pequeno + jogador (HumM) + 2 ou 3 criaturas com as animações do catálogo.

### Por que não as alternativas
- **Traduzir scripts para C++/Lua:** são 119 scripts compilados, sem fonte; a VM os executa como estão e preserva o comportamento.
- **Embutir o Pawn 4.x:** testado e descartado (incompatível com `file_version` 6).
- **Reescrever código decompilado (estilo openvb):** lento, juridicamente mais arriscado e não entrega o objetivo (engine moderna).
- **Rust/Bevy:** adiciona uma segunda linguagem e um renderer novo sem resolver o gargalo, que é decodificar formatos.
- **glTF como formato do runtime sem metadados:** perderia lightmaps, tipos de superfície e blend do original; por isso os `extras`.

## 5. Ordem de trabalho
1. Rodar o relatório v7 (agora com B3D por família) e confirmar a taxa de decodificação em todos os 3915 `.b3d`.
2. Decodificar a cauda da família de 64 bytes (terreno) e os tokens de Effects.
3. Escrever o exportador GLB (B3D + esqueleto + animação) e validar no Blender com 1 criatura e 1 animação.
4. Parse de `8TRE` e `EMAP` (mapa), carregar um mapa de teste no renderer.
5. Ligar a VM AMX ao motor: implementar as natives por grupos (diálogo, movimento, combate...) e rodar um script de nível simples. A VM já executa scripts reais com natives de log.
