// Backend de CPU da interface (v19): pinta a lista de desenho do GuiScreen numa imagem RGBA, sem placa de video.
// Usado pelo F3VBUiPreview (gera PNG de qualquer tela .int) e pelos testes. Mesmas regras de mistura do backend OpenGL:
// normal = alfa; aditivo = soma; textura modulada pela cor; repeticao quando u/v > 1; amostragem bilinear.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "gui.h"
#include "ui_assets.h"

namespace f3v {

struct Canvas {
    int w = 0, h = 0;
    std::vector<std::uint8_t> rgba;                      // linha de cima primeiro
    void clear(std::uint8_t r, std::uint8_t g, std::uint8_t b) { rgba.assign(static_cast<std::size_t>(w) * h * 4, 255); for (std::size_t i = 0; i < rgba.size(); i += 4) { rgba[i] = r; rgba[i + 1] = g; rgba[i + 2] = b; } }
};

// Pinta `draws` (coordenadas da tela virtual 1024x768) no canvas, escalando para w x h.
void raster_draws(const std::vector<GuiDraw>& draws, UiAssets& assets, Canvas& c);
// Cola uma imagem esticada no canvas inteiro (fundo).
void raster_background(const Image& img, Canvas& c);

// PNG sem compressao (blocos "stored" do deflate): simples e sem dependencias. true se gravou.
bool write_png(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgba);
std::vector<std::uint8_t> encode_png(int w, int h, const std::vector<std::uint8_t>& rgba);

}  // namespace f3v
