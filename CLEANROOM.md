# Protocolo clean-room (versão solo)

1. **Dados do jogo nunca entram no repositório.** A demo fica fora do repo; o caminho vem de `F3VB_GAME_DIR` ou de um argumento de CLI. `private/` e `original_assets/` (RETAIL + CONVERTED) são ignoradas pelo git, e `scripts/verificar_sem_assets.py` falha se algo do jogo aparecer no projeto (o `empacotar.py` roda essa checagem). **Não compartilhar a cópia extraída com link aberto** (Drive, nuvem, chat público): isso é distribuir conteúdo protegido.
2. **Saídas do Ghidra são privadas.** Exportações de funções, strings e pseudocódigo do `F3.exe` ficam em `private/` e não são commitadas nem coladas em specs.
3. **Specs próprias.** Para cada formato, escreva `docs/specs/<formato>.md` com suas palavras, a partir do que você observou no ImHex com os SEUS arquivos. O código só implementa a spec.
4. **Registre as fontes.** `docs/SOURCES.md` lista o que cada spec consultou (patterns, docs, observação própria).
5. **Código de terceiros:** registrar em `docs/LICENCAS_E_DIREITOS.md` ANTES de entrar (licença, origem, o que foi usado). Só copiar de repositório com licença compatível (blendergranny = MIT; VanBurenTools = GPL-3.0, só se o projeto for GPL). Repos sem LICENSE (`OpenJE/patterns`, `openje`, `openvb`) são só leitura: reescreva, não copie.
6. **Pseudocódigo decompilado** (ex.: código do `openje-re`) serve para entender comportamento, nunca para colar.
7. **Não usar** SDK vazado da Granny nem qualquer código vazado.

## Atenção com `vb-pat.md` e `vb-table.md` (OpenJE/patterns)
O `vb-table.md` diz traduzir lógica C# (`ToByte()`/leitores) de origem não declarada, possivelmente de uma ferramenta de terceiros (talvez o VanBurenTools, GPL-3.0). Trate-os como só leitura e confirme a origem antes de qualquer reaproveitamento.

## Vendorização e amostras
- `third_party/blendergranny/` mantém a licença MIT e o `UPSTREAM.md`. Só o parser puro; nunca `research_native.py` nem as sondas contra a `granny2.dll`.
- Amostras enviadas para análise (via `sample`) ficam em `private/`, fora do git, e não entram em releases, specs nem artefatos publicados.

## Engines e bibliotecas trazidas pelo autor (v13 em diante)
Uma engine de teste, biblioteca de física ou qualquer código trazido de fora segue a mesma regra: licença registrada em `docs/LICENCAS_E_DIREITOS.md` (seção 5) antes de misturar, e partes de terceiros sem licença clara ficam de fora ou são reescritas.

Não é parecer jurídico.
