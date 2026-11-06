// Texturas convertidas (.f3tex) para o F3VBMapViewer (v18). Portavel: sem OpenGL (o upload fica no viewer).
// Formato: "F3TX" + u32 largura + u32 altura + RGBA8 (linhas de CIMA para BAIXO, como no TGA da demo; com v = 0 no topo o RGBA sobe direto para o GL).
// Gerado por:  py -3 -m f3vb export-textures   (ou convert-retail --maps)
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace f3v {

struct Image {
    std::uint32_t w = 0, h = 0;
    std::vector<std::uint8_t> rgba;
};

constexpr std::uint32_t kMaxTexSize = 8192;      // limite de sanidade (a demo usa 64..1024)

bool load_f3tex(const std::uint8_t* data, std::size_t size, Image& out, std::string& err);
bool load_f3tex_file(const std::string& path, Image& out, std::string& err);

// Cadeia de mipmaps por filtro de caixa 2x2 (nivel 0 = a propria imagem, ate 1x1). Alfa tambem e filtrado.
std::vector<Image> build_mips(const Image& base);

}  // namespace f3v
