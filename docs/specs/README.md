# Specs de formatos (escritas por nós)

Modelo de cada arquivo `docs/specs/<formato>.md`:

- **Origem dos dados:** arquivo(s) da SUA demo usados na observação (nome + hash, nunca o conteúdo).
- **Layout:** tabela offset | tipo | nome | observação (endianness, tamanhos, alinhamento).
- **Invariantes verificadas:** o que sempre vale nos arquivos observados.
- **Incertezas:** campos desconhecidos.
- **Fontes:** o que foi consultado (ver `SOURCES.md`).

Ordem sugerida: `grp` -> `rht` -> `chunk` -> `stf` -> `b3d` -> `tre`/`map` -> entidades -> `gr2`.
