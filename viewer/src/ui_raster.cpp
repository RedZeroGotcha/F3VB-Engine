#include "ui_raster.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace f3v {

namespace {

void sample(const Image& im, float u, float v, float out[4]) {
    // bilinear com repeticao
    const float x = u * static_cast<float>(im.w) - 0.5f, y = v * static_cast<float>(im.h) - 0.5f;
    const int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(xi), fy = y - static_cast<float>(yi);
    const int W = static_cast<int>(im.w), H = static_cast<int>(im.h);
    auto px = [&](int xx, int yy, int c) {
        xx = ((xx % W) + W) % W; yy = ((yy % H) + H) % H;
        return static_cast<float>(im.rgba[(static_cast<std::size_t>(yy) * W + xx) * 4 + c]);
    };
    for (int c = 0; c < 4; ++c)
        out[c] = (px(xi, yi, c) * (1 - fx) + px(xi + 1, yi, c) * fx) * (1 - fy) + (px(xi, yi + 1, c) * (1 - fx) + px(xi + 1, yi + 1, c) * fx) * fy;
}

void blend(Canvas& c, int x, int y, const float src[4], bool additive) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h) return;
    std::uint8_t* d = &c.rgba[(static_cast<std::size_t>(y) * c.w + x) * 4];
    const float a = src[3] / 255.f;
    for (int k = 0; k < 3; ++k) {
        const float v = additive ? d[k] + src[k] * a : d[k] * (1 - a) + src[k] * a;
        d[k] = static_cast<std::uint8_t>(std::clamp(v, 0.f, 255.f));
    }
}

std::uint32_t crc_table[256];
bool crc_ready = false;
std::uint32_t crc32(const std::uint8_t* p, std::size_t n, std::uint32_t c = 0xFFFFFFFFu) {
    if (!crc_ready) {
        for (std::uint32_t i = 0; i < 256; ++i) { std::uint32_t v = i; for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1; crc_table[i] = v; }
        crc_ready = true;
    }
    for (std::size_t i = 0; i < n; ++i) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}

void put32(std::vector<std::uint8_t>& o, std::uint32_t v) { o.push_back(static_cast<std::uint8_t>(v >> 24)); o.push_back(static_cast<std::uint8_t>(v >> 16)); o.push_back(static_cast<std::uint8_t>(v >> 8)); o.push_back(static_cast<std::uint8_t>(v)); }

void chunk(std::vector<std::uint8_t>& o, const char* tag, const std::vector<std::uint8_t>& data) {
    put32(o, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = o.size();
    o.insert(o.end(), tag, tag + 4);
    o.insert(o.end(), data.begin(), data.end());
    put32(o, crc32(&o[start], o.size() - start) ^ 0xFFFFFFFFu);
}

}  // namespace

void raster_background(const Image& img, Canvas& c) {
    if (img.w == 0 || img.h == 0) return;
    for (int y = 0; y < c.h; ++y)
        for (int x = 0; x < c.w; ++x) {
            float s[4];
            sample(img, (static_cast<float>(x) + 0.5f) / static_cast<float>(c.w), (static_cast<float>(y) + 0.5f) / static_cast<float>(c.h), s);
            s[3] = 255;
            blend(c, x, y, s, false);
        }
}

void raster_draws(const std::vector<GuiDraw>& draws, UiAssets& assets, Canvas& c) {
    const float sx = static_cast<float>(c.w) / 1024.f, sy = static_cast<float>(c.h) / 768.f;
    for (const GuiDraw& d : draws) {
        if (d.kind == GuiDraw::Quad) {
            const Image* im = assets.image(d.texture);
            if (!im || im->w == 0) continue;
            const int X0 = static_cast<int>(std::floor(d.x0 * sx)), X1 = static_cast<int>(std::ceil(d.x1 * sx));
            const int Y0 = static_cast<int>(std::floor(d.y0 * sy)), Y1 = static_cast<int>(std::ceil(d.y1 * sy));
            const float dw = (d.x1 - d.x0) * sx, dh = (d.y1 - d.y0) * sy;
            if (dw <= 0 || dh <= 0) continue;
            for (int y = std::max(0, Y0); y < std::min(c.h, Y1); ++y)
                for (int x = std::max(0, X0); x < std::min(c.w, X1); ++x) {
                    const float tx = ((static_cast<float>(x) + 0.5f) - d.x0 * sx) / dw, ty = ((static_cast<float>(y) + 0.5f) - d.y0 * sy) / dh;
                    if (tx < 0 || tx > 1 || ty < 0 || ty > 1) continue;
                    float s[4];
                    sample(*im, d.u0 + tx * (d.u1 - d.u0), d.v0 + ty * (d.v1 - d.v0), s);
                    for (int k = 0; k < 3; ++k) s[k] = s[k] * d.rgba[k] / 255.f;
                    s[3] = s[3] * d.rgba[3] / 255.f;
                    blend(c, x, y, s, d.additive);
                }
        } else {
            const FontAtlas* a = assets.atlas(d.font, d.size_px * sy);
            if (!a) continue;
            const float tw = a->measure(d.text);
            float pen = d.x0 * sx + 1;
            if (d.align == 1) pen = (d.x0 * sx + d.x1 * sx - tw) * 0.5f;
            else if (d.align == 2) pen = d.x1 * sx - tw - 1;
            const float base = std::floor(d.y0 * sy + a->ascent + 0.5f);
            pen = std::floor(pen + 0.5f);
            for (std::size_t i = 0; i < d.text.size(); ++i) {
                const std::uint8_t ch = static_cast<std::uint8_t>(d.text[i]);
                const GlyphInfo& g = a->glyph[ch];
                for (int r = 0; r < g.h; ++r)
                    for (int q = 0; q < g.w; ++q) {
                        const float al = a->alpha[static_cast<std::size_t>(g.y + r) * a->w + g.x + q];
                        if (al <= 0) continue;
                        const int px = static_cast<int>(pen) + g.ox + q, py = static_cast<int>(base) + g.oy + r;
                        if (px < d.cx0 * sx || px >= d.cx1 * sx || py < d.cy0 * sy || py >= d.cy1 * sy) continue;   // v20: recorte no rotulo
                        const float s[4] = {static_cast<float>(d.rgba[0]), static_cast<float>(d.rgba[1]), static_cast<float>(d.rgba[2]), al * d.rgba[3] / 255.f};
                        blend(c, px, py, s, false);
                    }
                pen += g.advance;
                if (i + 1 < d.text.size()) {
                    const auto it = a->kern.find((static_cast<std::uint32_t>(ch) << 8) | static_cast<std::uint8_t>(d.text[i + 1]));
                    if (it != a->kern.end()) pen += it->second;
                }
            }
        }
    }
}

std::vector<std::uint8_t> encode_png(int w, int h, const std::vector<std::uint8_t>& rgba) {
    std::vector<std::uint8_t> o = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<std::uint8_t> ihdr;
    put32(ihdr, static_cast<std::uint32_t>(w)); put32(ihdr, static_cast<std::uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(o, "IHDR", ihdr);
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(h) * (1 + 4 * static_cast<std::size_t>(w)));
    for (int y = 0; y < h; ++y) { raw.push_back(0); raw.insert(raw.end(), rgba.begin() + static_cast<std::ptrdiff_t>(y) * w * 4, rgba.begin() + static_cast<std::ptrdiff_t>(y + 1) * w * 4); }
    std::vector<std::uint8_t> z = {0x78, 0x01};
    std::uint32_t s1 = 1, s2 = 0;
    for (std::uint8_t b : raw) { s1 = (s1 + b) % 65521; s2 = (s2 + s1) % 65521; }
    for (std::size_t p = 0; p < raw.size() || p == 0;) {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - p);
        const bool last = p + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n)); z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n)); z.push_back(static_cast<std::uint8_t>((~n) >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(p), raw.begin() + static_cast<std::ptrdiff_t>(p + n));
        p += n;
        if (last) break;
    }
    put32(z, (s2 << 16) | s1);
    chunk(o, "IDAT", z);
    chunk(o, "IEND", {});
    return o;
}

bool write_png(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgba) {
    const std::vector<std::uint8_t> b = encode_png(w, h, rgba);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size();
    std::fclose(f);
    return ok;
}

}  // namespace f3v
