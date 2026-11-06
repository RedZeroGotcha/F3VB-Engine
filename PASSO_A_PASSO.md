# F3VB: como ler e analisar cada tipo de arquivo

Regras de ouro: (1) tudo que sai da demo fica fora do projeto: análises em `private\`, arquivos extraídos/convertidos em `original_assets\` (RETAIL = originais, CONVERTED = convertidos; ver `docs\ORIGINAL_ASSETS.md`); (2) comece sempre pelo `03_resumo.txt` do
`rodar_tudo.ps1`; (3) quando algo der erro, copie a mensagem inteira.
Os comandos exatos estão em `COMANDOS.md`. Aqui está **o que olhar** em cada tipo.

## Ordem recomendada
1. `.grp` → 2. `resource.rht` → 3. `.gr2` → 4. `.b3d` → 5. `.tga`/`.bmp` → 6. mapas (`.8`/8TRE e `.map`) →
7. entidades (`.crt`, `.itm`...) → 8. textos (`.stf`) → 9. scripts (AMX) → 10. áudio (`.wav`).

---
## 1. `.grp` (arquivo de grupo)
- **Rodar:** `py -3 -m f3vb grp "$demo\NOME.grp"` e depois `--list`.
- **Olhar:** `lumps` (quantos), `issues` (deve ser 0), e a tabela "Conteúdo por heurística" (b3d, gr2, tga?, wav, amx?, chunk:XXXX, texto?, bin).
- **Sinal de problema:** "Não parece um .grp neste layout" ou `issues` > 0 → o layout dos patterns está errado ou o arquivo tem outro formato. Me mande a saída e os 64 primeiros bytes (`py -3 -m f3vb probe "$demo\NOME.grp"`).
- **Dica:** o `.grp` não guarda nomes. Nome e tipo vêm do `resource.rht`.

## 2. `resource.rht` (tabela de recursos)
- **Rodar:** `py -3 -m f3vb rht "$demo\resource.rht" --entries 60` e depois `py -3 -m f3vb catalog $demo`.
- **O que já sabemos (confirmado na demo):** `pack` aponta para o **nome do `.grp`** (Tiles, Critters...), `number` é o **índice do lump** nesse `.grp`, `one` é sempre 1. O enum de packs do `rht.pat` tem rótulos errados; ignore-os.
- **Olhar no `catalog`:** (1) `contagem` = True e `permutação` = True para cada pack; (2) a tabela "tipo x classe detectada": cada tipo deve concentrar-se numa classe (G3D_Model → b3d, GR2_Animation → gr2, AMX → amx...); (3) as amostras de nomes de recursos e a "consistência" do espaçamento dos offsets.
- **Sinal de problema:** tipos espalhados em várias classes → o mapeamento `number` → lump está errado para aquele pack. Me mande o `catalogo_resumo.txt`.

## 3. `.gr2` (Granny: esqueletos, animações, malhas)
- **Rodar:** `py -3 -m f3vb gr2 ARQUIVO.gr2` (ou `inspect`).
- **Olhar no JSON:**
  - `header.version` (esperado: 6 ou mais), `pointer_size` (32 bits?), `compressions` (`none`, `oodle0`, `oodle1`, `bitknit`...).
  - `errors`: lista de etapas que falharam (header, load_sections, geometry, skeletons, animations).
  - `geometry` / `skeletons` / `animations`: quantidade de malhas, ossos, clipes e duração.
- **Sinais de problema:** `errors` com `load_sections` → decodificador de compressão não aceitou (provável Oodle0); versão < 6 → parser precisa de extensão.
- **Proibido:** usar `granny2.dll`, SDK vazado, `--research-native`, `--unsafe-oodle0`.
- **Me mande:** o `.txt` do `gr2` de 2 ou 3 arquivos diferentes (um de personagem, um de animação).

## 4. `.b3d` (modelo)
- **Rodar:** `py -3 -m f3vb inspect ARQUIVO.b3d`.
- **Olhar:** `signature` (começa com `B3D `?), `globals`, `materials` (nomes, `blend_state`, `material_type`), `textures` (**nomes dos `.tga`**), `bone_count`, e `stopped` (onde a sonda parou).
- **Normal:** parar em `SceneNode` (`0x0a`). A sonda ainda não lê malhas.
- **Sinal de problema:** parar antes, em `token desconhecido` → ou o pattern está incompleto ou o arquivo é de outra versão. Me mande o `offset` e o hexdump (`hexdump_*.txt`).

## 5. `.tga` e `.bmp` (texturas)
- **Rodar:** `py -3 -m f3vb inspect ARQUIVO.tga`.
- **Olhar:** `width`, `height`, `bpp`, `type_name` (truecolor, RLE...), `size_ok` (tamanho coerente?), `plausible`.
- **Dica:** `tga` não tem assinatura; o resumo mostra "candidatos" x "plausíveis". Se `plausíveis` for bem menor, há falsos positivos.
- **Abrir de verdade:** dá para abrir `.tga` direto no Paint.NET, GIMP ou IrfanView.

## 6. Mapas: `.8` (8TRE) e `.map`
- **Ainda não há leitor.** O que dá para fazer hoje: `extract --classes 8tre` (ou `chunk`), e abrir no **ImHex** (guia abaixo).
- **Olhar no ImHex:** a sequência de chunks (`tag` de 4 letras, `version`, `size`). No `.map`, anote as tags que aparecem (EMAP, EME2, EMTR, EPTH...) e **os tamanhos** de cada uma.
- **Me mande:** `hexdump_*.txt` + o screenshot da árvore do ImHex.

## 7. Entidades (`.crt`, `.itm`, `.wea`, `.arm`, `.amo`, `.use`, `.dor`, `.con`)
- **Sem leitor ainda.** São famílias de chunks (`EEN2` → `GENT` → `GCRE`...). Use o ImHex com o pattern do tipo e confira se os valores fazem sentido (nomes, números).
- **Me mande:** 1 de cada tipo (o menor), via `sample --classes chunk`.

## 8. Textos (`.stf`)
- **Sem leitor ainda.** Formato: magic de 8 bytes, quantidade, e entradas com offset/tamanho da string. Abra no ImHex com `STF` e me diga se as strings aparecem legíveis.

## 9. Scripts (AMX / Pawn): confirmado na sua demo
- **Rodar:** o `03_resumo.txt` lista `AMX (Pawn?)`: versões, **publics** (funções do script) e **natives** (a API que o jogo oferece ao script). Para um arquivo: `py -3 -m f3vb inspect ARQUIVO.amx`.
- **Na sua demo:** 119 scripts com cabeçalho idêntico (`defsize=40`, magic `0xF1E0`), 300 públicas e 115 natives (A_Attack, StartDialogue, GetLOS...). Isso confirma Pawn/Small com uma API própria do jogo.

## 10. Áudio (`.wav`)
- Aparece como `wav` na classificação (cabeçalho RIFF/WAVE). Dá para extrair (`extract --classes wav`) e ouvir normalmente.

---
## ImHex (conferir visualmente)
1. Instale: `winget install WerWolv.ImHex`.
2. Baixe/descompacte o `patterns-main.zip` e copie a pasta `patterns-main\includes\je` para a pasta `includes` do ImHex (em Configurações → Pastas aparecem os caminhos; o nome do menu pode variar conforme a versão).
3. Abra o arquivo no ImHex, abra o editor de patterns e cole **uma** destas linhas (par `import` + variável):

| Arquivo | Pattern |
|---|---|
| `.grp` | `import je.file.grp;` `je::file::GRP grp @ 0x00;` |
| `resource.rht` | `import je.file.rht;` `je::file::RHT rht @ 0x00;` |
| `.b3d` | `import je.res.b3d;` `je::resource::B3d b3d @ 0x00;` |
| `.gr2` | `import je.res.gr2;` `je::resource::GR2 gr2 @ 0x00;` (só o cabeçalho) |
| `.stf` | `import je.res.stf;` `je::resource::STF stf @ 0x00;` |
| `.8` (8TRE) | `import je.res.tre;` `je::resource::TRE tre @ 0x00;` |
| `.map` | `import je.res.map;` `je::resource::MAP map @ 0x00;` |
| `.crt` | `import je.res.crt;` `je::resource::CRT crt @ 0x00;` |

Se o ImHex reclamar do nome com `je::file::`, tente só `GRP grp @ 0x00;` (o namespace é "auto"). Lembre: vários patterns de componente só modelam o payload e precisam do wrapper de chunk (`tag + version + size`); por isso comece sempre pelo arquivo inteiro.

## Ghidra (opcional, mais tarde)
Abra o `F3.exe`, rode `tools\ghidra\export_functions.py` e salve o JSON em `private\`. **Nunca commite** a saída.
