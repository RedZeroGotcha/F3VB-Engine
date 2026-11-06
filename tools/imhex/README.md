# ImHex: como usar com este projeto

ImHex tem a própria linguagem de patterns (não roda Python). O papel dele aqui é
**conferir visualmente** o que o código Python/C++ lê.

1. Clone `OpenJE/patterns` FORA deste repositório (não tem LICENSE visível: só leitura).
2. Em ImHex, aponte a pasta de includes para `patterns/includes` e abra os `.pat`
   (`grp.pat`, `rht.pat`, `chunk.pat`, `b3d.pat`, `tre.pat`, `Map.pat`...).
3. Abra o seu `.grp` e confira o layout.
4. Anote o que observou em `docs/specs/<formato>.md` (com SUAS palavras, ver CLEANROOM.md).
5. O leitor Python/C++ implementa a spec e é testado contra o que o ImHex mostra
   (offsets, contagens, tamanhos).
