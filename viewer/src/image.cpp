#include "image.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

namespace f3v {

namespace {

std::uint16_t rd16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t rd32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }

// Converte 1 pixel cru (bb bytes) do TGA para RGBA.
void tga_pixel(const std::uint8_t* s, int bb, int base, const std::vector<std::uint8_t>& pal, std::uint16_t cm_first, std::uint8_t* d) {
    if (base == 1) {                                              // paletizado (indice de 8 ou 16 bits)
        std::uint32_t idx = bb == 1 ? s[0] : rd16(s);
        idx = idx >= cm_first ? idx - cm_first : 0xFFFFFFFFu;
        if (idx < pal.size() / 4) { d[0] = pal[4 * idx]; d[1] = pal[4 * idx + 1]; d[2] = pal[4 * idx + 2]; d[3] = pal[4 * idx + 3]; }
        else { d[0] = d[1] = d[2] = 0; d[3] = 255; }
    } else if (base == 3) {                                       // cinza
        d[0] = d[1] = d[2] = s[0]; d[3] = bb >= 2 ? s[1] : 255;
    } else if (bb == 4) { d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3]; }
    else if (bb == 3) { d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = 255; }
    else {                                                        // 15/16 bits: 1-5-5-5
        const unsigned v = rd16(s);
        d[0] = static_cast<std::uint8_t>(((v >> 10) & 31) * 255 / 31);
        d[1] = static_cast<std::uint8_t>(((v >> 5) & 31) * 255 / 31);
        d[2] = static_cast<std::uint8_t>((v & 31) * 255 / 31);
        d[3] = 255;
    }
}

}  // namespace

bool decode_tga(const std::uint8_t* data, std::size_t size, Image& out, std::string& err) {
    if (!data || size < 18) { err = "TGA menor que o cabecalho (18 bytes)"; return false; }
    const std::uint8_t idlen = data[0], cmap_type = data[1], itype = data[2];
    const std::uint16_t cm_first = rd16(data + 3), cm_len = rd16(data + 5);
    const std::uint8_t cm_bits = data[7];
    const std::uint32_t w = rd16(data + 12), h = rd16(data + 14);
    const std::uint8_t bpp = data[16], desc = data[17];
    if (w == 0 || h == 0 || w > kMaxTexSize || h > kMaxTexSize) { err = "dimensoes invalidas " + std::to_string(w) + "x" + std::to_string(h); return false; }
    const int base = itype & 7;
    const bool rle = (itype & 8) != 0;
    if (base < 1 || base > 3) { err = "tipo de TGA nao suportado: " + std::to_string(itype); return false; }
    const int bb = (bpp + 7) / 8;
    if (bb < 1 || bb > 4 || (base == 2 && bb < 2) || (base == 1 && bb > 2)) { err = "bits por pixel nao suportados: " + std::to_string(bpp); return false; }
    std::size_t pos = 18u + idlen;
    std::vector<std::uint8_t> pal;
    if (cmap_type) {
        const int cb = (cm_bits + 7) / 8;
        if (cb < 2 || cb > 4) { err = "paleta TGA nao suportada (" + std::to_string(cm_bits) + " bits)"; return false; }
        if (pos + static_cast<std::size_t>(cm_len) * cb > size) { err = "paleta TGA truncada"; return false; }
        pal.resize(4u * cm_len);
        for (std::size_t i = 0; i < cm_len; ++i) tga_pixel(data + pos + i * cb, cb, 2, pal, 0, pal.data() + 4 * i);
        pos += static_cast<std::size_t>(cm_len) * cb;
    }
    if (base == 1 && pal.empty()) { err = "TGA paletizado sem paleta"; return false; }
    const std::size_t npx = static_cast<std::size_t>(w) * h;
    std::vector<std::uint8_t> rgba(npx * 4);
    if (!rle) {
        if (pos + npx * bb > size) { err = "dados de pixel truncados"; return false; }
        for (std::size_t i = 0; i < npx; ++i) tga_pixel(data + pos + i * bb, bb, base, pal, cm_first, rgba.data() + 4 * i);
    } else {
        std::size_t i = 0;
        while (i < npx) {
            if (pos >= size) { err = "RLE do TGA truncado no pixel " + std::to_string(i); return false; }
            const std::uint8_t hd = data[pos++];
            const std::size_t n = (hd & 0x7F) + 1u;
            if (i + n > npx) { err = "RLE do TGA passa do fim da imagem"; return false; }
            if (hd & 0x80) {
                if (pos + bb > size) { err = "RLE do TGA truncado"; return false; }
                std::uint8_t px[4];
                tga_pixel(data + pos, bb, base, pal, cm_first, px);
                pos += bb;
                for (std::size_t k = 0; k < n; ++k, ++i) for (int c = 0; c < 4; ++c) rgba[4 * i + c] = px[c];
            } else {
                if (pos + n * bb > size) { err = "RLE do TGA truncado"; return false; }
                for (std::size_t k = 0; k < n; ++k, ++i, pos += bb) tga_pixel(data + pos, bb, base, pal, cm_first, rgba.data() + 4 * i);
            }
        }
    }
    // Sem alfa declarado (bits 0-3 do descritor = 0) e 32 bits com alfa todo zero: trata como opaco (TGAs antigos sem canal alfa usado).
    if (bb == 4 && (desc & 0x0F) == 0) {
        bool all0 = true;
        for (std::size_t i = 0; i < npx && all0; ++i) all0 = rgba[4 * i + 3] == 0;
        if (all0) for (std::size_t i = 0; i < npx; ++i) rgba[4 * i + 3] = 255;
    }
    if (desc & 0x10) {                                            // origem a direita: espelha cada linha
        for (std::uint32_t y = 0; y < h; ++y)
            for (std::uint32_t x = 0; x < w / 2; ++x)
                for (int c = 0; c < 4; ++c) std::swap(rgba[(static_cast<std::size_t>(y) * w + x) * 4 + c], rgba[(static_cast<std::size_t>(y) * w + (w - 1 - x)) * 4 + c]);
    }
    out.w = w; out.h = h;
    if (desc & 0x20) out.rgba.swap(rgba);                         // origem em cima: ja esta de cima para baixo
    else {                                                        // origem embaixo (o comum na demo): inverte as linhas
        out.rgba.resize(rgba.size());
        const std::size_t row = static_cast<std::size_t>(w) * 4;
        for (std::uint32_t y = 0; y < h; ++y) std::copy(rgba.begin() + (h - 1 - y) * row, rgba.begin() + (h - y) * row, out.rgba.begin() + y * row);
    }
    return true;
}

bool decode_bmp(const std::uint8_t* data, std::size_t size, Image& out, std::string& err) {
    if (!data || size < 54 || data[0] != 'B' || data[1] != 'M') { err = "nao e BMP"; return false; }
    const std::uint32_t off = rd32(data + 10), dib = rd32(data + 14);
    const std::int32_t wi = static_cast<std::int32_t>(rd32(data + 18)), hi = static_cast<std::int32_t>(rd32(data + 22));
    const std::uint16_t bpp = rd16(data + 28);
    const std::uint32_t comp = rd32(data + 30);
    if (dib < 40 || (comp != 0 && comp != 3) || (bpp != 8 && bpp != 24 && bpp != 32) || wi <= 0 || hi == 0 || hi == INT32_MIN) {
        err = "BMP nao suportado (bpp=" + std::to_string(bpp) + ", compressao=" + std::to_string(comp) + ")"; return false;
    }
    const bool top_down = hi < 0;
    const std::uint32_t w = static_cast<std::uint32_t>(wi), h = static_cast<std::uint32_t>(top_down ? -hi : hi);
    if (w > kMaxTexSize || h > kMaxTexSize) { err = "BMP grande demais"; return false; }
    std::vector<std::uint8_t> pal;
    if (bpp == 8) {
        std::uint32_t n = size >= 50 ? rd32(data + 46) : 0;
        if (n == 0 || n > 256) n = 256;
        const std::size_t po = 14u + dib;
        if (po + 4u * n > size) { err = "paleta do BMP truncada"; return false; }
        pal.resize(4u * n);
        for (std::uint32_t i = 0; i < n; ++i) { pal[4 * i] = data[po + 4 * i + 2]; pal[4 * i + 1] = data[po + 4 * i + 1]; pal[4 * i + 2] = data[po + 4 * i]; pal[4 * i + 3] = 255; }
    }
    const std::size_t stride = ((static_cast<std::size_t>(w) * bpp + 31) / 32) * 4;
    if (off > size || stride * h > size - off) { err = "BMP truncado"; return false; }
    out.w = w; out.h = h;
    out.rgba.assign(static_cast<std::size_t>(w) * h * 4, 0);
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::uint32_t src_row = top_down ? y : h - 1 - y;
        const std::uint8_t* line = data + off + stride * src_row;
        std::uint8_t* d = out.rgba.data() + static_cast<std::size_t>(y) * w * 4;
        for (std::uint32_t x = 0; x < w; ++x, d += 4) {
            if (bpp == 24) { d[0] = line[3 * x + 2]; d[1] = line[3 * x + 1]; d[2] = line[3 * x]; d[3] = 255; }
            else if (bpp == 32) { d[0] = line[4 * x + 2]; d[1] = line[4 * x + 1]; d[2] = line[4 * x]; d[3] = line[4 * x + 3] ? line[4 * x + 3] : 255; }
            else {
                const std::uint32_t k = line[x];
                if (4u * k + 3 < pal.size()) { d[0] = pal[4 * k]; d[1] = pal[4 * k + 1]; d[2] = pal[4 * k + 2]; d[3] = 255; }
                else { d[3] = 255; }
            }
        }
    }
    return true;
}

bool decode_image(const std::uint8_t* data, std::size_t size, Image& out, std::string& err) {
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') return decode_bmp(data, size, out, err);
    if (size >= 4 && data[0] == 'F' && data[1] == '3' && data[2] == 'T' && data[3] == 'X') return load_f3tex(data, size, out, err);
    return decode_tga(data, size, out, err);
}

bool load_image_file(const std::string& path, Image& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "nao consegui abrir " + path; return false; }
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return decode_image(b.data(), b.size(), out, err);
}

}  // namespace f3v
