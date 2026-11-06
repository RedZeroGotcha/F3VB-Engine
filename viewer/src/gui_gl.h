// Backend OpenGL da interface (v19): desenha a lista de desenho do GuiScreen (gui.h) e textos/imagens avulsos.
// A interface e desenhada numa "tela virtual" de 1024x768 (a do .int), escalada para caber na janela mantendo 4:3 (faixas nas laterais em 16:9).
// Win32 + OpenGL 1.x. As imagens e fontes vem do UiAssets (portavel); aqui so viram texturas.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "gui.h"
#include "ui_assets.h"

namespace f3v {

class GuiGL {
public:
    explicit GuiGL(UiAssets& a) : assets_(a) {}
    void begin(int win_w, int win_h);                 // ortho da tela virtual; chame antes de desenhar a UI (depois do 3D)
    void end();
    void draw(const std::vector<GuiDraw>& list);
    void image(const std::string& tex, float x0, float y0, float x1, float y1, const std::uint8_t rgba[4] = nullptr, bool additive = false);
    void rect(float x0, float y0, float x1, float y1, float r, float g, float b, float a);
    // Texto numa fonte do jogo. align 0/1/2 em relacao a x (esquerda/centro/direita). y = topo da linha.
    void text(const std::string& font, float px, float x, float y, const std::string& cp1252, const std::uint8_t rgba[4], int align = 0);
    float text_width(const std::string& font, float px, const std::string& cp1252);
    // Mouse da janela -> tela virtual
    void to_virtual(int mx, int my, float& vx, float& vy) const;
    float scale() const { return s_; }
    void release();                                   // apaga as texturas (troca de contexto / fim)
    std::size_t texture_count() const { return tex_.size(); }

private:
    unsigned texture(const std::string& name, int& w, int& h);
    struct FontTex { unsigned id = 0; const FontAtlas* atlas = nullptr; };
    const FontTex* font(const std::string& name, float px);
    UiAssets& assets_;
    struct Tex { unsigned id = 0; int w = 0, h = 0; };
    std::map<std::string, Tex> tex_;
    std::map<const FontAtlas*, FontTex> fonts_;
    float s_ = 1, ox_ = 0, oy_ = 0;
    int win_w_ = 1, win_h_ = 1;
};

}  // namespace f3v
