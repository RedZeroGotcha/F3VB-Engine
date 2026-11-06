# Exportador GLB (B3D + esqueleto/animações GR2 + texturas)

Comandos (a partir de `tools\py`):
```powershell
py -3 -m f3vb export-list G3D_Model --prefix CR_ "F:\...\Demo"          # nomes dos modelos
py -3 -m f3vb export-list SKEL_Skeleton --prefix CR_ "F:\...\Demo"
py -3 -m f3vb export-list GR2_Animation --prefix CR_Ant_ "F:\...\Demo"
py -3 -m f3vb export-glb CR_Ant "F:\...\Demo" --out ..\..\private\glb\CR_Ant.glb
```
Por padrão procura o esqueleto com o MESMO nome do modelo e as animações com prefixo `NOME_` (até 20; `--max-anims 0` = todas).
Humanos: o corpo é modular (peças `Bac*`, `Bea*`, `Bod*`, `Mus*`...) e o esqueleto é `HumMSkl`/`HumFSkl`:
`export-glb <peça> --skeleton HumMSkl --anim HumM_Com_1H_Gun`.
Abra o `.glb` no Blender (Arquivo > Importar > glTF 2.0). Saída fica em `private\glb\` (derivada do jogo: não publique).

## O que foi VALIDADO (medido)
- Vértice de 44 bytes: a caixa envolvente gravada no arquivo bate exatamente com as posições (3 arquivos reais).
- Terreno (64 bytes): posições formam uma grade 3x3 plana com normais (0,1,0) e UV de lightmap em {0,032; 0,5; 0,968}.
- Pesos de osso são guardados por slot (slot-major); o índice refere-se à lista de ossos do topo do `.b3d`.
- Texturas TGA decodificadas por código próprio ficam **idênticas pixel a pixel ao Pillow** (16 de 16 TGAs reais).
- Par real esqueleto + animação (CR_FloatingEyeBot): 8 ossos, 15 canais, quaternions unitários, GLB estruturalmente válido.
- Visual: item (dinamite), prop (vidro), terreno e bigode do personagem plotados a partir do GLB aparecem com formas plausíveis.

## O que NÃO foi validado (hipóteses a conferir no Blender)
- Escala: 1 unidade do jogo = ? metros (personagem tem ~5,5 de altura: provavelmente pés). Use `--scale 0.3048`.
- Eixo e espelhamento: mão-esquerda Y-up → mão-direita Y-up (Z invertido). Se o modelo aparecer espelhado, rode com `--no-convert` e me avise.
- Matriz inversa de bind: usamos a do GR2 (`InverseWorld4x4`), que difere de uma translação constante pequena da composição das poses locais.
- Rotação de nó/osso do B3D como eixo-ângulo em graus (observado nos ossos).
- Famílias de B3D não decodificadas ainda (Effects; ~4% de Critters; ~24% de Props) falham com mensagem clara.
- Lightmaps não têm textura (só o segundo UV `TEXCOORD_1` e o nome em `extras`).
