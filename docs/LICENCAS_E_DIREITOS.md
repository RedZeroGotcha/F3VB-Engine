# Licenças, direitos autorais e o que usamos de terceiros

> Registro vivo. **Toda vez que o projeto passar a usar algo de terceiros (código, biblioteca, ferramenta, fonte, imagem, som),
> anote aqui ANTES de commitar.** Isto não é parecer jurídico: é o controle para ninguém ser pego de surpresa.

## 1. Conteúdo do jogo (o que NUNCA entra no projeto)
| O quê | Dono | Regra no F3VB |
|---|---|---|
| Arquivos da demo técnica do Van Buren (`.grp`, `.rht`, `F3.exe`, modelos, texturas, animações, scripts, sons, vídeos) | Detentores dos direitos do jogo (a marca Fallout e o conteúdo são de propriedade da Bethesda Softworks; a Black Isle/Interplay criou a demo) | **Não vão para o repositório, para o `.zip` do projeto, nem para releases.** Cada pessoa extrai a PRÓPRIA cópia para `original_assets/` (ver `docs/ORIGINAL_ASSETS.md`) |
| Arquivos convertidos a partir deles (`.f3m`, `.glb`, `.png`) | Os mesmos (são obras derivadas) | Ficam em `original_assets/CONVERTED/`, também fora do projeto |
| Logs do visualizador (`logs/f3viewer_log_*.txt`) | Nosso, mas listam nomes de arquivos do jogo | Fora do git (`.gitignore`); pode enviar para análise, não publicar em massa |
| Logos, ícones, fontes, sons e imagens do jogo | Idem | Nunca usar na interface, no README nem no site do projeto. O visualizador pede a fonte "Consolas" ao Windows (instalada no sistema, não incluída no projeto) só para o HUD |
| Nome "Fallout" | Bethesda | Usamos "F3VB" e "Van Buren" só para **descrever** a que se refere; não sugerir produto oficial ou afiliação |

**Trava automática:** `py -3 scripts\verificar_sem_assets.py` varre o projeto e falha se achar extensão de dado do jogo, qualquer coisa
em `original_assets/` ou `private/`, ou arquivo grande. `py -3 scripts\empacotar.py v16` roda essa trava antes de gerar o `.zip`.
`original_assets/` também tem um `.gitignore` com `*` dentro dela. Os testes (`test_v12.py`) cobrem a trava.

**Cuidado ao compartilhar a sua cópia extraída** (por exemplo numa pasta do Google Drive): link aberto a "qualquer pessoa" equivale a distribuir
conteúdo protegido. Prefira acesso só para pessoas específicas e remova o compartilhamento quando terminar de usar. Isso vale para qualquer
nuvem, repositório ou chat que guarde esses arquivos.

Status legal da própria demo (material proprietário e vazado): o uso aqui é para estudo, interoperabilidade e preservação, **sem distribuir
nada do jogo**. Quem usa o projeto deve ter obtido a cópia de forma que as leis do seu país permitam. Não é parecer jurídico.

## 2. Código e ferramentas de terceiros que o projeto usa
| Item | Licença | Como entra | Onde está | Observação |
|---|---|---|---|---|
| **blendergranny** (parser de GR2 em Python puro), de Rasetsuu/"ciupix and contributors" | MIT | **Copiado (vendorizado)**: só `io_scene_gr2/gr2/` | `third_party/blendergranny/` (com `LICENSE` e `UPSTREAM.md`) | Manter o aviso de copyright e a licença. Nunca copiar `research_native.py`, as sondas contra `granny2.dll` nem o addon do Blender |
| **Logo do F3VB Engine** (v21) | Do autor do projeto | Arte enviada pelo autor para ser a logo do projeto e o ícone do `.exe` | `docs/img/f3vb_engine_logo.png`, `viewer/res/f3vb.ico` | Não é arquivo da demo; o ícone é a mesma imagem em vários tamanhos |
| **blendergranny, porte C++** (v20) | MIT | **Obra derivada**: `viewer/src/gr2.cpp` traduz para C++ o descompressor Oodle0, as correções de ponteiro, os tipos, o esqueleto e as curvas do código acima | `viewer/src/gr2.h`, `viewer/src/gr2.cpp` (o cabeçalho cita a origem) | O aviso MIT vai junto com o código-fonte (`third_party/blendergranny/LICENSE`); quem distribuir **o executável** deve incluir também esse aviso (ex.: na pasta, ao lado do `.exe`) |
| **binkw32.dll** (RAD Game Tools), a da **demo do jogador** | Proprietária | **Não vem com o projeto.** O `F3VBVideo32.exe` carrega a DLL **da pasta da demo** com `LoadLibrary`, em tempo de execução | só na demo de quem joga | Nunca copiar a DLL para o kit nem para o pacote. As tabelas do Bink não são públicas, e o decodificador do ffmpeg é LGPL (exigiria ligar dinamicamente e publicar mudanças); por isso usamos a da própria demo |
| **Pawn / Small 4.1** (CompuPhase) | Apache-2.0 com cláusula de exceção | **Só referência de semântica**; nenhum código copiado | `NOTICE-PAWN.md` | A VM de `src/amx/` é código próprio. Pawn é marca da CompuPhase |
| **OpenJE/patterns** (padrões do ImHex), `vb-pat.md`, `vb-table.md` | **Sem LICENSE** (todos os direitos reservados por padrão) | **Só leitura**: fatos (nomes de campos, tamanhos) reescritos com palavras próprias | `docs/SOURCES.md`, `docs/specs/` | **Não copiar texto nem código.** `vb-table.md` cita lógica de origem não declarada (possível GPL-3.0): confirmar a origem antes de qualquer reaproveitamento |
| Repos `openje`, `openvb`, `openje-re` | Sem LICENSE | Só leitura | `docs/DECISAO_ABORDAGEM.md` | Pseudocódigo decompilado serve para entender, nunca para colar |
| **VanBurenTools** | GPL-3.0 | **Não usado** | | Só poderia entrar se o F3VB inteiro fosse GPL |
| Windows SDK (Win32, `opengl32`, `user32`, `gdi32`, `comdlg32`, `shell32`), runtime do MSVC | Licença da Microsoft para o SDK/compilador | Usados para compilar; o runtime vai **estático (`/MT`)**, sem redistribuir DLLs | `viewer/CMakeLists.txt` | Não redistribuímos headers nem libs do SDK |
| Python 3, tkinter, CMake, Ninja, g++/MSVC, Ghidra, ImHex | Licenças próprias de cada ferramenta | Só como **ferramentas** de desenvolvimento; nenhum código delas no projeto | `tools/ghidra/`, `tools/imhex/` | O script do Ghidra e o README do ImHex são escritos para o projeto; as exportações do Ghidra ficam em `private/`. Se algum dia entrar um padrão do ImHex que não seja nosso, conferir a licença dele antes |
| Formatos públicos: TGA, BMP, glTF 2.0, PNG/zlib | Especificações abertas | Implementação própria | `tools/py/f3vb/` | glTF/PNG: só escrevemos o formato; sem biblioteca externa |

**Granny (RAD Game Tools):** nunca usamos `granny2.dll`, o SDK ou código vazado. O GR2 é lido pelo parser MIT acima (Python) e pelo porte C++ dele (v20), e os testes de
fumaça com dados reais foram feitos só nas cópias dos jogadores.

## 3. Código próprio
Todo o resto (`tools/py/f3vb/`, `viewer/`, `src/`, `tests_cpp/`, `scripts/`, `docs/`) é escrito para este projeto. **Licença do projeto: a definir pelo autor**
(até lá, tratar como "todos os direitos reservados"). Se for publicar, escolha a licença olhando a tabela acima: nada do que está vendorizado
obriga o projeto a ser GPL; o blendergranny (MIT) exige manter o aviso dele.

## 4. Checklist antes de adicionar QUALQUER coisa de terceiros
1. Qual é a licença? Está no repositório de origem (arquivo `LICENSE`)? **Sem licença = não copiar** (só ler e reescrever).
2. A licença permite o nosso uso (inclusive modificar e redistribuir)? GPL/AGPL contaminam o projeto inteiro: evitar.
3. Copiar com `LICENSE`/`NOTICE` junto, num `third_party/<nome>/` com `UPSTREAM.md` (origem, data, commit, o que foi copiado e o que foi omitido de propósito).
4. Anotar a linha na tabela da seção 2 **no mesmo commit**.
5. O pacote contém dados do jogo? Rodar `scripts\verificar_sem_assets.py`.

## 5. A "segunda engine" de teste do v13 (física + OpenGL, trazida pelo autor)
> **Pendência do autor:** adicionar um `LICENSE` a `engine_test/rpggl/` (e decidir a licença do F3VB). Sem isso, vale "todos os direitos reservados". Código gerado com apoio de IA: a proteção por direito autoral varia por país (não é parecer jurídico).
Antes de misturar qualquer código dela no F3VB, preencher esta tabela com: nome, autor, **licença**, de onde veio, o que será reaproveitado
(código? só ideias?), e se há bibliotecas dentro dela (física, matemática, janela, imagem, áudio) com licenças próprias.
Se for código do próprio autor, registrar isso aqui também. Se tiver partes de terceiros sem licença clara, **ficam de fora** ou são reescritas.

| Engine/biblioteca | Autor | Licença | Origem | O que usamos | Registrado em |
|---|---|---|---|---|---|
| **RPGGL v0.2** (mini-RPG em C++/OpenGL 1.x) | O autor do F3VB, com apoio de IA | **Nenhuma declarada** (o autor confirmou: "zero licença"); sem terceiros dentro (varredura automática em `test_engine_test.py`) | `RPGGL_v0_2_src.zip` enviado pelo autor (2026-10-13) | Código inteiro como referência em `engine_test/rpggl/` (inalterado). Plano de reaproveitamento em `docs/RPGGL_ANALISE.md` | `engine_test/README.md`, `docs/RPGGL_ANALISE.md` |

## v16: nada novo de terceiros
O v16 (lotes de índices do `.8tre`, `.f3map` v2, modos de cor, `tre-check`) é **código próprio** baseado só em observação dos arquivos do autor (`docs/SOURCES.md`). Nenhuma biblioteca, código, fonte ou imagem de terceiros entrou. O OpenJE/OpenVB continua como "só leitura de fatos" (e nesta etapa não ajudou: nada sobre o formato do 8TRE). O `.f3map` v2 contém nomes de materiais e texturas do jogo: é derivado do conteúdo do jogo e fica em `original_assets/CONVERTED/maps`, fora do projeto.

## v17: nada novo de terceiros
A VM e a camada de natives já existiam (referência de semântica: Pawn, Apache-2.0, `NOTICE-PAWN.md`). O leitor de `.stf` segue o layout do `stf.pat` do OpenJE/patterns (só o *fato* do formato; nenhum código copiado) e **não foi verificado em arquivo real**. Os `.amx` e o `.stf` do jogo ficam em `original_assets` (fora do projeto); os testes usam scripts sintéticos montados em código.

## v18: nada novo de terceiros
Texturas (`.f3tex`), UVs no `.f3map`, correção da VM e `ui-info` são **código próprio**, baseado em observação dos arquivos do autor. As texturas/imagens da UI convertidas ficam em `original_assets/CONVERTED` (fora do projeto e do repositório). Nenhuma biblioteca de imagem de terceiros foi adicionada (decodificadores TGA/BMP já eram próprios).
