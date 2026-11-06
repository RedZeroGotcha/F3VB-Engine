# Spec: `.grp` (arquivo de grupo)

**Status: VERIFICADA em arquivos reais** (24 `.grp` únicos da demo do usuário, todos com `issues=0`). Origem do layout: `grp.pat` do OpenJE/patterns, reescrito; confirmado pela leitura dos arquivos.

## Layout (little-endian)

| Offset | Tipo | Campo | Observado na demo |
|---|---|---|---|
| 0 | u32 | vMajor | 2 |
| 4 | u32 | vMinor | 1 |
| 8 | u32 | nFiles | quantidade de lumps (0 nos `.grp` vazios) |
| 12 | `{u32 offset, u32 length}[nFiles]` | tabela de lumps | 8 bytes por lump |
| `offset` | bytes | dados do lump | `offset` absoluto desde o início do arquivo |

- Um `.grp` vazio tem 12 bytes (só o cabeçalho com `nFiles = 0`): `_CHM`, `_Dev`, `_ESF`, `_MSC`, `_RLZ`, `Templates`.
- O `.grp` **não guarda nomes nem tipos**: vêm do `resource.rht` (ver `rht.md`).
- Invariantes que valeram em todos os 24 arquivos: tabela termina antes dos dados; `offset + length` cabe no arquivo; lumps sem sobreposição.

## Ainda não sabemos
- Se existe padding/alinhamento entre lumps; se a tabela é sempre ordenada por `offset`.
