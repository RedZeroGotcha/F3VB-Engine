# Fontes consultadas (preencher a cada spec)

| Spec | Fonte | Como foi usada |
|---|---|---|
| (geral) | OpenJE/patterns README (lista de formatos, enquadramento de chunk, tabela de tipos do resource.rht) | Só fatos: nomes de formatos, `tag[4]+version+size`, ids de tipo. Nada de código copiado |
| grp, rht | `OpenJE/patterns`: `includes/je/file/grp.pat`, `rht.pat` (zip enviado pelo usuário, sem LICENSE) | Só o layout (campos e tamanhos), reescrito. Nenhum código copiado |
| chunk, b3d, gr2 | `OpenJE/patterns`: `comp/chunk.pat`, `res/b3d.pat`, `res/gr2.pat` | Leitura para entender; ainda sem implementação além do chunk |
| b3d (sonda) | `OpenJE/patterns`: `res/b3d.pat` | Fluxo de tokens de topo (globais, materiais, texturas, ossos) reescrito; parse de SceneNode/Surface NÃO implementado |
| gr2 | `Rasetsuu/blendergranny` (MIT), vendorizado em `third_party/` | Parser Python puro de header, seções, descompressão, malha, esqueleto, animação; assinaturas de 16 bytes (constantes factuais) |
| tga, bmp | Especificações públicas dos formatos | Sondas de cabeçalho |
| escala malha x esqueleto, posicionamento inicial (v12) | **Observação própria** nos arquivos reais do cão e do Eyebot (ajuste numérico e produto `mundo × InverseWorld`) | Medido por nós; a ligação do deslocamento com o "InitialPlacement" do Granny é só uma hipótese de interpretação, sem consultar o SDK |
| log e análise do visualizador (v12) | Código próprio (`viewer/src/log.*`, `analyze.*`) | Nenhuma fonte externa; estatísticas padrão (percentis, distância ponto-segmento) |
| layout `original_assets` (v12) | Decisão do autor do projeto | Organização de pastas; sem dependência externa |
| `.map` e `.rle` (v14) | **Observação própria** em 37 `.map` e 38 `.rle` reais enviados pelo autor | Layouts medidos e validados por nós (re-codificação byte a byte do `.rle`; 0 byte sobrando nos 14 chunks do `.map`); alguns nomes de tag já estavam nas listas do OpenJE (só fatos) |
| comparação com o OpenJE (v14) | Leitura de `github.com/OpenJE/openvb` e do perfil do mantenedor em 2026-10-14 (README, lista de arquivos, contadores) | Só fatos de descrição pública; não abri `openje-re`, BinSync nem o site; nenhum código copiado |
| `.8tre` e imagens `_AM`/lightmaps (v15), lotes de índices (v16) | **Observação própria** em 38 `.8tre` e 555 `.tga` reais enviados pelo autor | Layout, regra mundo→célula e ligações medidos e validados por nós; `openvb.zip` lido por dentro (só esqueletos de classe, nada usado) |
| `.stf` (tabela de strings), scripts compilados com depuração (v18) | **Observação própria** no `English.stf` e nos `.amx` reais do autor (Drive compartilhado por ele) | Layout do `.stf` conferido com o `stf.pat` do OpenJE; `FILE`/`SYMBOL` vistos nos bytes |
