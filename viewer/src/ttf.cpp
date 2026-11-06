#include "ttf.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

namespace f3v {

// ------------------------------------------------------------------------------------------------ leitura das tabelas
std::uint16_t TrueTypeFont::u16(std::size_t o) const { return o + 2 <= d_.size() ? static_cast<std::uint16_t>((d_[o] << 8) | d_[o + 1]) : 0; }
std::uint32_t TrueTypeFont::u32(std::size_t o) const {
    return o + 4 <= d_.size() ? (static_cast<std::uint32_t>(d_[o]) << 24) | (d_[o + 1] << 16) | (d_[o + 2] << 8) | d_[o + 3] : 0;
}

bool TrueTypeFont::load_file(const std::string& path, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "nao consegui abrir " + path; return false; }
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load(b.data(), b.size(), err);
}

bool TrueTypeFont::load(const std::uint8_t* data, std::size_t size, std::string& err) {
    ok_ = false;
    d_.assign(data, data + size);
    if (size < 12) { err = "arquivo pequeno demais para TrueType"; return false; }
    const std::uint32_t ver = u32(0);
    if (ver != 0x00010000u && ver != 0x74727565u /* 'true' */) { err = "nao e TrueType (versao " + std::to_string(ver) + "; OpenType/CFF nao suportado)"; return false; }
    const int ntab = u16(4);
    std::size_t head = 0, hhea = 0, maxp = 0, cmap = 0, name = 0, name_len = 0;
    std::size_t glyf_len = 0, loca_len = 0, hmtx_len = 0, cmap_len = 0;
    for (int i = 0; i < ntab; ++i) {
        const std::size_t e = 12 + 16 * static_cast<std::size_t>(i);
        if (e + 16 > size) { err = "diretorio de tabelas truncado"; return false; }
        const std::uint32_t off = u32(e + 8), len = u32(e + 12);
        if (off > size || len > size - off) { err = "tabela fora do arquivo"; return false; }
        const char* t = reinterpret_cast<const char*>(&d_[e]);
        if (!std::memcmp(t, "head", 4)) head = off;
        else if (!std::memcmp(t, "hhea", 4)) hhea = off;
        else if (!std::memcmp(t, "maxp", 4)) maxp = off;
        else if (!std::memcmp(t, "cmap", 4)) { cmap = off; cmap_len = len; }
        else if (!std::memcmp(t, "glyf", 4)) { glyf_ = off; glyf_len = len; }
        else if (!std::memcmp(t, "loca", 4)) { loca_ = off; loca_len = len; }
        else if (!std::memcmp(t, "hmtx", 4)) { hmtx_ = off; hmtx_len = len; }
        else if (!std::memcmp(t, "kern", 4)) { kern_ = off; kern_len_ = len; }
        else if (!std::memcmp(t, "name", 4)) { name = off; name_len = len; }
    }
    if (!head || !hhea || !maxp || !cmap || !glyf_ || !loca_ || !hmtx_) { err = "faltam tabelas obrigatorias (head/hhea/maxp/cmap/glyf/loca/hmtx)"; return false; }
    glyf_len_ = glyf_len;
    upem_ = u16(head + 18);
    loca_long_ = s16(head + 50);
    ascent_ = s16(hhea + 4); descent_ = s16(hhea + 6); line_gap_ = s16(hhea + 8);
    nhmetrics_ = u16(hhea + 34);
    nglyphs_ = u16(maxp + 4);
    if (upem_ < 16 || upem_ > 16384 || nglyphs_ == 0 || nhmetrics_ == 0) { err = "cabecalho da fonte invalido"; return false; }
    if (loca_len < static_cast<std::size_t>(nglyphs_ + 1) * (loca_long_ ? 4u : 2u)) { err = "loca menor que o numero de glifos"; return false; }
    if (hmtx_len < static_cast<std::size_t>(nhmetrics_) * 4u) { err = "hmtx menor que numberOfHMetrics"; return false; }
    // cmap: prefere Unicode (3,1) ou (0,x) formato 4; depois simbolo (3,0); depois Mac (1,0) formato 6/0
    const int nsub = u16(cmap + 2);
    int best = -1;
    for (int i = 0; i < nsub; ++i) {
        const std::size_t r = cmap + 4 + 8 * static_cast<std::size_t>(i);
        if (r + 8 > cmap + cmap_len) break;
        const int pid = u16(r), eid = u16(r + 2);
        const std::uint32_t off = u32(r + 4);
        if (off >= cmap_len) continue;
        const int fmt = u16(cmap + off);
        int score = -1;
        if (fmt == 4 && ((pid == 3 && eid == 1) || pid == 0)) score = 4;
        else if (fmt == 4 && pid == 3 && eid == 0) score = 3;
        else if ((fmt == 6 || fmt == 0) && pid == 1) score = 2;
        else if (fmt == 4 || fmt == 6 || fmt == 0) score = 1;
        if (score > best) { best = score; cmap_sub_ = cmap + off; cmap_fmt_ = fmt; symbol_ = pid == 3 && eid == 0; }
    }
    if (best < 0) { err = "cmap sem subtabela suportada (formatos 4, 6 ou 0)"; return false; }
    // nomes (id 1 familia, id 4 nome completo)
    if (name && name_len >= 6) {
        const int cnt = u16(name + 2), so = u16(name + 4);
        for (int i = 0; i < cnt; ++i) {
            const std::size_t r = name + 6 + 12 * static_cast<std::size_t>(i);
            if (r + 12 > name + name_len) break;
            const int pid = u16(r), nid = u16(r + 6), len = u16(r + 8), off = u16(r + 10);
            if (nid != 1 && nid != 4) continue;
            const std::size_t p = name + so + off;
            if (p + len > d_.size()) continue;
            std::string s;
            if (pid == 3 || pid == 0) { for (int k = 0; k + 1 < len; k += 2) { const unsigned c = (d_[p + k] << 8) | d_[p + k + 1]; s.push_back(c < 128 ? static_cast<char>(c) : '?'); } }
            else s.assign(reinterpret_cast<const char*>(&d_[p]), len);
            std::string& dst = nid == 1 ? family_ : full_;
            if (dst.empty() || pid == 3) dst = s;
        }
    }
    ok_ = true;
    return true;
}

int TrueTypeFont::glyph_index(std::uint32_t c) const {
    if (!ok_) return 0;
    const std::size_t t = cmap_sub_;
    if (cmap_fmt_ == 0) return c < 256 ? d_.size() > t + 6 + c ? d_[t + 6 + c] : 0 : 0;
    if (cmap_fmt_ == 6) {
        const std::uint32_t first = u16(t + 6), count = u16(t + 8);
        return c >= first && c < first + count ? u16(t + 10 + 2 * (c - first)) : 0;
    }
    if (cmap_fmt_ != 4) return 0;
    auto look = [&](std::uint32_t cp) -> int {
        if (cp > 0xFFFF) return 0;
        const int seg = u16(t + 6) / 2;
        const std::size_t ends = t + 14, starts = ends + 2 * seg + 2, deltas = starts + 2 * seg, ranges = deltas + 2 * seg;
        for (int i = 0; i < seg; ++i) {
            const std::uint32_t end = u16(ends + 2 * i);
            if (end < cp) continue;
            const std::uint32_t start = u16(starts + 2 * i);
            if (start > cp) return 0;
            const std::uint16_t delta = u16(deltas + 2 * i), ro = u16(ranges + 2 * i);
            if (ro == 0) return (cp + delta) & 0xFFFF;
            const std::size_t addr = ranges + 2 * i + ro + 2 * (cp - start);
            const std::uint16_t g = u16(addr);
            return g ? (g + delta) & 0xFFFF : 0;
        }
        return 0;
    };
    int g = look(c);
    if (!g && symbol_) g = look(0xF000 | (c & 0xFF));
    return g < nglyphs_ ? g : 0;
}

int TrueTypeFont::advance(int g) const {
    if (!ok_ || g < 0) return 0;
    const int i = g < nhmetrics_ ? g : nhmetrics_ - 1;
    return u16(hmtx_ + 4 * static_cast<std::size_t>(i));
}

int TrueTypeFont::left_bearing(int g) const {
    if (!ok_ || g < 0) return 0;
    if (g < nhmetrics_) return s16(hmtx_ + 4 * static_cast<std::size_t>(g) + 2);
    return s16(hmtx_ + 4 * static_cast<std::size_t>(nhmetrics_) + 2 * static_cast<std::size_t>(g - nhmetrics_));
}

int TrueTypeFont::kerning(int a, int b) const {
    if (!ok_ || !kern_ || kern_len_ < 4) return 0;
    const int ntab = u16(kern_ + 2);
    std::size_t p = kern_ + 4;
    for (int i = 0; i < ntab && p + 6 <= kern_ + kern_len_; ++i) {
        const int len = u16(p + 2), cov = u16(p + 4);
        if ((cov >> 8) == 0 && (cov & 1)) {
            const int n = u16(p + 6);
            int lo = 0, hi = n - 1;
            const std::uint32_t key = (static_cast<std::uint32_t>(a) << 16) | static_cast<std::uint32_t>(b);
            while (lo <= hi) {
                const int mid = (lo + hi) / 2;
                const std::size_t e = p + 14 + 6 * static_cast<std::size_t>(mid);
                const std::uint32_t k = u32(e);
                if (k == key) return s16(e + 4);
                if (k < key) lo = mid + 1; else hi = mid - 1;
            }
        }
        if (len <= 0) break;
        p += static_cast<std::size_t>(len);
    }
    return 0;
}

bool TrueTypeFont::glyph_range(int g, std::size_t& off, std::size_t& len) const {
    if (g < 0 || g >= nglyphs_) return false;
    std::size_t a, b;
    if (loca_long_) { a = u32(loca_ + 4 * static_cast<std::size_t>(g)); b = u32(loca_ + 4 * static_cast<std::size_t>(g) + 4); }
    else { a = 2u * u16(loca_ + 2 * static_cast<std::size_t>(g)); b = 2u * u16(loca_ + 2 * static_cast<std::size_t>(g) + 2); }
    if (b < a || b > glyf_len_) return false;
    off = glyf_ + a; len = b - a;
    return true;
}

// ------------------------------------------------------------------------------------------------ contornos
namespace {

void flatten_quad(std::vector<TrueTypeFont::Point>& out, TrueTypeFont::Point p0, TrueTypeFont::Point c, TrueTypeFont::Point p1, float tol) {
    const float dx = p0.x - 2 * c.x + p1.x, dy = p0.y - 2 * c.y + p1.y;
    const float dd = std::sqrt(dx * dx + dy * dy);
    int n = static_cast<int>(std::ceil(std::sqrt(dd / (8.f * std::max(tol, 1e-3f)))));
    n = std::clamp(n, 1, 64);
    for (int i = 1; i <= n; ++i) {
        const float t = static_cast<float>(i) / n, u = 1 - t;
        out.push_back({u * u * p0.x + 2 * u * t * c.x + t * t * p1.x, u * u * p0.y + 2 * u * t * c.y + t * t * p1.y});
    }
}

}  // namespace

bool TrueTypeFont::parse_glyph(int g, std::vector<std::vector<Point>>& out, float tol, int depth, const float m[6]) const {
    if (depth > 8) return false;
    std::size_t off, len;
    if (!glyph_range(g, off, len)) return false;
    if (len == 0) return true;                                     // glifo vazio (espaco)
    if (len < 10) return false;
    const std::size_t end = off + len;
    const int nc = s16(off);
    auto tx = [&](float x, float y) { return Point{m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]}; };
    if (nc >= 0) {
        if (off + 10 + 2 * static_cast<std::size_t>(nc) + 2 > end) return false;
        std::vector<int> ends(static_cast<std::size_t>(nc));
        for (int i = 0; i < nc; ++i) ends[static_cast<std::size_t>(i)] = u16(off + 10 + 2 * static_cast<std::size_t>(i));
        const int npts = nc ? ends.back() + 1 : 0;
        if (npts > 20000) return false;
        std::size_t p = off + 10 + 2 * static_cast<std::size_t>(nc);
        p += 2 + u16(p);                                           // instrucoes (ignoradas)
        std::vector<std::uint8_t> fl(static_cast<std::size_t>(npts));
        for (int i = 0; i < npts;) {
            if (p >= end) return false;
            const std::uint8_t f = d_[p++];
            fl[static_cast<std::size_t>(i++)] = f;
            if (f & 8) {
                if (p >= end) return false;
                int rep = d_[p++];
                while (rep-- > 0 && i < npts) fl[static_cast<std::size_t>(i++)] = f;
            }
        }
        std::vector<float> xs(static_cast<std::size_t>(npts)), ys(static_cast<std::size_t>(npts));
        int v = 0;
        for (int i = 0; i < npts; ++i) {
            const std::uint8_t f = fl[static_cast<std::size_t>(i)];
            if (f & 2) { if (p >= end) return false; v += (f & 16) ? d_[p] : -static_cast<int>(d_[p]); ++p; }
            else if (!(f & 16)) { if (p + 2 > end) return false; v += s16(p); p += 2; }
            xs[static_cast<std::size_t>(i)] = static_cast<float>(v);
        }
        v = 0;
        for (int i = 0; i < npts; ++i) {
            const std::uint8_t f = fl[static_cast<std::size_t>(i)];
            if (f & 4) { if (p >= end) return false; v += (f & 32) ? d_[p] : -static_cast<int>(d_[p]); ++p; }
            else if (!(f & 32)) { if (p + 2 > end) return false; v += s16(p); p += 2; }
            ys[static_cast<std::size_t>(i)] = static_cast<float>(v);
        }
        int s = 0;
        for (int ci = 0; ci < nc; ++ci) {
            const int e = ends[static_cast<std::size_t>(ci)];
            if (e < s || e >= npts) return false;
            const int n = e - s + 1;
            auto P = [&](int k) { const int i = s + ((k % n) + n) % n; return tx(xs[static_cast<std::size_t>(i)], ys[static_cast<std::size_t>(i)]); };
            auto on = [&](int k) { const int i = s + ((k % n) + n) % n; return (fl[static_cast<std::size_t>(i)] & 1) != 0; };
            std::vector<Point> poly;
            if (n >= 2) {
                int first = -1;
                for (int k = 0; k < n; ++k) if (on(k)) { first = k; break; }
                Point start, cur, ctrl{};
                bool have_ctrl = false;
                if (first < 0) {                                       // so pontos fora da curva: comeca no meio de P0 e P1
                    const Point a = P(0), b = P(1);
                    start = {(a.x + b.x) / 2, (a.y + b.y) / 2};
                    first = 0;
                } else start = P(first);
                poly.push_back(start);
                cur = start;
                for (int k = 1; k <= n; ++k) {                         // k == n volta ao primeiro ponto (P(first))
                    const int idx = first + k;
                    const Point q = P(idx);
                    if (on(idx)) {
                        if (have_ctrl) flatten_quad(poly, cur, ctrl, q, tol); else poly.push_back(q);
                        cur = q; have_ctrl = false;
                    } else {
                        if (have_ctrl) { const Point mid{(ctrl.x + q.x) / 2, (ctrl.y + q.y) / 2}; flatten_quad(poly, cur, ctrl, mid, tol); cur = mid; }
                        ctrl = q; have_ctrl = true;
                    }
                }
                if (have_ctrl) flatten_quad(poly, cur, ctrl, start, tol);   // fecha no ponto inicial (caso "so fora da curva")
            }
            if (poly.size() >= 3) out.push_back(std::move(poly));
            s = e + 1;
        }
        return true;
    }
    // glifo composto
    std::size_t p = off + 10;
    for (int guard = 0; guard < 64; ++guard) {
        if (p + 4 > end) return false;
        const int flags = u16(p), gi = u16(p + 2);
        p += 4;
        float dx = 0, dy = 0;
        if (flags & 1) { if (p + 4 > end) return false; dx = s16(p); dy = s16(p + 2); p += 4; }
        else { if (p + 2 > end) return false; dx = static_cast<float>(static_cast<std::int8_t>(d_[p])); dy = static_cast<float>(static_cast<std::int8_t>(d_[p + 1])); p += 2; }
        if (!(flags & 2)) dx = dy = 0;                              // casamento por pontos: nao suportado (raro), fica sem deslocamento
        float a = 1, b = 0, c = 0, dd = 1;
        auto f2 = [&](std::size_t o) { return static_cast<float>(s16(o)) / 16384.f; };
        if (flags & 8) { if (p + 2 > end) return false; a = dd = f2(p); p += 2; }
        else if (flags & 0x40) { if (p + 4 > end) return false; a = f2(p); dd = f2(p + 2); p += 4; }
        else if (flags & 0x80) { if (p + 8 > end) return false; a = f2(p); b = f2(p + 2); c = f2(p + 4); dd = f2(p + 6); p += 8; }
        // m_filho = m_pai * local    (local: x' = a x + c y + dx; y' = b x + dd y + dy)
        const float mm[6] = {m[0] * a + m[2] * b, m[1] * a + m[3] * b, m[0] * c + m[2] * dd, m[1] * c + m[3] * dd, m[0] * dx + m[2] * dy + m[4], m[1] * dx + m[3] * dy + m[5]};
        if (!parse_glyph(gi, out, tol, depth + 1, mm)) return false;
        if (!(flags & 0x20)) break;
    }
    return true;
}

bool TrueTypeFont::outline(int g, std::vector<std::vector<Point>>& contours, float tolerance) const {
    contours.clear();
    if (!ok_) return false;
    const float id[6] = {1, 0, 0, 1, 0, 0};
    return parse_glyph(g, contours, tolerance, 0, id);
}

// ------------------------------------------------------------------------------------------------ rasterizacao
bool TrueTypeFont::rasterize(int g, float scale, Bitmap& out) const {
    out = Bitmap();
    if (!ok_ || scale <= 0) return false;
    std::vector<std::vector<Point>> cs;
    if (!outline(g, cs, 0.25f / scale)) return false;              // erro de achatamento <= 1/4 de pixel
    if (cs.empty()) return true;
    float mnx = 1e30f, mny = 1e30f, mxx = -1e30f, mxy = -1e30f;
    for (auto& c : cs) for (auto& p : c) {
        p.x *= scale; p.y *= -scale;                                // pixels, y para baixo
        mnx = std::min(mnx, p.x); mxx = std::max(mxx, p.x); mny = std::min(mny, p.y); mxy = std::max(mxy, p.y);
    }
    const int x0 = static_cast<int>(std::floor(mnx)), y0 = static_cast<int>(std::floor(mny));
    const int w = static_cast<int>(std::ceil(mxx)) - x0 + 1, h = static_cast<int>(std::ceil(mxy)) - y0 + 1;
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048) return false;
    struct Edge { float x0, y0, x1, y1; int dir; };
    std::vector<Edge> edges;
    for (const auto& c : cs)
        for (std::size_t i = 0; i < c.size(); ++i) {
            Point a = c[i], b = c[(i + 1) % c.size()];
            if (a.y == b.y) continue;
            int dir = 1;
            if (a.y > b.y) { std::swap(a, b); dir = -1; }
            edges.push_back({a.x - x0, a.y - y0, b.x - x0, b.y - y0, dir});
        }
    out.w = w; out.h = h; out.x0 = x0; out.y0 = y0;
    out.alpha.assign(static_cast<std::size_t>(w) * h, 0);
    constexpr int S = 5;
    std::vector<float> acc(static_cast<std::size_t>(w) + 2);
    std::vector<std::pair<float, int>> xs;
    auto add_span = [&](float a, float b, float wt) {
        a = std::max(a, 0.f); b = std::min(b, static_cast<float>(w));
        if (b <= a) return;
        const int ia = static_cast<int>(a), ib = static_cast<int>(b);
        if (ia == ib) { acc[static_cast<std::size_t>(ia)] += (b - a) * wt; return; }
        acc[static_cast<std::size_t>(ia)] += (static_cast<float>(ia + 1) - a) * wt;
        for (int i = ia + 1; i < ib; ++i) acc[static_cast<std::size_t>(i)] += wt;
        if (ib < w) acc[static_cast<std::size_t>(ib)] += (b - static_cast<float>(ib)) * wt;
    };
    for (int row = 0; row < h; ++row) {
        std::fill(acc.begin(), acc.end(), 0.f);
        for (int s = 0; s < S; ++s) {
            const float y = static_cast<float>(row) + (static_cast<float>(s) + 0.5f) / S;
            xs.clear();
            for (const Edge& e : edges)
                if (y >= e.y0 && y < e.y1) xs.push_back({e.x0 + (y - e.y0) * (e.x1 - e.x0) / (e.y1 - e.y0), e.dir});
            if (xs.size() < 2) continue;
            std::sort(xs.begin(), xs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            int wind = 0;
            for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
                wind += xs[i].second;
                if (wind != 0) add_span(xs[i].first, xs[i + 1].first, 1.f / S);
            }
        }
        for (int x = 0; x < w; ++x) {
            const float v = std::min(1.f, acc[static_cast<std::size_t>(x)]);
            out.alpha[static_cast<std::size_t>(row) * w + x] = static_cast<std::uint8_t>(v * 255.f + 0.5f);
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ atlas
std::uint32_t cp1252_to_unicode(std::uint8_t c) {
    static const std::uint16_t hi[32] = {0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
                                         0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178};
    return (c >= 0x80 && c < 0xA0) ? hi[c - 0x80] : c;
}

float FontAtlas::measure(const std::string& s) const {
    float x = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const std::uint8_t c = static_cast<std::uint8_t>(s[i]);
        x += glyph[c].advance;
        if (i + 1 < s.size()) {
            const auto it = kern.find((static_cast<std::uint32_t>(c) << 8) | static_cast<std::uint8_t>(s[i + 1]));
            if (it != kern.end()) x += it->second;
        }
    }
    return x;
}

bool bake_atlas(const TrueTypeFont& font, float em, FontAtlas& out) {
    out = FontAtlas();
    if (!font.ok() || em < 2 || em > 256) return false;
    const float sc = font.scale_for_em(em);
    out.em = em;
    out.ascent = font.ascent() * sc;
    out.descent = font.descent() * sc;
    out.line = (font.ascent() - font.descent() + font.line_gap()) * sc;
    std::vector<TrueTypeFont::Bitmap> bm(256);
    int gidx[256] = {0};
    int total_area = 0;
    for (int c = 32; c < 256; ++c) {
        const std::uint32_t u = cp1252_to_unicode(static_cast<std::uint8_t>(c));
        if (!u) continue;
        const int g = font.glyph_index(u);
        if (!g && c != 32) continue;
        gidx[c] = g;
        out.glyph[c].advance = font.advance(g) * sc;
        out.glyph[c].present = true;
        font.rasterize(g, sc, bm[static_cast<std::size_t>(c)]);
        total_area += (bm[static_cast<std::size_t>(c)].w + 2) * (bm[static_cast<std::size_t>(c)].h + 2);
    }
    if (!out.glyph[static_cast<int>(' ')].present) { out.glyph[static_cast<int>(' ')].present = true; out.glyph[static_cast<int>(' ')].advance = em * 0.3f; }
    int W = 128;
    while (W < 2048 && W * W < total_area * 2) W *= 2;
    int x = 1, y = 1, rowh = 0;
    for (int c = 32; c < 256; ++c) {
        const auto& b = bm[static_cast<std::size_t>(c)];
        if (b.w == 0) continue;
        if (x + b.w + 1 > W) { x = 1; y += rowh + 1; rowh = 0; }
        GlyphInfo& gi = out.glyph[c];
        gi.x = x; gi.y = y; gi.w = b.w; gi.h = b.h; gi.ox = b.x0; gi.oy = b.y0;
        x += b.w + 1;
        rowh = std::max(rowh, b.h);
    }
    int H = 16;
    while (H < y + rowh + 1) H *= 2;
    if (H > 4096) return false;
    out.w = W; out.h = H;
    out.alpha.assign(static_cast<std::size_t>(W) * H, 0);
    for (int c = 32; c < 256; ++c) {
        const auto& b = bm[static_cast<std::size_t>(c)];
        const GlyphInfo& gi = out.glyph[c];
        for (int r = 0; r < b.h; ++r)
            std::memcpy(&out.alpha[static_cast<std::size_t>(gi.y + r) * W + gi.x], &b.alpha[static_cast<std::size_t>(r) * b.w], static_cast<std::size_t>(b.w));
    }
    // kerning so entre caracteres presentes (a tabela kern so existe no Gothic 821)
    for (int a = 32; a < 256; ++a) {
        if (!out.glyph[a].present || !gidx[a]) continue;
        for (int b = 32; b < 256; ++b) {
            if (!out.glyph[b].present || !gidx[b]) continue;
            const int k = font.kerning(gidx[a], gidx[b]);
            if (k) out.kern[(static_cast<std::uint32_t>(a) << 8) | static_cast<std::uint32_t>(b)] = k * sc;
        }
    }
    return true;
}

}  // namespace f3v
