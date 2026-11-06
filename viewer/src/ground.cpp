#include "ground.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace f3v {

namespace {
// altura do triangulo em (x, z) pelo baricentro no plano XZ; false se o ponto esta fora ou o triangulo e vertical
bool tri_height(const float* a, const float* b, const float* c, float x, float z, float& y) {
    const float d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2]);
    if (std::fabs(d) < 1e-6f) return false;
    const float l1 = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / d;
    const float l2 = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / d;
    const float l3 = 1.f - l1 - l2;
    const float e = -1e-4f;
    if (l1 < e || l2 < e || l3 < e) return false;
    y = l1 * a[1] + l2 * b[1] + l3 * c[1];
    return true;
}
}  // namespace

void Ground::build(const MapData& m, float cell) {
    tris_.clear(); cells_.clear();
    cell_ = cell;
    x0_ = z0_ = 1e30f; x1_ = z1_ = -1e30f;
    for (const MapTriList& tl : m.tris) {
        if (tl.block >= m.blocks.size()) continue;
        if (tl.material < m.materials.size()) {
            const MapMaterial& mt = m.materials[tl.material];
            if (mt.blend == "ALPHABLEND") continue;                       // vidro, fumaca, sombra pintada: nao e chao
        }
        const MapBlock& b = m.blocks[tl.block];
        for (std::size_t i = 0; i + 2 < tl.idx.size(); i += 3) {
            Tri t;
            const float* p[3] = {&b.xyz[static_cast<std::size_t>(tl.idx[i]) * 3], &b.xyz[static_cast<std::size_t>(tl.idx[i + 1]) * 3],
                                 &b.xyz[static_cast<std::size_t>(tl.idx[i + 2]) * 3]};
            for (int k = 0; k < 3; ++k) { t.a[k] = p[0][k]; t.b[k] = p[1][k]; t.c[k] = p[2][k]; }
            // normal: so superficies com inclinacao ate ~60 graus contam como chao
            const float u[3] = {t.b[0] - t.a[0], t.b[1] - t.a[1], t.b[2] - t.a[2]}, v[3] = {t.c[0] - t.a[0], t.c[1] - t.a[1], t.c[2] - t.a[2]};
            const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len < 1e-8f || std::fabs(n[1]) / len < 0.5f) continue;
            tris_.push_back(t);
            for (const float* q : p) { x0_ = std::min(x0_, q[0]); x1_ = std::max(x1_, q[0]); z0_ = std::min(z0_, q[2]); z1_ = std::max(z1_, q[2]); }
        }
    }
    if (tris_.empty()) return;
    nx_ = std::max(1, static_cast<int>(std::ceil((x1_ - x0_) / cell_)) + 1);
    nz_ = std::max(1, static_cast<int>(std::ceil((z1_ - z0_) / cell_)) + 1);
    cells_.assign(static_cast<std::size_t>(nx_) * static_cast<std::size_t>(nz_), {});
    for (std::uint32_t i = 0; i < tris_.size(); ++i) {
        const Tri& t = tris_[i];
        const float mnx = std::min({t.a[0], t.b[0], t.c[0]}), mxx = std::max({t.a[0], t.b[0], t.c[0]});
        const float mnz = std::min({t.a[2], t.b[2], t.c[2]}), mxz = std::max({t.a[2], t.b[2], t.c[2]});
        const int cx0 = std::clamp(static_cast<int>((mnx - x0_) / cell_), 0, nx_ - 1), cx1 = std::clamp(static_cast<int>((mxx - x0_) / cell_), 0, nx_ - 1);
        const int cz0 = std::clamp(static_cast<int>((mnz - z0_) / cell_), 0, nz_ - 1), cz1 = std::clamp(static_cast<int>((mxz - z0_) / cell_), 0, nz_ - 1);
        for (int cz = cz0; cz <= cz1; ++cz)
            for (int cx = cx0; cx <= cx1; ++cx) cells_[static_cast<std::size_t>(cz) * static_cast<std::size_t>(nx_) + static_cast<std::size_t>(cx)].push_back(i);
    }
}

template <class F> void Ground::each_at(float x, float z, F&& f) const {
    if (cells_.empty() || x < x0_ || z < z0_ || x > x1_ || z > z1_) return;
    const int cx = std::clamp(static_cast<int>((x - x0_) / cell_), 0, nx_ - 1), cz = std::clamp(static_cast<int>((z - z0_) / cell_), 0, nz_ - 1);
    for (std::uint32_t i : cells_[static_cast<std::size_t>(cz) * static_cast<std::size_t>(nx_) + static_cast<std::size_t>(cx)]) {
        const Tri& t = tris_[i];
        float y;
        if (tri_height(t.a, t.b, t.c, x, z, y)) f(y);
    }
}

bool Ground::height(float x, float z, float ref, float& out) const {
    bool ok = false;
    float best = -1e30f;
    each_at(x, z, [&](float y) { if (y <= ref + kStepUp && y > best) { best = y; ok = true; } });
    if (ok) out = best;
    return ok;
}

bool Ground::top(float x, float z, float& out) const {
    bool ok = false;
    float best = -1e30f;
    each_at(x, z, [&](float y) { if (y > best) { best = y; ok = true; } });
    if (ok) out = best;
    return ok;
}

bool Ground::pick(const float o[3], const float d[3], float hit[3]) const {
    if (tris_.empty()) return false;
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-8f) return false;
    const float dir[3] = {d[0] / len, d[1] / len, d[2] / len};
    const float step = cell_ * 0.25f, max_t = 4000.f;
    float prev_t = 0;
    for (float t = step; t < max_t; t += step) {
        const float p[3] = {o[0] + dir[0] * t, o[1] + dir[1] * t, o[2] + dir[2] * t};
        float gy;
        if (top(p[0], p[2], gy) && p[1] <= gy) {
            float a = prev_t, b = t;                                       // refina entre o passo anterior e este
            for (int k = 0; k < 24; ++k) {
                const float m = 0.5f * (a + b);
                const float q[3] = {o[0] + dir[0] * m, o[1] + dir[1] * m, o[2] + dir[2] * m};
                float qy;
                if (top(q[0], q[2], qy) && q[1] <= qy) b = m; else a = m;
            }
            hit[0] = o[0] + dir[0] * b; hit[2] = o[2] + dir[2] * b;
            float hy;
            hit[1] = top(hit[0], hit[2], hy) ? hy : o[1] + dir[1] * b;
            return true;
        }
        prev_t = t;
        if (p[1] < -500.f) break;
    }
    return false;
}

bool Ground::walkable(float xa, float za, float ya, float xb, float zb, float* y_end) const {
    const float dx = xb - xa, dz = zb - za, dist = std::sqrt(dx * dx + dz * dz);
    const int n = std::max(1, static_cast<int>(dist / 0.5f));
    float y = ya;
    for (int i = 1; i <= n; ++i) {
        const float f = static_cast<float>(i) / static_cast<float>(n);
        float gy;
        if (!height(xa + dx * f, za + dz * f, y, gy)) return false;
        if (gy < y - kStepDown) return false;
        y = gy;
    }
    if (y_end) *y_end = y;
    return true;
}

bool Ground::path(float sx, float sz, float sy, float tx, float tz, std::vector<float>& out, int max_nodes) const {
    out.clear();
    float ty;
    if (walkable(sx, sz, sy, tx, tz, &ty)) { out = {tx, tz}; return true; }   // reta livre: sem A*
    const float g = 1.5f;                                                     // grade do A*
    auto key = [](int i, int j) { return (static_cast<std::int64_t>(i) << 32) ^ static_cast<std::uint32_t>(j); };
    const int si = static_cast<int>(std::floor(sx / g)), sj = static_cast<int>(std::floor(sz / g));
    const int ti = static_cast<int>(std::floor(tx / g)), tj = static_cast<int>(std::floor(tz / g));
    struct Node { int i, j; float f; };
    auto cmp = [](const Node& a, const Node& b) { return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);
    std::unordered_map<std::int64_t, float> gcost, ycell;
    std::unordered_map<std::int64_t, std::int64_t> from;
    auto h = [&](int i, int j) { return std::hypot(static_cast<float>(i - ti), static_cast<float>(j - tj)) * g; };
    gcost[key(si, sj)] = 0; ycell[key(si, sj)] = sy;
    open.push({si, sj, h(si, sj)});
    int expanded = 0;
    std::int64_t goal = 0;
    bool found = false;
    while (!open.empty() && expanded < max_nodes) {
        const Node n = open.top(); open.pop();
        const std::int64_t k = key(n.i, n.j);
        ++expanded;
        if (std::abs(n.i - ti) <= 1 && std::abs(n.j - tj) <= 1) { goal = k; found = true; break; }
        const float cy = ycell[k];
        const float cx = (static_cast<float>(n.i) + 0.5f) * g, cz = (static_cast<float>(n.j) + 0.5f) * g;
        for (int di = -1; di <= 1; ++di)
            for (int dj = -1; dj <= 1; ++dj) {
                if (!di && !dj) continue;
                const int ni = n.i + di, nj = n.j + dj;
                const float nxp = (static_cast<float>(ni) + 0.5f) * g, nzp = (static_cast<float>(nj) + 0.5f) * g;
                if (nxp < x0_ || nxp > x1_ || nzp < z0_ || nzp > z1_) continue;
                float ny;
                if (!walkable(cx, cz, cy, nxp, nzp, &ny)) continue;
                const float ng = gcost[k] + (di && dj ? 1.4142f : 1.f) * g;
                const std::int64_t nk = key(ni, nj);
                const auto it = gcost.find(nk);
                if (it != gcost.end() && it->second <= ng) continue;
                gcost[nk] = ng; ycell[nk] = ny; from[nk] = k;
                open.push({ni, nj, ng + h(ni, nj)});
            }
    }
    if (!found) return false;
    std::vector<std::int64_t> chain;
    for (std::int64_t k = goal; k != key(si, sj); k = from[k]) { chain.push_back(k); if (!from.count(k)) break; }
    std::reverse(chain.begin(), chain.end());
    for (std::int64_t k : chain) {
        const int i = static_cast<int>(k >> 32), j = static_cast<int>(static_cast<std::int32_t>(k & 0xffffffff));
        out.push_back((static_cast<float>(i) + 0.5f) * g); out.push_back((static_cast<float>(j) + 0.5f) * g);
    }
    out.push_back(tx); out.push_back(tz);
    return true;
}

}  // namespace f3v
