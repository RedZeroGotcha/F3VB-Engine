#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include "gui_gl.h"

#include <algorithm>
#include <cmath>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace f3v {

void GuiGL::begin(int w, int h) {
    win_w_ = std::max(1, w); win_h_ = std::max(1, h);
    s_ = std::min(static_cast<float>(win_w_) / 1024.f, static_cast<float>(win_h_) / 768.f);
    ox_ = (static_cast<float>(win_w_) - 1024.f * s_) * 0.5f;
    oy_ = (static_cast<float>(win_h_) - 768.f * s_) * 0.5f;
    glViewport(0, 0, win_w_, win_h_);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, win_w_, win_h_, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glTranslatef(ox_, oy_, 0);
    glScalef(s_, s_, 1);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void GuiGL::end() {
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

void GuiGL::to_virtual(int mx, int my, float& vx, float& vy) const {
    vx = (static_cast<float>(mx) - ox_) / s_;
    vy = (static_cast<float>(my) - oy_) / s_;
}

unsigned GuiGL::texture(const std::string& name, int& w, int& h) {
    const std::string key = lower_ascii(name);
    auto it = tex_.find(key);
    if (it == tex_.end()) {
        Tex t;
        if (const Image* im = assets_.image(key)) {
            GLuint id = 0;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(im->w), static_cast<GLsizei>(im->h), 0, GL_RGBA, GL_UNSIGNED_BYTE, im->rgba.data());
            t.id = id; t.w = static_cast<int>(im->w); t.h = static_cast<int>(im->h);
        }
        it = tex_.emplace(key, t).first;
    }
    w = it->second.w; h = it->second.h;
    return it->second.id;
}

const GuiGL::FontTex* GuiGL::font(const std::string& name, float px) {
    const FontAtlas* a = assets_.atlas(name, px);
    if (!a) return nullptr;
    auto it = fonts_.find(a);
    if (it == fonts_.end()) {
        FontTex f;
        f.atlas = a;
        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, a->w, a->h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, a->alpha.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        f.id = id;
        it = fonts_.emplace(a, f).first;
    }
    return &it->second;
}

void GuiGL::image(const std::string& tex, float x0, float y0, float x1, float y1, const std::uint8_t rgba[4], bool additive) {
    int w, h;
    const unsigned id = texture(tex, w, h);
    if (!id) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, id);
    if (additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    if (rgba) glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]); else glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x0, y0);
    glTexCoord2f(1, 0); glVertex2f(x1, y0);
    glTexCoord2f(1, 1); glVertex2f(x1, y1);
    glTexCoord2f(0, 1); glVertex2f(x0, y1);
    glEnd();
    if (additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GuiGL::rect(float x0, float y0, float x1, float y1, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

float GuiGL::text_width(const std::string& f, float px, const std::string& s) {
    const FontAtlas* a = assets_.atlas(f, px * s_);
    return a ? a->measure(s) / s_ : static_cast<float>(s.size()) * px * 0.5f;
}

void GuiGL::text(const std::string& fname, float px, float x, float y, const std::string& s, const std::uint8_t rgba[4], int align) {
    // o atlas e assado no tamanho REAL em pixels de tela (px * escala) para o texto sair nitido; as medidas voltam para a tela virtual
    const FontTex* f = font(fname, px * s_);
    if (!f) return;
    const FontAtlas& a = *f->atlas;
    const float inv = 1.f / s_;
    float pen = x;
    const float tw = a.measure(s) * inv;
    if (align == 1) pen = x - tw * 0.5f; else if (align == 2) pen = x - tw;
    // alinha a linha de base a um pixel de tela
    const float base_screen = std::floor((y * s_ + oy_) + a.ascent + 0.5f);
    float pen_screen = std::floor(pen * s_ + ox_ + 0.5f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, f->id);
    glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]);
    glPushMatrix();
    glLoadIdentity();                               // desenha em pixels de tela (sem a escala), para o atlas casar 1:1
    glBegin(GL_QUADS);
    const float iw = 1.f / static_cast<float>(a.w), ih = 1.f / static_cast<float>(a.h);
    for (std::size_t i = 0; i < s.size(); ++i) {
        const std::uint8_t ch = static_cast<std::uint8_t>(s[i]);
        const GlyphInfo& g = a.glyph[ch];
        if (g.w > 0) {
            const float gx = pen_screen + static_cast<float>(g.ox), gy = base_screen + static_cast<float>(g.oy);
            const float u0 = g.x * iw, v0 = g.y * ih, u1 = (g.x + g.w) * iw, v1 = (g.y + g.h) * ih;
            glTexCoord2f(u0, v0); glVertex2f(gx, gy);
            glTexCoord2f(u1, v0); glVertex2f(gx + g.w, gy);
            glTexCoord2f(u1, v1); glVertex2f(gx + g.w, gy + g.h);
            glTexCoord2f(u0, v1); glVertex2f(gx, gy + g.h);
        }
        pen_screen += g.advance;
        if (i + 1 < s.size()) {
            const auto it = a.kern.find((static_cast<std::uint32_t>(ch) << 8) | static_cast<std::uint8_t>(s[i + 1]));
            if (it != a.kern.end()) pen_screen += it->second;
        }
    }
    glEnd();
    glPopMatrix();
}

void GuiGL::draw(const std::vector<GuiDraw>& list) {
    for (const GuiDraw& d : list) {
        if (d.kind == GuiDraw::Quad) {
            int w, h;
            const unsigned id = texture(d.texture, w, h);
            if (!id) continue;
            glBindTexture(GL_TEXTURE_2D, id);
            if (d.additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4ub(d.rgba[0], d.rgba[1], d.rgba[2], d.rgba[3]);
            glBegin(GL_QUADS);
            glTexCoord2f(d.u0, d.v0); glVertex2f(d.x0, d.y0);
            glTexCoord2f(d.u1, d.v0); glVertex2f(d.x1, d.y0);
            glTexCoord2f(d.u1, d.v1); glVertex2f(d.x1, d.y1);
            glTexCoord2f(d.u0, d.v1); glVertex2f(d.x0, d.y1);
            glEnd();
            if (d.additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            float x = d.x0 + 1;
            if (d.align == 1) x = (d.x0 + d.x1) * 0.5f; else if (d.align == 2) x = d.x1 - 1;
            const bool clip = d.cx1 - d.cx0 < 1e8f;
            if (clip) {                                   // v20: corta no retangulo do rotulo (como o original)
                const int sx0 = static_cast<int>(std::floor(d.cx0 * s_ + ox_)), sx1 = static_cast<int>(std::ceil(d.cx1 * s_ + ox_));
                const int sy0 = static_cast<int>(std::floor(d.cy0 * s_ + oy_)), sy1 = static_cast<int>(std::ceil(d.cy1 * s_ + oy_));
                glEnable(GL_SCISSOR_TEST);
                glScissor(sx0, win_h_ - sy1, std::max(0, sx1 - sx0), std::max(0, sy1 - sy0));
            }
            text(d.font, d.size_px, x, d.y0, d.text, d.rgba, d.align);
            if (clip) glDisable(GL_SCISSOR_TEST);
        }
    }
}

void GuiGL::release() {
    for (auto& kv : tex_) if (kv.second.id) { GLuint id = kv.second.id; glDeleteTextures(1, &id); }
    for (auto& kv : fonts_) if (kv.second.id) { GLuint id = kv.second.id; glDeleteTextures(1, &id); }
    tex_.clear(); fonts_.clear();
}

}  // namespace f3v
