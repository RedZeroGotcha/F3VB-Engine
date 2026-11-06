# F3VB x OpenJE: comparação honesta (2026-10-14)

**Fontes desta comparação e limites.** (1) Minha revisão de 03/10/2026 do `openvb.zip` enviado pelo autor (`docs/DECISAO_ABORDAGEM.md`). (2) A leitura de hoje, pela web, do perfil do mantenedor e da página `github.com/OpenJE/openvb` (README, lista de arquivos, contadores). **Não abri** `openje-re`, `openje`, `auto-re`, `tracing`, o banco BinSync nem o site de documentação, e não li o histórico de commits. Tudo o que digo sobre o OpenJE vale só para o que está listado abaixo. Nada do código deles foi copiado (ver `docs/LICENCAS_E_DIREITOS.md`).

## Atualização (2026-10-19): o zip `openvb.zip` enviado pelo autor foi lido por dentro
1.511 arquivos, 28 MB. Os cabeçalhos de classe (por exemplo `Map` com 136 membros, `AreaMap`, `AreaMapNote`, `MapSound`, `MapEffect`, `G3D_WaterTile`, `OCTREE_*`) listam **membros por deslocamento e métodos por endereço, sem nomes de campo**; o código C++ tem 998 linhas; a documentação `docs/je/*.md` são páginas-índice. **Não há nada sobre o formato do `.8tre`** (nenhuma das tags `VTXD`, `LVLD`, `LGTD`... aparece). Os nomes de classe só confirmam interpretações nossas (`AreaMapNote` ↔ marcador `EMNO`, `AreaMap` ↔ imagem `_AM`). O `.8tre` foi decifrado por nós, só com os dados.

## O que o OpenJE é (confirmado hoje)
Organização com 15 repositórios públicos sobre a demo do Van Buren, entre eles: `openje-re` (Prolog, "engine reversa"), `openje` (C++, "Jefferson Engine aberta"), `openvb` (C++), `auto-re` ("ferramenta de engenharia reversa automatizada com apoio de LLM"), `tracing`, `openje-bs.bsproj` (banco BinSync) e um site de documentação. O `openvb` tinha **84 commits e 18 issues abertas**. O README diz que o objetivo é **reescrever o jogo em C++ "o mais próximo possível do código original"**, que as etapas de inicialização estão parcialmente reconstruídas e **compilam e executam**, que usam IDA e Ghidra, que compilam com Visual Studio .NET 2003 no Windows XP, e que só depois trocariam Win32/DirectX 8 por APIs portáveis. Ele traz o aviso de que não é afiliado às empresas dos direitos e de que "não há intenção comercial"; **não vi um arquivo de licença** (um aviso de uso não é licença).
Na minha revisão de 03/10 do zip: ~998 linhas de C++ com corpo; sem carregador de B3D, GR2, AMX, `.grp` ou `.rht`; `Chunk.cpp` só lia tag, versão e tamanho.

## Duas abordagens diferentes (não é a mesma corrida)
| | **OpenJE** | **F3VB** |
|---|---|---|
| Ponto de partida | o **executável** (decompilação com IDA/Ghidra) | os **arquivos de dados** do jogo |
| Objetivo declarado | reescrever o código original de forma fiel | importar os dados para formatos abertos e rodar numa engine moderna, com a VM de scripts |
| O que produz hoje (pelo que vi) | estruturas, classes e funções documentadas; inicialização parcialmente reimplementada | extrator, conversores, visualizador, análise e leitores testados |

## Onde o F3VB está à frente (medido e testado por nós)
- **Dados utilizáveis hoje, ponta a ponta:** 7.044 recursos extraídos e indexados; 1.472 modelos convertidos e animados no visualizador (cão, Eyebot e humanos validados); texturas em PNG; esqueletos e animações do GR2 lidos **sem a DLL** (547/547); 119/119 scripts AMX decodificados e uma VM própria que roda scripts reais; **37/37 mapas e 38/38 grades `.rle` decodificados** (com re-codificação idêntica no `.rle`).
- **Achados de conversão que tornam os modelos animáveis:** escala da malha ≈10,24× o esqueleto (7 modelos), posicionamento inicial do GR2, ossos com sufixo `_1`, animações de humanos por família (`HumM_*`/`HumF_*`).
- **Qualidade de engenharia:** 100+ testes automáticos, log e análise por modelo, trava contra assets no pacote, documentação com rótulos de confiança.
- **Não verifiquei** se o OpenJE já documentou esses mesmos pontos (as listas de padrões do ImHex deles que usamos como referência de campos podem cobrir parte).

## Onde o OpenJE está à frente (ou onde nós não chegamos)
- **O comportamento real do motor:** classes, métodos e fluxo de inicialização do executável. Nós **não** temos nada do código do jogo; só inferimos formatos pelos dados. Isso é decisivo para o que ainda está em aberto no F3VB.
- **Ferramental de engenharia reversa do binário** (banco BinSync, ferramenta automatizada, tracing) e um projeto com mais tempo, histórico e issues.
- **Fidelidade:** a reimplementação "o mais próxima possível" tende a reproduzir comportamentos sutis (física, scripts, IA) que o nosso caminho (reimplementar a partir dos dados) só descobre testando.

## O que isso significa na prática
Os **buracos do F3VB são exatamente o que o código do carregador do executável responde**: a "cauda do B3D" (2.444 modelos que não convertem), os números do `EEOV` das entidades, o formato do **`.8tre`** (octree da geometria dos mapas), o significado dos ids de tile do `.rle` e de onde vem o fator 10,24. Ler essas funções no `openje-re`/`openvb` (somente leitura) pode destravar semanas de tentativa e erro. Em sentido contrário, as nossas especificações (`docs/specs/`) e os testes podem servir a eles para validar as deles.
**Sugestão:** contatar os mantenedores (o README do `openvb` indica Discord e e-mail do mantenedor), propor troca de achados e **perguntar qual é a licença** do repositório. Até haver licença, mantemos o uso como **só leitura e fatos reescritos com palavras próprias**.
