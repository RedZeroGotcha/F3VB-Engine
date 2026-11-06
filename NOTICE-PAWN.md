# Aviso sobre o Pawn

A VM em `src/amx/` é **código próprio**, escrito para executar scripts AMX no instruction set clássico
(Pawn/Small `file_version` 6, células de 32 bits). Nenhum código do Pawn foi copiado.

Como referência de **semântica** (divisão com piso e resto, comportamento de `LCTRL`/`SCTRL`, `MOVS`, `CMPS`,
`FILL`, `BOUNDS`, `HALT`) consultei o código-fonte do **Pawn 4.1 (build 7487)**, da CompuPhase, licenciado
sob Apache-2.0 com uma cláusula de exceção (ver `LICENSE` e `NOTICE` do pacote deles). O Pawn 4.1 NÃO executa os
scripts da demo: exige `file_version` >= 11 e, desde a versão 4.0, o instruction set foi reformulado e não é
compatível com arquivos antigos (informação do `history.txt` do próprio Pawn). Testamos: `amx_Init` do Pawn 4.1
devolve `AMX_ERR_VERSION` para os scripts reais.

Pawn é marca/produto da CompuPhase (https://www.compuphase.com/pawn/). Este projeto não é afiliado a ela.
