# Como os diálogos funcionam (observado nos scripts reais)

Ferramenta: `dialog_dump ARQ.amx` (C++; executa o script na VM com a camada de natives e imprime a árvore).
Script de exemplo analisado: `10 Mormon` (27 publics).

- `OnStartDialogue`: sorteia um número (`RandomNumber(1,10)`) e chama uma das falas de abertura `LA<k>_R1_N1`.
- `LA<k>_R1_N1`: `FloatingSpeechText(entidade, idDoTexto, 0, 128, 128, 128)` (texto do NPC, cor RGB cinza) e depois `SetNodeTextFunction("EndDialogue")`.
- `N<k>`: cria o nó (`SetCurrentNodeName("Node 1")`) e define a função que atualiza o nó (`SetNodeTextFunction("NU<k>_N<k>")`).
- `NU<k>_N<k>`: registra respostas do jogador: `RegisterReply(idResposta, idDoTexto, "condição", "resultado")`; condição vazia = sempre disponível.
- `RC<k>_N<k>` (condição da resposta, devolve 0/1), `RL<k>_N<k>` (resultado: fala do NPC, mudança de nó...).
- Funções de script são citadas **por nome**, como textos na memória do script (1 caractere por célula), não por endereço.
- Os `idDoTexto` (ex.: 3113 a 3123) apontam para a tabela de strings (`.stf`): `py -3 -m f3vb stf "...\*.stf" --id 3113 --id 3123`.

Natives já implementadas de verdade: floats (`float`, `floatadd`, `floatsub`, `floatmul`, `floatdiv`, `floatcmp`, `floatround`, `floatfract`), `RandomNumber`, `Myself`, `GetGlobal`/`SetGlobal`, `SetCurrentNodeName`, `SetNodeTextFunction`, `RegisterReply`, `FloatingSpeechText`. As outras são "somente log" (registram nome e argumentos e devolvem 0), e o `dialog_dump` mostra quais faltam.

**v17:** o diálogo agora roda dentro do visualizador de mapas (`V` na entidade selecionada) e no `F3VBScript` (sem janela). Fluxo, suposições e limites: `docs/VM_E_DIALOGOS.md`. `FloatingSpeechText` e `PCSpeaker` passam a guardar a fala; `PCSpeaker` assume o id do texto como 1º argumento (suposição).
