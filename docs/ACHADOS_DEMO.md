# Achados na demo real (medições da cópia do usuário)

Fonte: relatórios gerados por `scripts/rodar_tudo.ps1` na pasta da demo. Tudo abaixo foi MEDIDO, salvo quando marcado como hipótese.

## Pasta
- 36 arquivos, 763.957.024 bytes listados (24 `.grp` únicos + a cópia duplicada); **158 MB são uma cópia duplicada** de `Sounds.grp` (`data\Sounds.grp` e `data\Demo\data\Sounds.grp`). Tamanho real ≈ 570 MB.
- 24 `.grp` únicos = 559,4 MB. Também: 2 `.mve` (6,7 MB), `F3.exe` (3,0 MB), 2 `.dll` (uma é `granny2.dll`, que NÃO usamos), `resource.rht`, 1 `.stf` (248,9 KB), 3 `.ttf`, 1 `.txt`. Sem `F3.ini` e sem pasta `Override`.

## Lumps por `.grp` (total 7044 = entradas do `.rht`)
Tiles 2979 · Critters 1574 · Maps 668 · Interface 453 · Props 437 · Items 226 · Effects 147 · Sounds 129 · Scripts 121 · _CRT 108 · _WEA 51 · _CON 48 · Engine 25 · _ARM 22 · _AMO 20 · _USE 18 · _DOR 13 · _ITM 5 · (vazios: _CHM, _Dev, _ESF, _MSC, _RLZ, Templates).

## Modelos e animação
- **547 `.gr2`**: Critters 437, Items 91, Props 18, Engine 1. **Todos** Granny formato 6, ponteiros de 32 bits, seções `none + oodle0`. 448 são animações e 99 são esqueletos (= tipos GR2_Animation e SKEL_Skeleton do `.rht`).
- Os 7 `.gr2` analisados a fundo decodificaram **sem erro** com o parser vendorizado (Oodle0 incluído). Nenhum tem malha (`geometry = 0`): só esqueletos e animações; curvas no formato legado `LegacyCurve32f`.
- **3915 `.b3d`** (Tiles 2928, Critters 456, Props 387, Items 131, Effects 13), assinatura `B3D 1.1`, `coordinate_scale = 1024`, sistema `LEFT_HANDED`. Materiais com ids tipo `BASE_VERTEX_ALPHA_2X`, blend `OPAQUE/ALPHABLEND/ADD`, tipo de superfície `SILENT/SAND/CONCRETE/GRASS/STONE/METAL`.
- Personagens: 453 de 456 `.b3d` de Critters têm ossos no topo (Root, Pelvis, Spine_1, Head, L/R UpperArm...). Os nomes das texturas sugerem **corpos modulares** (prefixos Bac, Bea, Bod, Mus...). Rotação de osso lida como 4 floats com o último valor parecendo ângulo em graus (hipótese: eixo + ângulo).
- A sonda de B3D chega ao primeiro `SceneNode` em ~99% dos arquivos (Tiles 2928/2928, Items 131/131, Critters 453/456, Props 385/387, Effects 5/13). Tokens inesperados: `0x2F` (3 em Critters), `0x00`, `0xE1`, `0x99` (Effects/Props): a investigar.

## Texturas
1614 candidatos a TGA, **todos plausíveis** (24/32 bpp, alguns RLE; tamanhos comuns 64x64, 32x32, 128x128, 256x256, 512x512). 39 lumps de tipo TGA_Image são na verdade arquivos **BMP** (começam com `BM`: 10 em Engine, 29 em Interface). *Correção:* antes eu os associei ao tipo RLE; o catálogo mostrou que não.

## Scripts (Pawn/Small confirmado pela estrutura)
- 119 AMX em `Scripts.grp` (83 do tipo AMX + 36 do tipo SMA_Script), todos com o mesmo cabeçalho: `file_version 6, amx_version 6, defsize 40, magic 0xF1E0`.
- **Relatório v8: 119/119 scripts decodificados por completo** pelo instruction set clássico (436.888 instruções; 76 dos 138 opcodes usados; todas as publics em `PROC`, saltos e `SYSREQ` válidos). As **115 natives são todas chamadas** em algum script (`docs/NATIVES.md`). Os 62 opcodes que nenhum script usa (ex.: `SDIV`, `JLESS`, `SHL`, `CMPS`, `JREL`) só foram exercitados pelos testes sintéticos.
- 300 funções públicas e **115 `natives`** (a API do jogo para scripts), incluindo as de biblioteca de float do Pawn (`float`, `floatadd`...). Públicas com padrão `LA1_R1_N1` / `LC1_R2_N3` sugerem **nós de diálogo** (hipótese). A lista completa fica em `03_report.json` (pasta `private`).

## Outros
- Sounds: 113 `.wav` + 16 textos. Effects: 119 lumps `bin` (= os 119 VEG), 15 TGA, 13 B3D. Interface: 393 TGA, 29 BMP-like, 25 `bin` (= 25 INT).
- Maps: 37 `EMAP` (+1 em Engine) = 38 MAP; 38 `8TRE` (+1 em Engine) = 39 TRE; os 38 lumps `bin` de Maps (+1 em Engine) são o tipo **RLE** (39), um por mapa, com o mesmo nome do mapa (ex.: `MarkTest`, `Test_Int_Vault13`).

## Catálogo `.rht` x `.grp` (VALIDADO nos seus arquivos)
- 7044 de 7044 entradas resolvidas, 7044 nomes de recurso lidos, 0 lumps sem entrada, espaçamento dos offsets 7043/7043 coerente: `offset` = posição do nome do recurso na região de recursos.
- Em todos os 18 packs: nº de entradas = nº de lumps e `number` é uma permutação de 0..n-1.
- Tipo x conteúdo: concordância quase total. Exceções: G3D_Model 3915 b3d + 1 texto; TGA_Image 1614 tga + 39 bmp; INI_Config 123 texto + 2 vazios.
- Nomes de recurso perdem o separador de pasta: `Weapons` + `9mm_Autoloader` = `Weapons9mm_Autoloader`; `DS_Props` + `VaultBox_Blue`. Chaves como `{23}` ou `{45}` aparecem em nomes de modelos (`Male_Outfits{23}MusHermit`).
- Os `.gr2` guardam o caminho original de arte do autor dentro do arquivo (ex.: `M:\Art\Critters\Creatures\...`), útil para reconstruir a árvore de pastas.

## Personagens, esqueletos e animações (catálogo)
- **99 esqueletos**: 17 de criaturas (CR_Ant, CR_AntQ, CR_Bat, CR_Beetle, CR_Centipede, CR_Cougar, CR_Cow, CR_DesertStalker, CR_Dog, CR_Gila, CR_Mantrap, CR_RadToad, CR_Rat, CR_ThornSlinger, CR_WaspGiant, CR_Weedling, CR_Deathclaw), 3 sem animação própria (CR_Badger, CR_Bear, CR_Sheep) e CR_FloatingEyeBot; **humanos**: HumMSkl e HumFSkl + 8 variantes de corpo (HumM/HumF + Nor, Fat, Str, Wir); mais ~60 `WP_*` (um esqueleto por arma), `IT_*` (itens) e `DS_*` (portas) e `DefaultSkeleton`.
- **448 animações**: 184 humanas (`HumM_`/`HumF_` + modo Com/Def/ProneB/ProneF + classe de arma Una/1H/2HH/2HC + ação + variante) e ~264 de criaturas e outros (11 a 13 por criatura). As variantes de corpo humano não têm animações próprias: provavelmente **reaproveitam** as de HumMSkl/HumFSkl (hipótese; validar comparando listas de ossos).
- 110 dos 125 `INI_Config` têm o mesmo nome de uma animação (metadados por animação; conteúdo ainda não lido).
- Um arquivo de esqueleto (tipo SKEL) traz o esqueleto e uma "animação" vazia (0 trilhas): o caso `Critters#117` (CR_FloatingEyeBot) **não é bug**.
- 108 definições de NPC/criatura (`_CRT`, ex.: `Critters00_TMBobby`), 51 armas, 22 armaduras, 20 munições, 48 containers, 18 usáveis, 13 portas, 5 kits.
- **Mapas**: 38 mapas de teste em `zz_TestMaps` (ex.: `Test_Int_Vault13`, `MarkTest`, `Test_Ext5x5`, `Test_Building`) + `Default_StartMap` em Engine.

## GR2 em leitura profunda (7 arquivos reais; o relatório novo faz todos os 547)
- 0 erros; Oodle0 decodificou. Curvas de animação no formato legado `LegacyCurve32f` (orientação e posição). Duração das animações amostradas: até ~2 s.

## B3D: estado medido no relatório v8 (sonda anterior ao terreno)
| `.grp` | `.b3d` | decodificados por completo | observação |
|---|---:|---:|---|
| Items | 131 | 129 (98%) | 2 falhas (lumps 0 e 19) |
| Critters | 456 | 438 (96%) | 18 falhas: strings absurdas, lista de índices fora do arquivo, token `0x2F` (3 criaturas) |
| Props | 387 | 296 (76%) | 91 falhas: 4 de terreno, o resto variantes de material |
| Tiles | 2928 | 1 | terreno (família 64 bytes): cauda ainda não decodificada na época |
| Effects | 13 | 0 | tokens `0x00/0xE1/0x99` no topo e variantes |
Total 864/3915 (22%). **No v9 a família de 64 bytes (terreno) foi decodificada** nas amostras reais (8 de 10 exatas); o relatório v9 mede o efeito em todos os 2928 Tiles.

Layout do terreno (observado): vértices de 64 bytes; `u32 0`; `str16` lightmap (ex.: `LM_32_Floor`); `s32 n`; `n x str16` textura; `u32 0`; `u8 0xC9`; `s32 kk`; `kk x s32` índices (kk = 3 x triângulos); `kk x u8` (= 1).

## Scripts (complemento) e a VM AMX
- Os 36 `SMA_Script` (nomes de NPC: `00 Corporal Armstrong`, `00 Frank`, `04 FastHands`) também são AMX compilado; os 83 AMX têm nomes de mapa/nível (`MainMenu`, `00_03_Tutorial_Junktown`, `00_Animals_Dog_Stray`).
- Magic `0xF1E0` = células de **32 bits**. Cabeçalho de 52 bytes (versão 6, sem `nametable`), nomes embutidos nas tabelas (`defsize 40`).
- **Pawn 4.1 não roda esses scripts** (`AMX_ERR_VERSION`). A VM clássica própria roda: ver `docs/specs/amx.md` e `docs/DECISAO_ABORDAGEM.md`.
- Script real `10 Mormon` (2 amostras): 523 instruções, 27 publics, 6 natives (`RandomNumber`, `SetCurrentNodeName`, `SetNodeTextFunction`, `RegisterReply`, `Myself`, `FloatingSpeechText`). Cada public de fala chama `FloatingSpeechText(entidade, idDeString, 0, 128, 128, 128)`: os ids (3113...) apontam para a tabela de strings (`.stf`).

## Exportador, leitor de B3D e diálogos (v10)
Ver `docs/EXPORTADOR_GLB.md` e `docs/DIALOGOS.md`. Resumo: o leitor completo de B3D lê 8 de 10 amostras reais (as 2 que faltam são do tipo Effects); o exportador gera GLB válido com malha, texturas, esqueleto e animações; a camada de natives executa um script real de diálogo e reconstrói a árvore (nós, respostas, textos por id).

## Em aberto
1. Cauda da superfície do B3D (materiais e triângulos) e os tokens inesperados (Effects, Badger/Bear/Sheep).
2. Formatos `.rle` (um por mapa), VEG (119), INT (25), `.stf` fora dos `.grp`, `.mve` (vídeo).
3. Mapa: parse de `8TRE` e `EMAP` (geometria e entidades colocadas).
4. Retarget das variantes de corpo humano (comparar ossos de HumMNor/Fat/Str/Wir com HumMSkl).
5. Leitura profunda dos 547 GR2 (o relatório novo já faz) e conteúdo dos 125 INI de animação.
6. Implementar as 115 natives; confirmar a decodificação dos 119 AMX (relatório v8) e a semântica de `JREL`.

## Extrator, F3M e visualizador (v11)
Ver `docs/VISUALIZADOR.md`. O formato F3M (`tools/py/f3vb/f3m.py`) é lido pelo núcleo C++ do visualizador (`viewer/src`); animação e skinning em C++ coincidem com o Python de referência com erro ≤ 5e-7, inclusive com o esqueleto e a animação reais do Eyebot. O deslocamento constante de ~0,012 unidades entre a pose local e o `InverseWorld4x4` do Granny continua sem explicação (provável `InitialPlacement` do modelo, que o parser vendorizado não expõe); o visualizador usa o `InverseWorld4x4` (como o jogo original).

## Extração real na máquina do usuário (v11, 2026-10-11)
`extract-all --png --f3m`: **7.044 arquivos originais gravados (559,3 MB) em 136,7 s**, PNG 1.653, F3M 1.472 modelos + 98 esqueletos. As 2.444 "falhas" são conversões de B3D para F3M que ainda não decodificam (3.915 B3D - 1.472 F3M = 2.443, mais 1 lump de texto marcado como G3D_Model, `CR_Monsta`), não falhas de extração. Exemplos no relatório: strings absurdas e listas de índices inválidas em Critters, `numVerts` absurdo em Props/Items/Effects, token `0x2F` em Badger/Bear/Sheep. É o item "cauda do B3D" já em aberto.

## v12: escala da malha, posicionamento inicial e diagnóstico (2026-10-12, cão e Eyebot)
- **Escala (medido):** vértices de peso dominante >= 0,9 ajustados ao segmento do osso dono: melhor fator `k` = **10,15–10,35 (cão)** e **10,3–10,5 (Eyebot)** (mínimo bem definido: no cão, `k=1024` dá erro médio ~7× maior que o melhor). O esticamento das arestas em animação tem mínimo largo em `k` ≈ 9,5–10,5. **Hipótese:** `k = 1024/100 = 10,24` (o B3D tem `coordinate_scale = 1024`). Só 2 modelos medidos: a análise do visualizador estima `k` por modelo para conferir nos demais.
- **Efeito (medido):** com malha e esqueleto em escalas diferentes, a pose de bind fica certa e a animação estica a malha: razão pose/bind das arestas p95 de 3,7 a 8,7 no cão; com 1/10,24: 1,25 a 1,7 (Idle/Walk/Attack/Defesa). Isto explica o "cão bugado com animação" e o Eyebot com peças soltas.
- **Posicionamento inicial (medido):** Eyebot: `mundo × InverseWorld` = translação **(0,0117; 0; 0,0087)** idêntica nas 8 juntas, sem rotação; cão: zero. Compensando nas raízes, a pose de bind skinada reproduz a malha (erro ~1e-7). Interpretação ("InitialPlacement" do Granny) é hipótese; o dado é medido.
- **Esqueleto do Eyebot:** a hierarquia tem 8 juntas (grupo raiz `CR_FloatingEyeBot_4ani`, `Root`, `Bone01..03`, `Weapon_1..3`); a razão das arestas pose/bind é 1,00 nas animações testadas, ou seja, as peças parecem rígidas (inferido, pesos não listados um a um), então só o deslocamento das juntas e o afastamento entre peças mostram o erro, não o esticamento.
- **Extração real (usuário, v11):** 7.044 originais (559,3 MB) em 136,7 s; 2.444 conversões B3D ainda falham (cauda do B3D).
- **Ainda em aberto:** valor exato da escala (outros modelos), pesos/retarget humano, cauda do B3D, terreno nos tiles, `.rle`, `.stf`/`.mve`, mapas `8TRE`/`EMAP`.

## v13: testes do usuário no Windows e achados nos humanos (2026-10-13)
**Confirmado no Windows pelo usuário (v12):** build e testes ok; cão e Eyebot animam de forma convincente (log do cão: sem avisos, `f` estimado ~1,0). **Medido** nos logs dos modelos humanos (a escala 1/10,24 se confirma em mais 5 modelos):
| Modelo | Fator extra estimado `f` (1 = escala certa) |
|---|---|
| HumFStr | 1,0065 |
| HumFSkl | 0,9507 |
| HumMWir | 1,0231 |
| HumMNor | 1,0214 |
| HumMFat | 1,0198 |
Junto com o cão (≈1,0) e o Eyebot (0,98), são **7 modelos** dentro de ±5%: a hipótese `1024/100` ganha força (ainda não é prova, mas agora tem 7 pontos).

**Bug dos ossos 3D "esqueleto" (HumFSkl), medido no log:** o B3D cita `L Clavicle`, `L Fingers`, `R Clavicle`, `R Fingers` e o esqueleto GR2 tem `L Clavicle_1`, `L Fingers_1`, `R Clavicle_1`, `R Fingers_1` (também `R Handle_1`, `L Handle_1`). Sem casamento, 236 influências ficavam sem junta; o visualizador somava só os pesos que casavam e os braços **colapsavam para a origem** (as "lâminas" da captura, já na pose de bind). Corrigido no v13: (1) conversor e visualizador casam por sufixo (`X` ↔ `X_1`, maiúsculas); (2) osso realmente ausente deixa aquela parte do peso no bind, sem colapsar.

**Animações dos humanos (medido pelos nomes no Drive):** não existem `HumMNor_*`. As animações são compartilhadas por família: `HumM_Com_Una_Walk_01`, `HumM_Def_1H_Idle_01`, `HumM_ProneB_Una_Death_01`... (padrão `Hum[MF]_<Postura>_<Arma>_<Ação>_NN`). Por isso os humanos saíam com `animacoes=0`. Corrigido no v13: busca por `NomeDoModelo_` e pela família `HumM_`/`HumF_`; o F3M de esqueleto (`f3m\skeletons\`) leva **todas** as animações da família. `HumFSkl` usa o esqueleto feminino (raiz `Female_Collision`), então usa `HumF_*` (não confirmado se a família feminina tem o mesmo conjunto; confira com `export-list GR2_Animation --prefix HumF_`).

**Textura dos humanos (hipóteses, ainda sem prova):** todos os corpos têm UV só em `u,v ∈ [0, 0,5]` (ex.: `Skin_HumF_default_LG`, TGA 256×256 de 24 bits, 196.652 bytes = 196.608 + cabeçalho + rodapé: tamanho real, não é 512 reduzido). O corpo aparece com "manchas" pretas e rosto sem traços (captura do HumFStr). Possíveis causas: (a) a pele é só uma camada: o jogo sobrepõe outra textura (rosto/roupa/outfit; os nomes de malha são `Female_Outfits{1}Base Guy`) e o nosso F3M só usa a **primeira** textura de cada material; (b) as manchas pretas são regiões mascaradas por roupa; (c) um deslocamento de UV por material que o leitor B3D ainda não lê. **Para decidir:** rodar `py -3 -m f3vb b3dmat HumFStr` (lista todas as texturas de cada material, blend, floats e faixas de UV) e/ou `py -3 -m f3vb pack-sample HumFStr HumMNor` e enviar o zip.

**As 2.444 falhas da extração (explicação):** são **conversões de B3D para F3M que o leitor ainda não consegue** (o arquivo original foi extraído normalmente; 7.044/7.044 gravados). Causas das 200 primeiras listadas: `token de nó desconhecido` 71 (o leitor perdeu o sincronismo no meio da árvore de nós), `numVerts absurdo` 91 (só em Props/Effects), `token de topo desconhecido` 13 (ex.: CR_Badger), `lista de índices inválida` 8 (ex.: CR_Cougar), `contagem de texturas absurda` 16 (Tiles e CR_Weedling), `string absurda` 7 (CR_Bat), 1 arquivo que não é B3D (`CR_Monsta`). Por pack, nas 200: Props 91, Tiles 75, Critters 19, Effects 13, Items 2. É a "cauda do B3D" (formato com campos ainda não decifrados após a lista de materiais). No v13 o relatório passa a trazer o **resumo por causa e por pack de todas as falhas** e a lista completa em `FALHAS_COMPLETO.txt`.

## v14: mapas (`.map`) e grades (`.rle`) decodificados (2026-10-14)
Amostras enviadas pelo autor: `MAP_Map.zip` (37 `.map`, 49.914 bytes) e `RLE.zip` (38 `.rle`, 536 KB). Especificações completas: `docs/specs/map.md` e `docs/specs/rle.md`.
- **`.rle` (medido, 38/38):** cabeçalho `u32 a, b, c` + `u8 0x2F` + runs `(u32 contagem, u8 valor)`; grade de `a` colunas × `b−1` linhas; nenhum run cruza a linha; **re-codificar reproduz os 38 arquivos byte a byte**. Valores 0..44 e 47 (vazio). Visto como imagem, o `Tutorial_Vault` é a **planta do cofre** (paredes = células vazias): é a camada de piso/ocupação.
- **`.map` (medido, 37/37):** lista plana de chunks `tag + versão + tamanho` (os 37 arquivos cobertos sem sobra). **14 tipos de chunk decodificados por inteiro** (0 byte sobrando): cabeçalho com 3 nomes de arquivo + cores, câmera, 175 entidades colocadas (posição, yaw, protótipo, script, pele, inventário), 35 efeitos, pontos de entrada, **grafo de navegação** (14 nós, 28 arestas), regiões de gatilho (script, viagem entre mapas, vínculo), caminhos de patrulha, sons, marcadores e água (56 remendos). Ficam **4.800 bytes de números sem nome** dentro das sobrescritas de entidade (`EEOV`), guardados em hexadecimal.
- **Validação cruzada:** sobrepondo entidades, nós de navegação, caminhos e pontos de entrada sobre a grade, tudo cai dentro das salas (imagem `CONVERTED\maps\*.png`). Isso confirma que `x,z` são coordenadas do plano do mapa e que o `.rle` é a planta. **(Corrigido no v15: o "fator por mapa" era um artefato; a regra é única, ver abaixo.)**
- **Viagem entre mapas:** `Junktown` → `Vault` (entrada 1) e `Vault` → `Junktown` (entrada 2): os gatilhos `ETTR` carregam o nome do mapa de destino.
- **O que são os `.tga` e os `.8tre` da pasta `Maps` (hipóteses a partir dos nomes citados no `EMAP`):** o `EMAP` de cada mapa cita `<Nome>.8` (o recurso `TRE_Octree`, o `.8tre`: octree com a **geometria 3D** do mapa; a lista de classes do OpenJE tem `OCTREE_*`) e `<Nome>_AM.tga|dds` (a imagem de **mapa de área**, provavelmente o minimapa/automap). Os `.tga` de `Maps` seriam essas imagens `_AM`; o `.8tre` é o próximo alvo (v15). A pasta pesa >100 MB principalmente pelos `.8tre`.
- **4 dos 37 `.map` não têm `.rle` no zip** (`Kihantest`, `Test_Building`, `Junktown_fences_metas`, `Weapon_Ammo_Item_Test1`) e há `.rle` sem `.map` (`Test_City_Building05_b`).
- **Mainmenu:** (v14, **corrigido no v15**) as 3 entidades (`Default_PC`, o Cel. Armstrong, um vagão) caem dentro da grade (3 de 3) quando se usa a origem do `.8tre`; não é uma cena com coordenadas próprias.

## v15: geometria 3D dos mapas (`.8tre`) decodificada (2026-10-19)
Amostras: `TRE_Octree.zip` (38 `.8tre`, 106 MB) e `TGA_Image.zip` (555 `.tga`, 119 MB). Especificação: `docs/specs/tre.md`.
- **Estrutura (medido, 38/38 até o último byte):** `HEAD`, `LGTD` (luzes), `LVLD`, `MATD` (materiais), `TXTD` (texturas), `VTXD` (blocos de vértices com fluxos `XYZ `, `NRML`, `DIFU`, `TXUV`, `LMUV`) e `TREE` (sequência plana de `NODE` e `LEAF` com listas de índices `u16`).
- **Dois achados que destravaram a leitura:** (1) o tamanho declarado de cada `LEAF` vem **4 bytes menor** que o real (a soma dessas diferenças é exatamente o "resto" de 16 a 5.244 bytes que sobrava depois da raiz); (2) os "01 01 01 01" que pareciam marcadores de filho vazio eram o fim do `FLGS`.
- **Validação da geometria:** 1.946.527 triângulos nos 38 arquivos; **99,06% dos índices caem dentro da caixa da folha** (*corrigido no v16: os 0,94% restantes eram cabeçalhos de lote*); no `MarkTest` os 256 vértices formam uma grade 16×16 com espaçamento 2,5 e as arestas dos triângulos medem 2,5 e 3,54 (a diagonal). Só 0,08% dos índices têm flags nos 2 bits altos (*corrigido no v16: não eram flags e sim cabeçalhos de lote; ver abaixo*).
- **Regra única mundo → grade (medido):** `célula = (posição − origem) / 0,5`, origem = canto mínimo da geometria; `LVLD.INFO` traz o tamanho da célula (0,5 em todos). 186 de 197 pontos em células ocupadas. Isso corrige o "fator por mapa" do v14.
- **Mapas completos:** `convert-retail --maps` agora também grava `CONVERTED\maps\<mapa>.f3map` (34 mapas com geometria; 69 MB no total; 12 s). O leitor C++ lê os 34 (1.659.759 triângulos) com o mesmo resultado do Python.
- **Materiais dos mapas (medido):** cada `MATR` tem nome (`SK_BASE_LM_2X`, `NO_TEXTURE`...), cor RGBA, modo de mistura (`OPAQUE`, `ALPHABLEND`) e superfície (`METAL`, `CONCRETE`, `SILENT`...). Texturas `.tga` e `.ctx` (lightmap?). **A ligação triângulo → material foi encontrada no v16 (abaixo).**
- **OpenVB/OpenJE (zip enviado, 1.511 arquivos, 28 MB):** os cabeçalhos de classe (`OCTREE_*`, `Map`, `AreaMap`, `AreaMapNote`, `MapSound`, `MapEffect`, `G3D_WaterTile`...) listam membros por deslocamento e métodos por endereço, sem nomes de campo, e o código C++ tem 998 linhas: **nada sobre o formato do 8TRE**. Os nomes de classe confirmam as nossas interpretações (`AreaMapNote` ↔ marcador `EMNO`; `AreaMap` ↔ imagem `_AM`).
- **TGA (medido):** 555/555 decodificam (24 bits). **39 `_AM`** = automap (preto = piso, verde = paredes), com dimensões = `LVLD.INFO` em 32 de 36 pares. **494 de 494 referências `.ctx` dos `.8tre` existem como `<nome>.tga`**: são as páginas de lightmap (256×256). As texturas difusas citadas (`.tga`, 32 nomes) estão em outros packs.
- **Pendências:** ligação triângulo → material e → página de lightmap; `FLGS`; bytes extras das folhas; grade `LVL0`; hierarquia dos nós; origem exata da grade (94,4%, não 100%); texturas difusas dos outros packs.

## v16: lotes de índices do `.8tre` (2026-10-26; medido no chat anterior, nos 38 arquivos; reconferir com `tre-check`)
Especificação: `docs/specs/tre.md` ("Fluxo de índices em lotes").
- **O problema dos espinhos:** no Mainmenu (v15) havia triângulos longos atravessando o mapa. Causa: o fluxo `IDXS` de cada nó não é só uma lista de índices: tem **cabeçalhos de lote** no meio, e o v15 lia essas palavras como índices (as "flags ocasionais 0,08%" e os "0,94% fora da caixa da folha" eram isso; ~18.400 triângulos-fantasma).
- **Gramática (medida):** `[lote inicial] [grupo]*`; grupo = `[0x8000|material][A] página+`; página = `[0x4000|página][B] sublote+`; sublote = `[k][C] índices` (+1 de enchimento se `C` é ímpar). Material = índice em `MATD`; página = índice 1-based na lista `.ctx` (página de lightmap); `k` = índice 1-based na lista `.tga` (textura difusa; forte, não 100% provado). Total de índices = 3 × `len(FLGS)` (agora usado como restrição).
- **Evidências:** o maior valor de página é igual ao número de `.ctx` em todos os mapas grandes (Furnace 81, Spelunking 78, Junktown 34, Vault 13...); k ≤ nº de `.tga`; material < nº de `MATR`; **5.925 nós com índices, 0 não fechados; 1.928.117 triângulos; 0 índices fora da caixa da folha** (antes 5,784 milhões de índices com 0,94% fora); 0 triângulos com material fora da tabela.
- **Ambiguidade:** os 2 bits altos colidem com índices de vértice de blocos com mais de 16.383 vértices (blocos vão a 63.887); o leitor valida por intervalo, fechamento exato e total. **Achado deste chat (dados sintéticos):** sem exigir que o enchimento seja 0, um par (k, C) plausível dentro do lote inicial podia "engolir" uma palavra de cabeçalho como enchimento e ainda fechar o total. O v16 final tenta primeiro o modo estrito e cai no frouxo se necessário. **Falta confirmar nos arquivos reais** que o modo estrito fecha os mesmos 5.925 nós (e que o número de triângulos continua 1.928.117 ou muda só por correção).
- **Ainda em aberto:** significado de `FLGS` (1 byte por triângulo: 0, 1, 4) e dos bytes extras das folhas; `LVL0`; hierarquia dos nós; material do lote inicial (assumido 0); origem exata da grade (94,4%); desenhar com texturas (v17).

## v18: o que os logs do v17 mostraram (medido por você no Windows)
- **Espinhos acabaram:** Mainmenu 0 de 24.168 triângulos com aresta maior que 20% da diagonal; `tre-check` = 5925 nós / 0 não fechados / 0 fora da caixa / 1.928.117 triângulos nos 38 `.8tre` reais (igual ao medido no chat anterior: o modo estrito de enchimento fecha os mesmos nós).
- **Materiais do Mainmenu (6):** `SK_BASE_LM_2X`, `NO_TEXTURE`, `BASE_2X`, `BASE_LM_2X`, `NO_TEXTURE_VERTEX_ALPHA` (**ALPHABLEND**), `BASE_2X`; superfícies `SILENT`. Só 1 das 2 páginas de lightmap é usada (Mainmenu_0) e 2 das 3 difusas (`MainMenu01`, `PS_MainMenu01`). **Junktown: 48 materiais**, superfícies `CONCRETE`, `SAND`, `SILENT`; nomes como `BASE_IMAGE_ALPHA_2X` (provável recorte de alfa: foliagem) e sufixo `_LM_2X` (base × lightmap × 2).
- **`.stf` real:** `English.stf` = 3.281 textos, 254.846 bytes; entradas de 16 bytes (offset, length, constante `7e e3 03 00 00 00 00 00`); cabeçalho começa com `03 00 00 00 01 00 00 00`; entrada 0 = "Single Player", 1 = "Multiplayer", 2 = "New Game", 3 = "Load Game", 4 = "Join Game" (o layout hipotético do OpenJE está certo).
- **Scripts de criaturas/objetos são compilados com depuração** (`flags=2`; do `Template_DOOR` lido de uma cópia da demo: 13 publics, 26 natives, 5 variáveis públicas, `FILE`/`LINE`/`SYMBOL`/`SRANGE` no código, `CASETBL` presente). O `10 Mormon` (diálogo) não tem depuração. Ver `docs/VM_E_DIALOGOS.md`.
- **Entidades do Junktown:** 35 entidades; as de script `CaptainArmstrong`, `Exploding_Barrels`, `Shattering_Window` etc. são scripts de **comportamento** (11-13 publics `OnCreate`, `OnUpdate`, `OnDamage`...), sem `OnStartDialogue`. Marcadores do viewer: vermelho = `CRT` (criaturas/NPCs), laranja = armas, azul = portas, amarelo = containers.

## v19: interface, fontes, sons e textos (medido nos seus arquivos)
- **`.int` (pack Interface, `INT_Interface`):** 25 de 25 decodificados, 1.596 widgets; versões 1.1, 1.2 e 1.3 no mesmo jogo. Telas de jogo usam y 0..604; a `Actionbar` tem 164 px e fica em y 604; `CharacterGeneration` ocupa 1024×768; `MessageBox` é centralizada. Spec: `docs/specs/int.md`.
- **Texturas citadas que não existem** em lugar nenhum da demo (`zzmainpicture-*.bmp` nos `.int`, `zzmainlabel-*.tga` nas skins): o jogo simplesmente não desenha; o F3VB faz o mesmo.
- **Fontes soltas** em `<demo>\Fonts`: `r_fallouty.ttf` ("Fallouty"), `TT0807M_.TTF` ("Gothic 821 Condensed BT", com `kern`), `JH_FALLOUT.TTF` ("JH_Fallout"). O `fontlist.ini` cita também Arial, Arial Black, Lucida Console, Times New Roman e Hellbound Smooth, que não vêm com a demo (substituições em `docs/JOGO.md`).
- **`English.stf`:** ids usados por scripts e `.int` são **1-based**; textos trazem `{cNNN}` (COLORREF decimal).
- **Sons da interface** (`RETAIL\Sounds\WAV_Audio`): `f3_button_down_01`, `f3_button_up_01`, `f3_messagebox_open`, `f3_messagebox_close`; ambiente `AMB_WindBlowing`; músicas `MA1_*`.
- **`.8tre`:** os 12 bytes iniciais do `IDXS` são 6 `u16` do cabeçalho do primeiro lote; `C` = tamanho do antigo "lote inicial" em 60/60 nós (Mainmenu) e 339/339 (Junktown).
- **Diálogo** (`00 Corporal Armstrong`): `RegisterReply(id, texto, ação RAn, destino RLn)`; a condição `RCn` é testada pela função de texto antes do registro; `RegisterNodeText(id)` = fala do NPC.
