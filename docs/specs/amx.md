# Spec: scripts AMX (Pawn/Small clássico)

**Status: VERIFICADA.** Decodificação completa de **119/119** scripts reais (relatório v8) e execução de todas as publics de 2 scripts reais na VM própria. Tipos do `.rht`: `AMX` (1200, 83) e `SMA_Script`
(300, 36), ambos compilados (`.sma` é só o nome do fonte original, que fica em instruções FILE/SYMBOL, ex.: `c:\fo3\Code\F3Scripts\10 Mormon.sma`).

## Cabeçalho (file_version 6: 52 bytes, little-endian)
`i32 size` (= tamanho do arquivo), `u16 magic` (0xF1E0 = células de 32 bits), `i8 file_version` (6), `i8 amx_version` (6),
`i16 flags` (0x02 = debug), `i16 defsize` (40), `i32 cod, dat, hea, stp, cip` (cip = -1: sem `main`), `i32 publics, natives, libraries, pubvars, tags`.
O campo `nametable` só existe a partir da versão 7. Código = `[cod, dat)`; dados iniciais = `[dat, hea)`; pilha+heap = `[hea, stp)`.

## Tabelas de funções (`publics`, `natives`)
Entradas de `defsize` = 40 bytes: `u32 address` + 36 bytes de nome (terminado em NUL, **embutido**). Em `natives`, `address` = 0 e o índice da entrada é o operando de `SYSREQ.C`. Endereços de publics são byte-offsets no código e apontam para `PROC`.

## Instruction set clássico (138 opcodes, validado)
Numeração em `tools/py/f3vb/amxdis.py` (`OPCODES`) e `src/amx/amx_vm.cpp`. Só esta numeração (com `PUSH.R` e `JREL` presentes) decodifica o código inteiro: todas as publics em `PROC`, saltos em início de instrução, `SYSREQ.C` dentro da tabela. FILE e SYMBOL são `opcode, size(bytes), payload`; CASETBL é `opcode, n, default, n pares (valor, destino)`.

## Convenções de execução (observadas e confirmadas pela execução)
- Pilha cresce para baixo. Chamada de função: argumentos empurrados da direita para a esquerda, depois `nbytes` e o endereço de retorno; `PROC` empilha o frame antigo; `RETN` desempilha frame, retorno e remove `nbytes + 4`.
- A célula 0 do código é `HALT 0`: retornar ao endereço 0 encerra a chamada de nível superior.
- `STACK n`: `ALT = STK` (valor antigo), depois `STK += n` (semântica pré-4.0). `HEAP n`: `ALT = HEA`, `HEA += n`.
- Nativas: `SYSREQ.C i` chama `natives[i]` com `params[0] = nbytes` e `params[1..]` = argumentos; o compilador limpa a pilha depois com `STACK`.
- Divisão com piso: `SDIV` = `pri / alt` e `SDIV.alt` = `alt / pri`, ambos com resto em `ALT`.
- Strings: desempacotadas (1 caractere por célula) ou empacotadas (4 por célula, primeiro no byte alto).

## Dialogo (observado no script `10 Mormon`)
Publics `OnStartDialogue`, `N1`, `NU1_N1`, `NA1`, `NC1`, `RC1_N1`, `RL1_N1` e pares `LAx_R1_N1` / `LCx_R1_N1`. `LAx` chamam `FloatingSpeechText(entidade, idString, 0, 128, 128, 128)` com ids de string (3113..3122) — **ids da tabela de strings** (`.stf`); `N1` chama `SetCurrentNodeName` e `SetNodeTextFunction`; `NU1_N1` chama `RegisterReply(1, 3123, 60, 64)`; `NC1`/`RC1` são condições (devolvem 0/1). Hipótese: nós de diálogo do estilo "texto do NPC / resposta do jogador".

## Em aberto
Semântica de `JREL` (nenhum script amostrado o usa); `SYSREQ.D`/`SYSREQ.pri`; significado exato de `flags=0x02` (debug) sem tabela de debug anexada; implementar e testar as 115 nativas.
