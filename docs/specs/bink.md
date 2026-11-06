# Vídeos .mve da demo (v20)

- Os `.mve` da demo **não são Interplay MVE**: são **Bink 1** (assinatura `BIKi`), 640×320, som 22050 Hz.
  - `IPlogo.mve`: 230 quadros a 125000/8341 fps (~15).
  - `BISlogo.mve`: 185 quadros a 15 fps.
- O F3VB não decodifica Bink. A especificação não tem tabelas públicas, e o decodificador do ffmpeg é LGPL. O ajudante **`F3VBVideo32.exe`**
  (`viewer/src/video_helper32.cpp`, 32 bits) carrega a **`binkw32.dll` da própria demo** e chama `BinkOpen`, `BinkDoFrame`,
  `BinkCopyToBuffer` (superfície tipo 3 = BGRA, conferido), `BinkNextFrame`, `BinkWait` e `BinkClose`.
- Saída pelo stdout (pipe): `"F3VBVID1"` + 5 × u32 (largura, altura, quadros, fps numerador, fps denominador); depois, por quadro, u32 índice +
  BGRA. O `F3VB.exe` (`video_player.cpp`) lê numa thread e desenha centrado mantendo a proporção.
- Som: `BinkSetSoundSystem(BinkOpenDirectSound)` antes do `BinkOpen`. Com o waveOut o som saía mudo no teste.
- Códigos de saída do ajudante: 3 sem DLL, 4 função ausente, 5 vídeo não abriu, 6 pipe fechado.
