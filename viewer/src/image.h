// Decodificadores de imagem proprios (v19): TGA (cru e RLE; 8 bits paletizado/cinza, 15/16, 24 e 32 bits) e BMP (8, 24 e 32 bits, sem compressao).
// Portavel (so C++ padrao), com checagem de limites: arquivo corrompido devolve false com a mensagem em `err`, nunca derruba o programa.
// Saida no mesmo formato do .f3tex (f3tex.h): RGBA8, linhas de CIMA para BAIXO. Assim a interface le direto do RETAIL, sem conversao.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "f3tex.h"

namespace f3v {

bool decode_tga(const std::uint8_t* data, std::size_t size, Image& out, std::string& err);
bool decode_bmp(const std::uint8_t* data, std::size_t size, Image& out, std::string& err);
// Escolhe pelo conteudo: "BM" = BMP, "F3TX" = .f3tex, senao TGA.
bool decode_image(const std::uint8_t* data, std::size_t size, Image& out, std::string& err);
bool load_image_file(const std::string& path, Image& out, std::string& err);

}  // namespace f3v
