#include "f3tex.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace f3v {

bool load_f3tex(const std::uint8_t* data, std::size_t size, Image& out, std::string& err) {
    out = Image();
    if (size < 12 || std::memcmp(data, "F3TX", 4) != 0) { err = "nao e um .f3tex (magic F3TX ausente)"; return false; }
    std::uint32_t w, h;
    std::memcpy(&w, data + 4, 4);
    std::memcpy(&h, data + 8, 4);
    if (w == 0 || h == 0 || w > kMaxTexSize || h > kMaxTexSize) { err = "dimensoes absurdas: " + std::to_string(w) + "x" + std::to_string(h); return false; }
    const std::uint64_t bytes = static_cast<std::uint64_t>(w) * h * 4;
    if (bytes != size - 12) { err = "tamanho dos pixels nao bate com " + std::to_string(w) + "x" + std::to_string(h); return false; }
    out.w = w; out.h = h;
    out.rgba.assign(data + 12, data + 12 + static_cast<std::size_t>(bytes));
    return true;
}

bool load_f3tex_file(const std::string& path, Image& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "nao consegui abrir " + path; return false; }
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_f3tex(b.data(), b.size(), out, err);
}

std::vector<Image> build_mips(const Image& base) {
    std::vector<Image> out;
    if (base.w == 0 || base.h == 0 || base.rgba.size() != static_cast<std::size_t>(base.w) * base.h * 4) return out;
    out.push_back(base);
    while (out.back().w > 1 || out.back().h > 1) {
        const Image& s = out.back();
        Image d;
        d.w = std::max(1u, s.w / 2);
        d.h = std::max(1u, s.h / 2);
        d.rgba.resize(static_cast<std::size_t>(d.w) * d.h * 4);
        for (std::uint32_t y = 0; y < d.h; ++y) {
            const std::uint32_t y0 = std::min(s.h - 1, y * 2), y1 = std::min(s.h - 1, y * 2 + 1);
            for (std::uint32_t x = 0; x < d.w; ++x) {
                const std::uint32_t x0 = std::min(s.w - 1, x * 2), x1 = std::min(s.w - 1, x * 2 + 1);
                for (int c = 0; c < 4; ++c) {
                    const unsigned sum = s.rgba[(static_cast<std::size_t>(y0) * s.w + x0) * 4 + static_cast<std::size_t>(c)] + s.rgba[(static_cast<std::size_t>(y0) * s.w + x1) * 4 + static_cast<std::size_t>(c)] +
                                         s.rgba[(static_cast<std::size_t>(y1) * s.w + x0) * 4 + static_cast<std::size_t>(c)] + s.rgba[(static_cast<std::size_t>(y1) * s.w + x1) * 4 + static_cast<std::size_t>(c)];
                    d.rgba[(static_cast<std::size_t>(y) * d.w + x) * 4 + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>((sum + 2) / 4);
                }
            }
        }
        out.push_back(std::move(d));
    }
    return out;
}

}  // namespace f3v
