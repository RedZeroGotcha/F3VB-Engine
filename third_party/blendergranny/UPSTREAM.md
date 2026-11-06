# blendergranny (vendorizado)

- Origem: https://github.com/Rasetsuu/blendergranny (zip da branch `main`, baixado em 01/10/2026; commit não registrado no zip: fixe um commit quando possível).
- Licença: MIT (ver `LICENSE`, "Copyright (c) 2026 ciupix and contributors").
- O que foi copiado: SOMENTE `io_scene_gr2/gr2/` (parser em Python puro, sem `bpy`).
- O que foi omitido de propósito: `io_scene_gr2/__init__.py` (addon do Blender), `tools/` (inclui sondas contra a `granny2.dll`) e `decompress/research_native.py` (carrega biblioteca nativa privada).
- Regra do projeto F3VB: nunca usar `granny2.dll`, SDK, `--research-native` nem `--unsafe-oodle0`.
- Testes upstream observados: 48 passam, 4 pulam (amostras privadas) e 2 falham (gravador `.gr2`, em desenvolvimento).
