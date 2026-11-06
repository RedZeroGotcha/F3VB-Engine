"""Conhecimento de partida, extraído dos patterns do OpenJE (README, rht.pat, grp.pat).

Só nomes, ids e layouts (fatos), reescritos aqui. Os patterns não têm LICENSE visível:
não copiamos código deles. Tudo é HIPÓTESE até ser confirmado nos SEUS arquivos
(ver CLEANROOM.md e docs/specs/).
"""

# ResourceType do resource.rht: id -> nome (como nomeado nos patterns).
RESOURCE_TYPES = {
    100: "G3D_Model", 200: "TGA_Image", 300: "SMA_Script", 400: "TRE_Octree",
    500: "SKN_Skin", 600: "SKEL_Skeleton", 700: "GR2_Animation", 800: "INI_Config",
    900: "RLZ", 1000: "RTD", 1100: "WAV_Audio", 1200: "AMX", 1300: "MAP_Map",
    1400: "CRT_ENT_Critter", 1500: "VEG_Vfx", 1600: "INT_Interface", 1700: "RLE",
    1800: "ITM", 1900: "PSF", 2000: "GSF", 2100: "GLS", 2200: "SCO", 2300: "PCE",
    2400: "WEA_ENT_Weapon", 2500: "ARM_ENT_Armor", 2600: "DOR_ENT_Door",
    2700: "USE_ENT_Use", 2800: "SST", 2900: "STR", 3000: "ENC", 3100: "WMP",
    3200: "PRO", 3300: "SPL", 3400: "FNT", 3500: "AMO_ENT_Ammo", 3600: "CON_ENT",
    3700: "TOK_Tokens",
}

# Extensão provável por tipo. Os 13 primeiros nomes vêm do README dos patterns;
# os demais são palpite pelo nome do enum (INFERIDO).
RESOURCE_EXT = {
    100: "b3d", 200: "tga", 300: "sma", 400: "8", 500: "skn", 600: "skl",
    700: "gr2", 800: "ini", 900: "rlz", 1000: "rtd", 1100: "wav", 1200: "amx",
    1300: "map", 1400: "crt", 1500: "veg", 1600: "int", 1700: "rle", 1800: "itm",
    1900: "psf", 2000: "gsf", 2100: "gls", 2200: "sco", 2300: "pce", 2400: "wea",
    2500: "arm", 2600: "dor", 2700: "use", 2800: "sst", 2900: "str", 3000: "enc",
    3100: "wmp", 3200: "pro", 3300: "spl", 3400: "fnt", 3500: "amo", 3600: "con",
    3700: "tok",
}

# PackOffset do rht.pat: valor -> nome. DEPRECADO: na demo real os VALORES são deslocamentos na
# região de packs do .rht (onde ficam os nomes dos .grp: Tiles, Interface, Critters...), e os
# RÓTULOS deste dicionário (PSF, INT, RLE...) estão ERRADOS. Use RhtInfo.pack_label().
PACK_OFFSETS = {
    0: "PSF", 6: "INT", 16: "RLE", 25: "ITM", 32: "VEG", 42: "CRT", 50: "MAP",
    55: "WAV", 63: "AMX", 68: "RTD", 74: "RLZ", 81: "INI", 87: "SKL", 92: "GR2",
    97: "SKN", 102: "EIGHT", 107: "DLG", 112: "STR", 117: "SST", 122: "USE",
    127: "ARM", 132: "DOR", 137: "WEA", 142: "PCE",
}
PACK_INVALID = 0xFFFFFFFF

# Enquadramento de chunk: char[4] tag, u32 version, u32 size (inclui os 12 bytes).
CHUNK_HEADER_SIZE = 12

# Tags de chunk deduzidas dos NOMES dos patterns (não verificadas nos bytes).
CHUNK_TAGS = {
    "mapa": ["EMAP", "EME2", "EMEF", "EMEP", "EMFG", "EMNO", "EMNP", "EMSD",
             "EMTR", "EPTH", "ECAM", "2MWT"],
    "entidade": ["EEN2", "EEOV", "GENT", "GCRE", "GCHR", "GITM", "GIAR",
                 "GIAM", "GIWP", "GWAM", "GOBJ", "GDOR", "GCON"],
    # Genéricas: muito falso positivo; o filtro de plausibilidade ajuda.
    "tre": ["HEAD", "LGTD", "MATD", "VTXD", "TREE"],
}

MAGICS = {b"B3D ": "b3d (modelo)", b"8TRE": "8tre (geometria de mapa)"}

# Magic do Pawn/Small (hipótese para o tipo AMX): u16 no offset 4 do arquivo.
AMX_MAGICS = {0xF1E0: "magic F1E0", 0xF1E1: "magic F1E1", 0xF1E2: "magic F1E2"}
