# VM, scripts e diálogos ligados ao mapa (v17, semântica corrigida no v19)

O que existe e **o que ainda é suposição**. Os testes usam scripts AMX **sintéticos** (montados no próprio teste); o que roda em dados reais é o que você testa na sua máquina (v17: Mainmenu e Junktown abriram; o `.stf` real foi lido; o `10 Mormon` rodou as 27 publics).

## v19: semântica do diálogo medida (e corrigida)
Medido passo a passo no script **`00 Corporal Armstrong`** (o do tutorial de Junktown), com o `English.stf` real:
- **Ids de texto são 1-based**: o id `N` que o script passa é a entrada `N-1` do `.stf`. Com o índice direto (v17/v18) todas as falas saíam deslocadas em uma linha (o texto "do lado"). `Stf::text_cp1252(id)` faz a conta; `Stf::text(i)` continua sendo por índice.
- **A condição é testada pelo próprio script**: a função de texto `NUn` chama `RCn` e só então chama `RegisterReply`. Os 4 argumentos de `RegisterReply` são `(id, texto, função de AÇÃO RAn, função de DESTINO RLn)`, não `(…, condição, resultado)`. No v18 o host chamava o 3º argumento como condição, ele devolvia 0 e **todas as respostas sumiam**. Agora toda resposta registrada aparece.
- **`RegisterNodeText(id)`** é a fala do NPC naquele nó (implementada no `GameHost`: vira uma fala no painel).
- **Escolher** uma resposta: chama a ação (se houver), depois o destino (se houver); a função de texto do nó atual roda de novo. **Fim** quando nem o nó nem a função de texto mudaram (ou `EndDialogue`).
- Textos trazem códigos de cor `{cNNN}` (um COLORREF em decimal): a cor vai para `DialogLine::color`/`DialogChoice::color` e o código sai do texto (`strip_color_codes`).
- O roteiro antigo abaixo fica como histórico; onde ele divergir, vale este.

## v18: o bug que derrubava os scripts de objetos/criaturas (corrigido)
No log do v17 **todas as publics** dos scripts de criaturas/objetos (`CaptainArmstrong`, `Exploding_Barrels`, `Template_*`...) falhavam com "instrução inválida". Causa: esses scripts foram compilados **com símbolos de depuração** (`flags=2`), e o código traz instruções de tamanho variável (`FILE`, `SYMBOL`) com o nome dos símbolos no meio. A VM calculava o tamanho dessas instruções como 1 célula e **executava o texto dos símbolos como se fosse código**. Os scripts de diálogo (`10 Mormon`) são compilados sem depuração e por isso funcionavam. Agora a VM pula o `[size][payload]` de `FILE`/`SYMBOL`. Teste de regressão com um script sintético idêntico ao padrão real (`tests/test_script.cpp::make_debug_script`): falha sem a correção, passa com ela. Dois ajustes junto: a mesma native importada **duas vezes** (`floatadd`, `floatmul`) agora é registrada nas duas entradas, e o levantamento (`B`) passa 8 argumentos zero às publics (quase todas leem parâmetros como `currentGameTime`; sem isso davam "acesso inválido" falso). **Reconfira com `B`**: o esperado agora é quase nenhum erro e a lista real de natives a implementar.

## v18: de onde vem o diálogo de um NPC
O script que o mapa cita para a entidade (`CaptainArmstrong.amx`) é o de **comportamento** e **não tem `OnStartDialogue`**; o diálogo mora em outro script (os 36 `SMA_Script`: `00 Corporal Armstrong`, `10 Mormon`...). Como a engine liga os dois **ainda não sabemos**. Provisório: `V` usa o script da entidade se ele tiver diálogo; senão procura por **nome** um script de diálogo parecido (palavras do nome do script que aparecem no protótipo/instância da entidade). `K` passa para o **próximo script de diálogo** da biblioteca, em qualquer entidade (bom para ver todos os diálogos). O log lista os candidatos. Se você descobrir/me mandar um `.crt` (protótipo da criatura), dá para achar o elo real.

## Visão geral
```
convert-retail --maps / export-scripts  ->  CONVERTED\scripts\<nome>.amx   (119 scripts: tipos AMX e SMA_Script)
F3VBMapViewer: abre o mapa -> lista os scripts citados pelas entidades -> acha o .amx pelo nome
   Tab seleciona a entidade -> V inicia o diálogo (VM) -> 1..9 escolhem as respostas -> V fecha
   B roda TODAS as publics de cada script do mapa e resume erros e natives que faltam
F3VBScript ARQ.amx [--stf T.stf] [--choices 1,2] [--survey]   (o mesmo, sem janela, para depurar e mandar a saída)
```

## Peças (todas em `viewer/src`, portáveis, sem Windows/OpenGL)
| Arquivo | O que faz |
|---|---|
| `../src/amx/amx_vm.*` | A VM Pawn/Small clássica (AMX versão 6), já existente desde o M1 |
| `../src/host/game_host.*` | Camada de *natives*: 115 funções que os scripts chamam; as implementadas de verdade + as "só log". **v17:** guarda as falas (`FloatingSpeechText`, `PCSpeaker`) |
| `stf.*` | Leitor da tabela de strings (`.stf`): `id do texto -> texto`. **Layout verificado** com o `English.stf` real (3.281 textos) |
| `script_world.*` | `ScriptLibrary` (acha o `.amx` pelo nome), `ScriptInstance` (Program + Machine + GameHost), `DialogSession` (o diálogo), `survey_script` (roda todas as publics), `map_scripts` (scripts citados pelo mapa) |
| `script_main.cpp` | O `F3VBScript` (linha de comando) |

## Como o diálogo roda (roteiro do v17; corrigido pela seção v19 acima)
Deduzida dos scripts reais (`docs/DIALOGOS.md`); cada passo vai para o log (`[VM] chamou X -> ok`), então dá para corrigir com dados reais.
1. **Iniciar** = chamar a public `OnStartDialogue` (sem argumentos). Ela costuma escolher uma fala de abertura (`LA<k>_R1_N1`) que chama `FloatingSpeechText(entidade, idDoTexto, 0, R, G, B)` e `SetNodeTextFunction(...)`.
2. O **nó atual** é o último criado por `SetCurrentNodeName`. A **função de texto** do nó (`SetNodeTextFunction("NU<k>_N<k>")`) é chamada pela engine para (re)registrar as **respostas** (`RegisterReply(id, idDoTexto, "condição", "resultado")`).
3. **Condição:** se a resposta tem função de condição (`RC<k>_N<k>`), ela só aparece quando a função devolve diferente de 0 (o painel mostra quantas ficaram escondidas).
4. **Escolher** uma resposta = chamar a função de resultado (`RL<k>_N<k>`: fala do NPC, troca de nó, troca da função de texto...). Depois a função de texto do nó atual roda de novo.
5. **Fim:** função de texto vazia ou `"EndDialogue"`, ou nenhuma resposta disponível. Há trava de 64 passos contra laço infinito e limite de 2 milhões de instruções por chamada.
6. As falas do jogador via `PCSpeaker` assumem o **id do texto como 1º argumento** (suposição; continua listada como "native sem implementar").

## O que cada tecla faz no F3VBMapViewer
- `Tab`/`Shift+Tab`: seleciona entidade (a linha de seleção mostra se o script dela foi encontrado).
- `V`: abre/fecha o diálogo da entidade selecionada (painel no canto direito; falas do NPC em amarelo, do jogador em azul, respostas em verde).
- `1`..`9` (ou o teclado numérico): escolhe a resposta enquanto o diálogo está aberto.
- `B`: **levantamento**: para cada script citado pelo mapa, roda todas as publics e escreve no log: nº de publics ok/com erro, se tem diálogo, e as natives que ainda faltam. É o melhor relatório para me mandar.
- `F2`: relatório do mapa (agora inclui os scripts citados e onde foram achados).
- No log ao abrir cada mapa: pasta de scripts, quantos `.amx`, se o `.stf` foi achado, e quantos scripts do mapa foram encontrados/faltam.

## A tabela de strings (`.stf`)
O jogo traz **1 arquivo `.stf` (248,9 KB) fora dos `.grp`**, na pasta da demo. O `convert-retail` não o copia (ele só vê o RETAIL). Copie-o para `CONVERTED\scripts` (comando em `COMANDOS.md`) ou aponte a variável `F3VB_STF`. O leitor usa o layout do `stf.pat` do OpenJE (cabeçalho de 12 bytes, entradas de 16 bytes). **v19:** o id que os scripts e os `.int` usam é **1-based** (`id = índice + 1`); o `py -3 -m f3vb import-loose` copia o arquivo para `RETAIL` e `CONVERTED\scripts`. **Verificado:** o `English.stf` real tem 3.281 entradas (254.846 bytes) e a entrada 0 é `Single Player`, a 1 `Multiplayer`... (saída de `py -3 -m f3vb stf` e o log do viewer "3281 textos"). **v19, verificado:** o id que os scripts passam é o índice **+ 1** (as falas do Armstrong e os rótulos dos menus só fazem sentido assim).

## O que NÃO existe ainda
- Mundo de jogo de verdade: `Myself` devolve `1000 + índice da entidade`; as ~100 natives restantes só registram a chamada (ações `A_*`, inventário, quests, `MessageGetArg`...). O levantamento mostra exatamente quais são as mais chamadas.
- Eventos (`OnInit`, gatilhos do mapa, mensagens entre entidades), scripts do mapa (`MainMenu`, `00_03_Tutorial_Junktown`) e o laço de IA/ações dos NPCs.
- Fala flutuante no mundo 3D (hoje só no painel).
- **v19:** a interface já existe no `F3VB.exe` (veja `docs/JOGO.md`); falta ligá-la às natives de UI que `MainMenu.amx`/`Player.amx` chamam.
- A forma como a entidade do `.map` cita o script (`entidade.script`) ainda não foi vista em dado real: o casamento ignora pasta, `.amx`, maiúsculas e caracteres proibidos do Windows. Se algum script do mapa aparecer como "NÃO ENCONTRADO" no log, me mande a linha.

## Como isso conversa com o resto
`ENTS` do `.f3map` já tem `script` por entidade (v14). Nada mudou no formato do `.f3map`: o elo entidade → script é feito em tempo de execução pelo nome. Isso mantém os assets fora do projeto: os `.amx` ficam em `original_assets\CONVERTED\scripts`.
