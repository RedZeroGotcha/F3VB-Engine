# Para onde isso tudo leva (o destino do projeto, sem enfeite)

## O que existe no final
Uma **engine nova, em 64 bits, no seu renderer**, que carrega a demo técnica do Van Buren a partir da cópia que a pessoa já tem, com os **mesmos modelos, texturas, animações e scripts** de 2003, rodando de forma estável e podendo ser modificada (mods). É uma ferramenta de **preservação e modding**, não um jogo novo.

## O tamanho real do que se recria (medido)
Uma demo curta: ~39 mapas de teste (a maioria `zz_TestMaps`), 108 NPCs/criaturas definidos, 99 esqueletos, 448 animações, 119 scripts, 3.915 modelos (quase todos peças de cenário), 113 sons. Não é o Fallout 3 inteiro: é a fatia técnica que a Black Isle mostrou.

## Marcos honestos, do mais perto ao mais longe
0. **Feito (v11/v12):** extrator com interface e layout `original_assets/{RETAIL,CONVERTED}`; visualizador 3D OpenGL com log automático e análise de modelo; **causas de animação quebrada achadas e corrigidas** (escala da malha ~10,24× e posicionamento inicial do GR2). Cão e Eyebot servem de casos de debug.
0. **v19 feito (aguarda teste no Windows): o jogo começa.** `F3VB.exe` com a interface original (menu, opções, criação de personagem, barra de ações, pausa, diálogo), Junktown com câmera livre e conversa por clique, `.int` decifrado, TrueType próprio, manchas brancas do `.8tre` corrigidas, diálogos certos. **Próximo, v20:** (a) ajustar a aparência com os seus prints e o log; (b) **personagem do jogador no mapa** (modelo `.f3m` animado do v12, clique para andar no grafo `EMNP`/planta `.rle`); (c) **NPCs como modelos** no lugar das caixas (protótipo `.CRT` → modelo/pele); (d) rodar `OnCreate`/`OnUpdate` dos scripts de objetos (portas, barris) e as natives mais chamadas; (e) inventário com itens do `EEOV` e o Pip-Boy; (f) gatilhos de viagem (`ETTR`) entre Junktown e o Vault.
0. **v18 feito (testado):** texturas e lightmaps no mapa, VM corrigida, `ui-info`. **Próximo, v19:** (a) ajustar a aparência com os seus prints (lightmap, alfa); (b) implementar as natives mais chamadas que o levantamento listar e rodar `OnCreate`/`OnUpdate` dos objetos no mapa (portas, barris, janelas); (c) **decifrar os `.int`** (layouts de tela) e as fontes `.fnt` para desenhar o menu principal e a criação de personagem (`Player.amx`); (d) achar o elo entidade -> script de diálogo (`.crt`).
0. **v17 feito (testado: abriu mapas, VM rodou o 10 Mormon): VM ligada ao mapa** (`V` diálogo da entidade, `B` levantamento, `F3VBScript`, `export-scripts`). **Próximo, v18: texturas** (TGA, difusas, lightmaps `.ctx`, `TXUV`/`LMUV`, `OPAQUE` × `ALPHABLEND`), depois o que os logs de scripts mostrarem (natives mais chamadas, `A_*`, eventos, gatilhos).
0. **v16 feito (aguarda teste no Windows):** lotes de índices do `.8tre` (cada triângulo → material, página de lightmap e textura difusa; fim dos espinhos), `.f3map` v2 e modos de cor no `F3VBMapViewer` (tecla `M`). **Próximo, v17:** **desenhar com texturas** (decodificador de TGA 24 bits em C++, difusas buscadas em `RETAIL` por nome, lightmaps `.ctx` → `<nome>.tga`, usando `TXUV`/`LMUV`; mistura `OPAQUE` × `ALPHABLEND`), depois `FLGS`/hierarquia do `.8tre`, protótipos `.CRT` (para trocar caixas por personagens), VM ligada ao mapa (gatilhos, scripts).
0. **v15 feito (testado por você: abriu o Mainmenu):** `.8tre` decodificado, `.f3map` e `F3VBMapViewer.exe` (escolher mapa e ver em 3D).
0a. **v14 feito:** `.map` e `.rle` decodificados e validados (37 e 38 arquivos reais). **Próximo, v15:** `.8tre` (geometria 3D dos mapas), imagens `_AM`, números do `EEOV` e tipos de tile; abrir um mapa de verdade no Scene Viewer (marcos 13.2 a 13.6).
0b. **Em andamento, v13 (marco 13.1 pronto):** consertos dos humanos, RPGGL no repositório com teste, ferramentas de amostra. Marcos 13.2 a 13.6 em `docs/RPGGL_ANALISE.md` (seção 4). Descrição original do v13: "segunda engine" de teste junto do Viewer (OpenGL, com física embutida, trazida pelo autor): analisar todos os arquivos extraídos e convertidos (mapas, NPCs, criaturas), ligar a VM de scripts e tentar abrir **um mapa de verdade, ou ao menos a intro/menu 3D**. Antes de misturar código dessa engine: registrar a licença em `docs/LICENCAS_E_DIREITOS.md` (seção 5). Em paralelo: cauda do B3D, `.rle`, mapas `8TRE`/`EMAP`.
1. **Hoje (feito):** ler todos os arquivos, nomear os 7.044 recursos, exportar modelo + esqueleto + animação + textura para GLB, rodar scripts reais na VM e reconstruir diálogos.
2. **Próximos (semanas/poucos meses):** terminar o B3D de terreno nos 2.928 tiles, ver personagens animados no Blender, ler `.stf` para mostrar os textos, ler mapas (`8TRE`/`EMAP`).
3. **Meio (meses):** carregar um mapa no seu renderer, andar com um personagem, tocar animações e executar scripts de nível (as 115 natives, por grupos).
4. **Fim (muitos meses, meio período):** a fatia jogável: clique para andar, combate em tempo real, diálogo funcionando, interface, som. Paridade visual "Fiel" validada por screenshots.

## O que o projeto NÃO é
- Não distribui nada do jogo: cada pessoa usa a própria cópia.
- Não é um jogo completo nem o Fallout 3 cancelado, só a demo técnica.
- Não é um parecer jurídico: a demo é material proprietário e vazado; o uso aqui é para interoperabilidade, preservação e estudo.

## Onde pode dar errado (riscos reais)
- Partes dos formatos ainda sem decifrar (terreno 64 bytes em escala, efeitos, mapas, VEG, RLE, vídeo).
- Dúvidas de escala/espelhamento/pose que só o Blender e o seu renderer confirmam.
- Falta de tempo: é muito trabalho para uma pessoa; por isso cada passo é pequeno, testável e documentado.
- Risco legal: manter dados do jogo fora do repositório (trava automática: `scripts/verificar_sem_assets.py`), não compartilhar a cópia extraída com link aberto, e registrar toda licença de terceiros (`docs/LICENCAS_E_DIREITOS.md`).

## Quando considerar "pronto"
Você abre a demo no seu renderer, entra num mapa, vê o personagem animado, conversa com um NPC e o diálogo (texto e respostas) sai dos scripts originais.
