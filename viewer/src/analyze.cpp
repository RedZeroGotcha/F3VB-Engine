#include "analyze.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

namespace f3v {

namespace {

struct Out {
    std::string s;
    void f(const char* fmt, ...)
#if defined(__GNUC__)
        __attribute__((format(printf, 2, 3)))
#endif
    {
        va_list ap;
        va_start(ap, fmt);
        va_list ap2;
        va_copy(ap2, ap);
        const int n = std::vsnprintf(nullptr, 0, fmt, ap);
        va_end(ap);
        std::vector<char> buf(static_cast<std::size_t>(n > 0 ? n : 0) + 1);
        if (n > 0) std::vsnprintf(buf.data(), buf.size(), fmt, ap2);
        va_end(ap2);
        s += buf.data();
        s += '\n';
    }
};

std::string fmt(const char* f, double a = 0, double b = 0, double c = 0) {
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a, b, c);
    return buf;
}

Vec3 jpos(const Mat4& m) { return Vec3{m.m[12], m.m[13], m.m[14]}; }
double dist(const Vec3& a, const Vec3& b) {
    const double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
double dist3(const float* a, const float* b) {
    const double dx = static_cast<double>(a[0]) - b[0], dy = static_cast<double>(a[1]) - b[1], dz = static_cast<double>(a[2]) - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
double seg_dist(const double p[3], const Vec3& a, const Vec3& b) {
    const double ab[3] = {double(b.x) - a.x, double(b.y) - a.y, double(b.z) - a.z};
    const double ap[3] = {p[0] - a.x, p[1] - a.y, p[2] - a.z};
    const double l2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];
    double t = l2 > 1e-18 ? (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / l2 : 0.0;
    t = std::max(0.0, std::min(1.0, t));
    const double c[3] = {a.x + t * ab[0], a.y + t * ab[1], a.z + t * ab[2]};
    return std::sqrt((p[0] - c[0]) * (p[0] - c[0]) + (p[1] - c[1]) * (p[1] - c[1]) + (p[2] - c[2]) * (p[2] - c[2]));
}
double pct(std::vector<double>& v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const std::size_t i = std::min(v.size() - 1, static_cast<std::size_t>(p * static_cast<double>(v.size() - 1) + 0.5));
    return v[i];
}
void skin_all(Scene& s, std::vector<std::vector<float>>& pos) {
    pos.assign(s.meshes.size(), {});
    std::vector<float> nrm;
    for (std::size_t i = 0; i < s.meshes.size(); ++i) s.skin(static_cast<int>(i), pos[i], nrm);
}
bool pow2(std::uint32_t v) { return v && !(v & (v - 1)); }
const char* verdict(const PoseStats& p) {
    if (p.nonfinite > 0) return "ERRO: valores invalidos (NaN/inf)";
    if (p.edge_p95 > 2.0 || p.edge_p99 > 4.0) return "SUSPEITO: esticamento forte";
    return "ok";
}

}  // namespace

PoseStats measure_pose(Scene& s, int clip, float t) {
    PoseStats st;
    if (s.meshes.empty()) return st;
    s.evaluate(-1, 0.f);
    std::vector<std::vector<float>> bind;
    skin_all(s, bind);
    std::vector<Vec3> jb;
    for (const Mat4& w : s.joint_world) jb.push_back(jpos(w));
    s.evaluate(clip, t);
    std::vector<std::vector<float>> pose;
    skin_all(s, pose);

    float bmn[3] = {0, 0, 0}, bmx[3] = {0, 0, 0};
    bool any = false;
    for (std::size_t mi = 0; mi < pose.size(); ++mi)
        for (std::size_t v = 0; v + 2 < pose[mi].size(); v += 3) {
            for (int i = 0; i < 3; ++i) {
                const float x = pose[mi][v + static_cast<std::size_t>(i)], b = bind[mi][v + static_cast<std::size_t>(i)];
                if (!std::isfinite(x)) { ++st.nonfinite; continue; }
                if (!any) { st.bb_min[i] = st.bb_max[i] = x; bmn[i] = bmx[i] = b; }
                else { st.bb_min[i] = std::min(st.bb_min[i], x); st.bb_max[i] = std::max(st.bb_max[i], x); bmn[i] = std::min(bmn[i], b); bmx[i] = std::max(bmx[i], b); }
            }
            any = true;
        }
    st.ok = any;
    double growth = 0, diag = 0;
    for (int i = 0; i < 3; ++i) {
        const double sb = double(bmx[i]) - bmn[i], sp = double(st.bb_max[i]) - st.bb_min[i];
        diag += sb * sb;
        if (sb > 1e-9) growth = std::max(growth, sp / sb);
    }
    st.bbox_growth = growth;
    diag = std::sqrt(diag);

    std::size_t tris = 0;
    for (const Mesh& m : s.meshes) tris += m.idx.size() / 3;
    const std::size_t stride = std::max<std::size_t>(1, tris * 2 / 6000);
    std::vector<double> ratios;
    const double eps = 1e-9 + 1e-5 * diag;
    for (std::size_t mi = 0; mi < s.meshes.size(); ++mi) {
        const Mesh& m = s.meshes[mi];
        const std::uint32_t nv = m.nv();
        for (std::size_t i = 0; i + 2 < m.idx.size(); i += 3 * stride) {
            const std::uint32_t a = m.idx[i], b = m.idx[i + 1], c = m.idx[i + 2];
            if (a >= nv || b >= nv || c >= nv) continue;
            const std::uint32_t e[2][2] = {{a, b}, {b, c}};
            for (const auto& ed : e) {
                const double lb = dist3(&bind[mi][ed[0] * 3], &bind[mi][ed[1] * 3]);
                const double lp = dist3(&pose[mi][ed[0] * 3], &pose[mi][ed[1] * 3]);
                if (lb > eps && std::isfinite(lp)) ratios.push_back(lp / lb);
            }
        }
    }
    st.edges = static_cast<int>(ratios.size());
    if (!ratios.empty()) {
        std::size_t gt2 = 0;
        for (double r : ratios) if (r > 2.0) ++gt2;
        st.edge_gt2 = static_cast<double>(gt2) / static_cast<double>(ratios.size());
        st.edge_p50 = pct(ratios, 0.50);
        st.edge_p95 = pct(ratios, 0.95);
        st.edge_p99 = pct(ratios, 0.99);
    }
    for (std::size_t j = 0; j < s.joint_world.size() && j < jb.size(); ++j)
        st.max_joint_shift = std::max(st.max_joint_shift, dist(jpos(s.joint_world[j]), jb[j]));
    return st;
}

double estimate_mesh_scale(Scene& s, double* err_now, double* err_best, int* samples) {
    if (err_now) *err_now = 0;
    if (err_best) *err_best = 0;
    if (samples) *samples = 0;
    if (s.joints.empty()) return 0;
    s.evaluate(-1, 0.f);
    std::vector<Vec3> pj;
    for (const Mat4& w : s.joint_world) pj.push_back(jpos(w));
    std::vector<int> child(s.joints.size(), -1);
    for (std::size_t j = 0; j < s.joints.size(); ++j) {
        const int p = s.joints[j].parent;
        if (p >= 0 && p < static_cast<int>(s.joints.size()) && child[static_cast<std::size_t>(p)] < 0) child[static_cast<std::size_t>(p)] = static_cast<int>(j);
    }
    struct Sample { double p[3]; int joint; };
    std::vector<Sample> sm;
    std::size_t total = 0;
    for (const Mesh& m : s.meshes) if (m.has_skin) total += m.nv();
    const std::size_t stride = std::max<std::size_t>(1, total / 2000);
    std::size_t counter = 0;
    for (std::size_t mi = 0; mi < s.meshes.size(); ++mi) {
        const Mesh& m = s.meshes[mi];
        if (!m.has_skin) continue;
        for (std::uint32_t v = 0; v < m.nv(); ++v, ++counter) {
            if (counter % stride) continue;
            float wsum = 0, wbest = 0;
            int kbest = -1;
            for (int k = 0; k < 4; ++k) {
                const float w = m.weights[v * 4 + static_cast<std::uint32_t>(k)];
                if (w <= 0) continue;
                wsum += w;
                if (w > wbest) { wbest = w; kbest = k; }
            }
            if (kbest < 0 || wbest < 0.9f * wsum) continue;
            const int j = s.bone_to_joint(static_cast<int>(mi), m.joints[v * 4 + static_cast<std::uint32_t>(kbest)]);
            if (j < 0) continue;
            Sample x;
            for (int i = 0; i < 3; ++i) x.p[i] = static_cast<double>(m.pos[v * 3 + static_cast<std::uint32_t>(i)]) * s.mesh_scale;
            x.joint = j;
            sm.push_back(x);
        }
    }
    if (samples) *samples = static_cast<int>(sm.size());
    if (sm.size() < 20) return 0;
    auto err = [&](double f) {
        double tot = 0;
        for (const Sample& x : sm) {
            const double p[3] = {x.p[0] * f, x.p[1] * f, x.p[2] * f};
            const std::size_t j = static_cast<std::size_t>(x.joint);
            const Vec3& a = pj[j];
            const Vec3& b = child[j] >= 0 ? pj[static_cast<std::size_t>(child[j])] : pj[j];
            tot += seg_dist(p, a, b);
        }
        return tot / static_cast<double>(sm.size());
    };
    const double lo = std::log(0.02), hi = std::log(50.0);
    const int steps = 240;
    double best_e = 1e300, best_f = 1;
    for (int i = 0; i <= steps; ++i) {
        const double f = std::exp(lo + (hi - lo) * i / steps);
        const double e = err(f);
        if (e < best_e) { best_e = e; best_f = f; }
    }
    const double step = (hi - lo) / steps;
    const double c = std::log(best_f);
    for (int i = -20; i <= 20; ++i) {
        const double f = std::exp(c + step * i / 20.0);
        const double e = err(f);
        if (e < best_e) { best_e = e; best_f = f; }
    }
    if (err_now) *err_now = err(1.0);
    if (err_best) *err_best = best_e;
    return best_f;
}

std::string analyze_scene(Scene& s, const std::string& source) {
    Out hd, o;
    std::vector<std::string> warn;
    const std::size_t nj = s.joints.size();
    auto add_warn = [&warn](const std::string& w) { if (std::find(warn.begin(), warn.end(), w) == warn.end()) warn.push_back(w); };
    std::set<std::string> jnames;
    for (const Joint& j : s.joints) jnames.insert(j.name);

    // ---- texturas / materiais
    o.f("[TEXTURAS] %zu", s.textures.size());
    for (std::size_t i = 0; i < s.textures.size(); ++i) {
        const Texture& t = s.textures[i];
        int amin = 255, amax = 0;
        const std::size_t px = t.rgba.size() / 4, st = std::max<std::size_t>(1, px / 4096);
        for (std::size_t p = 0; p < px; p += st) { const int a = t.rgba[p * 4 + 3]; amin = std::min(amin, a); amax = std::max(amax, a); }
        o.f("  #%zu %-28s %ux%u  %s  alfa %d..%d  %zu bytes%s", i, t.name.c_str(), t.w, t.h, pow2(t.w) && pow2(t.h) ? "potencia de 2" : "NAO potencia de 2 (o visualizador redimensiona)",
            px ? amin : 0, px ? amax : 0, t.rgba.size(), t.rgba.size() != static_cast<std::size_t>(t.w) * t.h * 4 ? "  ERRO: tamanho dos pixels inconsistente" : "");
        if (t.rgba.size() != static_cast<std::size_t>(t.w) * t.h * 4) add_warn("textura #" + std::to_string(i) + ": tamanho dos pixels inconsistente");
    }
    o.f("[MATERIAIS] %zu", s.materials.size());
    for (std::size_t i = 0; i < s.materials.size(); ++i) {
        const Material& m = s.materials[i];
        const bool has = m.texture >= 0 && m.texture < static_cast<int>(s.textures.size());
        o.f("  #%zu %-28s textura=%s cor=(%.2f,%.2f,%.2f,%.2f) alfa_modo=%d dupla_face=%d", i, m.name.c_str(), has ? s.textures[static_cast<std::size_t>(m.texture)].name.c_str() : "(nenhuma)",
            m.color[0], m.color[1], m.color[2], m.color[3], m.alpha_mode, m.double_sided);
    }

    // ---- malhas
    std::size_t total_v = 0, total_t = 0;
    o.f("[MALHAS] %zu   (escala extra aplicada pelo visualizador: x%.5f)", s.meshes.size(), static_cast<double>(s.mesh_scale));
    for (std::size_t mi = 0; mi < s.meshes.size(); ++mi) {
        const Mesh& m = s.meshes[mi];
        const std::uint32_t nv = m.nv();
        total_v += nv;
        total_t += m.idx.size() / 3;
        float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
        for (std::uint32_t v = 0; v < nv; ++v)
            for (int i = 0; i < 3; ++i) {
                const float x = m.pos[v * 3 + static_cast<std::uint32_t>(i)] * s.mesh_scale;
                if (v == 0) mn[i] = mx[i] = x; else { mn[i] = std::min(mn[i], x); mx[i] = std::max(mx[i], x); }
            }
        const bool has_mat = m.material >= 0 && m.material < static_cast<int>(s.materials.size());
        o.f("  #%zu %-26s material=%s  vertices=%u  triangulos=%zu  pele=%s", mi, m.name.c_str(), has_mat ? s.materials[static_cast<std::size_t>(m.material)].name.c_str() : "(nenhum)", nv, m.idx.size() / 3, m.has_skin ? "sim" : "nao");
        if (!has_mat) add_warn("malha '" + m.name + "' sem material");
        o.f("      caixa: min(%.4f, %.4f, %.4f) max(%.4f, %.4f, %.4f) tamanho(%.4f, %.4f, %.4f)", mn[0], mn[1], mn[2], mx[0], mx[1], mx[2], mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2]);
        std::size_t bad_idx = 0, degenerate = 0;
        for (std::size_t i = 0; i + 2 < m.idx.size(); i += 3) {
            const std::uint32_t a = m.idx[i], b = m.idx[i + 1], c = m.idx[i + 2];
            if (a >= nv || b >= nv || c >= nv) { ++bad_idx; continue; }
            if (a == b || b == c || a == c) { ++degenerate; continue; }
            const float* pa = &m.pos[a * 3]; const float* pb = &m.pos[b * 3]; const float* pc = &m.pos[c * 3];
            const double u[3] = {double(pb[0]) - pa[0], double(pb[1]) - pa[1], double(pb[2]) - pa[2]}, w[3] = {double(pc[0]) - pa[0], double(pc[1]) - pa[1], double(pc[2]) - pa[2]};
            const double cx = u[1] * w[2] - u[2] * w[1], cy = u[2] * w[0] - u[0] * w[2], cz = u[0] * w[1] - u[1] * w[0];
            if (cx * cx + cy * cy + cz * cz < 1e-20) ++degenerate;
        }
        o.f("      indices: %zu fora do intervalo, %zu triangulos degenerados", bad_idx, degenerate);
        if (bad_idx) add_warn("malha '" + m.name + "': " + std::to_string(bad_idx) + " triangulos com indice invalido");
        if (m.idx.size() / 3 && degenerate * 100 > m.idx.size() / 3) add_warn("malha '" + m.name + "': mais de 1% de triangulos degenerados");
        if (m.uv.size() == static_cast<std::size_t>(nv) * 2 && nv) {
            float umin = m.uv[0], umax = m.uv[0], vmin = m.uv[1], vmax = m.uv[1];
            std::size_t outside = 0;
            for (std::uint32_t v = 0; v < nv; ++v) {
                const float u = m.uv[v * 2], w = m.uv[v * 2 + 1];
                umin = std::min(umin, u); umax = std::max(umax, u); vmin = std::min(vmin, w); vmax = std::max(vmax, w);
                if (u < -0.001f || u > 1.001f || w < -0.001f || w > 1.001f) ++outside;
            }
            o.f("      UV: u %.3f..%.3f  v %.3f..%.3f  fora de [0,1]: %zu vertices (repeticao de textura se > 0)", umin, umax, vmin, vmax, outside);
        } else {
            o.f("      UV: ausentes ou com tamanho inconsistente");
            add_warn("malha '" + m.name + "' sem UV valida");
        }
        if (m.nrm.size() == static_cast<std::size_t>(nv) * 3 && nv) {
            double lmin = 1e9, lmax = 0;
            std::size_t off = 0;
            for (std::uint32_t v = 0; v < nv; ++v) {
                const double l = std::sqrt(double(m.nrm[v * 3]) * m.nrm[v * 3] + double(m.nrm[v * 3 + 1]) * m.nrm[v * 3 + 1] + double(m.nrm[v * 3 + 2]) * m.nrm[v * 3 + 2]);
                lmin = std::min(lmin, l); lmax = std::max(lmax, l);
                if (std::fabs(l - 1.0) > 0.05) ++off;
            }
            o.f("      normais: comprimento %.3f..%.3f, fora de 1+-0.05: %zu", lmin, lmax, off);
        }
        if (m.has_skin) {
            std::size_t badsum = 0, unmapped = 0, noinfl = 0;
            double smin = 1e9, smax = 0, ssum = 0;
            int maxinfl = 0;
            std::map<int, std::size_t> dom;
            std::set<std::string> missing;
            for (std::uint32_t v = 0; v < nv; ++v) {
                double sum = 0, wbest = 0;
                int n = 0, jbest = -1;
                for (int k = 0; k < 4; ++k) {
                    const float w = m.weights[v * 4 + static_cast<std::uint32_t>(k)];
                    if (w <= 0) continue;
                    ++n; sum += w;
                    const std::uint16_t bi = m.joints[v * 4 + static_cast<std::uint32_t>(k)];
                    const int j = s.bone_to_joint(static_cast<int>(mi), bi);
                    if (j < 0) { ++unmapped; missing.insert(bi < m.bones.size() ? m.bones[bi] : "(indice " + std::to_string(bi) + ")"); }
                    else if (w > wbest) { wbest = w; jbest = j; }
                }
                if (n == 0) ++noinfl;
                smin = std::min(smin, sum); smax = std::max(smax, sum); ssum += sum;
                if (std::fabs(sum - 1.0) > 0.01) ++badsum;
                maxinfl = std::max(maxinfl, n);
                if (jbest >= 0) ++dom[jbest];
            }
            o.f("      pesos: soma %.3f..%.3f (media %.3f), soma fora de 1+-0.01: %zu, maximo de influencias por vertice: %d, vertices sem influencia: %zu", smin, smax, nv ? ssum / nv : 0.0, badsum, maxinfl, noinfl);
            o.f("      ossos citados pela malha: %zu, influencias sem junta correspondente: %zu", m.bones.size(), unmapped);
            std::string alias;
            for (const std::string& b : m.bones) {
                const int j = s.find_joint(b);
                if (j >= 0 && s.joints[static_cast<std::size_t>(j)].name != b) alias += (alias.empty() ? "" : ", ") + b + "->" + s.joints[static_cast<std::size_t>(j)].name;
            }
            if (!alias.empty()) o.f("      ossos casados por variacao de nome (sufixo/maiusculas): %s", alias.c_str());
            if (!missing.empty()) {
                std::string l;
                for (const std::string& n : missing) l += (l.empty() ? "" : ", ") + n;
                o.f("      ossos SEM junta: %s", l.c_str());
                add_warn("malha '" + m.name + "': ossos sem junta no esqueleto: " + l);
            }
            if (noinfl) add_warn("malha '" + m.name + "': " + std::to_string(noinfl) + " vertices sem influencia (ficam parados no bind)");
            if (badsum * 50 > nv) add_warn("malha '" + m.name + "': mais de 2% dos vertices com soma de pesos diferente de 1");
            std::vector<std::pair<std::size_t, int>> top;
            for (const auto& kv : dom) top.push_back({kv.second, kv.first});
            std::sort(top.rbegin(), top.rend());
            std::string l;
            for (std::size_t i = 0; i < top.size() && i < 8; ++i) l += (i ? ", " : "") + s.joints[static_cast<std::size_t>(top[i].second)].name + "=" + std::to_string(top[i].first);
            o.f("      juntas dominantes (vertices): %s%s", l.c_str(), top.size() > 8 ? ", ..." : "");
        }
    }

    // ---- esqueleto
    s.evaluate(-1, 0.f);
    o.f("[ESQUELETO] %zu juntas", nj);
    std::vector<int> depth(nj, 0);
    int roots = 0, maxdepth = 0;
    std::vector<double> lens;
    std::vector<Vec3> bindpos;
    for (const Mat4& w : s.joint_world) bindpos.push_back(jpos(w));
    bool const_off = nj > 0;
    double off0[3] = {0, 0, 0};
    for (std::size_t j = 0; j < nj; ++j) {
        const Joint& jt = s.joints[j];
        if (jt.parent < 0) ++roots; else if (jt.parent < static_cast<int>(j)) { depth[j] = depth[static_cast<std::size_t>(jt.parent)] + 1; lens.push_back(dist(bindpos[j], bindpos[static_cast<std::size_t>(jt.parent)])); }
        else add_warn("junta '" + jt.name + "' tem pai com indice maior que o dela (hierarquia fora de ordem)");
        maxdepth = std::max(maxdepth, depth[j]);
        const Mat4& k = s.joint_skin[j];
        double rd = 0;
        for (int c = 0; c < 3; ++c) for (int r = 0; r < 3; ++r) rd = std::max(rd, std::fabs(double(k.m[c * 4 + r]) - (c == r ? 1.0 : 0.0)));
        const double tr[3] = {k.m[12], k.m[13], k.m[14]};
        if (j == 0) { off0[0] = tr[0]; off0[1] = tr[1]; off0[2] = tr[2]; }
        if (rd > 1e-3 || std::fabs(tr[0] - off0[0]) > 1e-3 || std::fabs(tr[1] - off0[1]) > 1e-3 || std::fabs(tr[2] - off0[2]) > 1e-3) const_off = false;
        if (j < 160) {
            const double ql = std::sqrt(double(jt.q[0]) * jt.q[0] + double(jt.q[1]) * jt.q[1] + double(jt.q[2]) * jt.q[2] + double(jt.q[3]) * jt.q[3]);
            o.f("  %3zu %-26s pai=%3d t(%.4f,%.4f,%.4f) |q|=%.3f s(%.2f,%.2f,%.2f) mundo-bind(%.4f,%.4f,%.4f) mundo*ibm: rot-dev %.5f transl(%.4f,%.4f,%.4f)", j, jt.name.c_str(), jt.parent, jt.t[0], jt.t[1], jt.t[2], ql,
                jt.s[0], jt.s[1], jt.s[2], bindpos[j].x, bindpos[j].y, bindpos[j].z, rd, tr[0], tr[1], tr[2]);
            if (std::fabs(ql - 1.0) > 0.01) add_warn("junta '" + jt.name + "': quaternion de bind nao normalizado");
        }
    }
    if (nj > 160) o.f("  ... (%zu juntas nao listadas)", nj - 160);
    double lmin = 0, lmax = 0, lavg = 0;
    if (!lens.empty()) { lmin = *std::min_element(lens.begin(), lens.end()); lmax = *std::max_element(lens.begin(), lens.end()); for (double l : lens) lavg += l; lavg /= static_cast<double>(lens.size()); }
    o.f("  raizes: %d, profundidade maxima: %d, comprimento de osso (bind): min %.4f media %.4f max %.4f", roots, maxdepth, lmin, lavg, lmax);
    if (nj && const_off && (std::fabs(off0[0]) > 1e-5 || std::fabs(off0[1]) > 1e-5 || std::fabs(off0[2]) > 1e-5)) {
        o.f("  AVISO: mundo*ibm = deslocamento CONSTANTE (%.4f, %.4f, %.4f) em todas as juntas: a pose de bind skinada = malha + esse deslocamento "
            "(posicionamento inicial do GR2; o conversor v12 compensa, F3M do v11 nao).", off0[0], off0[1], off0[2]);
        add_warn(fmt("pose de bind deslocada de forma constante (%.4f, %.4f, %.4f); reconverter com o v12 corrige", off0[0], off0[1], off0[2]));
    } else if (nj) {
        double worst = 0;
        for (std::size_t j = 0; j < nj; ++j) for (int i = 12; i < 15; ++i) worst = std::max(worst, std::fabs(double(s.joint_skin[j].m[i])));
        if (worst > 1e-3) add_warn(fmt("mundo*ibm no bind difere da identidade (translacao max %.4f): esqueleto e ibm inconsistentes", worst));
    }

    // ---- escala malha x esqueleto
    if (nj && !s.meshes.empty()) {
        o.f("[ESCALA MALHA x ESQUELETO]");
        float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
        bool any = false;
        for (const Mesh& m : s.meshes) for (std::uint32_t v = 0; v < m.nv(); ++v) { any = true; for (int i = 0; i < 3; ++i) { const float x = m.pos[v * 3 + static_cast<std::uint32_t>(i)] * s.mesh_scale; mn[i] = std::min(mn[i], x); mx[i] = std::max(mx[i], x); } }
        float jn[3] = {1e30f, 1e30f, 1e30f}, jx[3] = {-1e30f, -1e30f, -1e30f};
        for (const Vec3& p : bindpos) { const float c[3] = {p.x, p.y, p.z}; for (int i = 0; i < 3; ++i) { jn[i] = std::min(jn[i], c[i]); jx[i] = std::max(jx[i], c[i]); } }
        if (any) o.f("  tamanho da malha (x%.5f aplicado): (%.4f, %.4f, %.4f)   tamanho da caixa dos ossos: (%.4f, %.4f, %.4f)", static_cast<double>(s.mesh_scale), mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2], jx[0] - jn[0], jx[1] - jn[1], jx[2] - jn[2]);
        double en = 0, eb = 0;
        int ns = 0;
        const double f = estimate_mesh_scale(s, &en, &eb, &ns);
        if (f <= 0) {
            o.f("  estimativa por ajuste: sem dados suficientes (%d vertices de peso ~1)", ns);
        } else {
            o.f("  estimativa por ajuste (%d vertices de peso dominante vs o segmento do osso dono): fator EXTRA f = %.4f  (1 = ja correto)", ns, f);
            o.f("  erro medio ate o osso: com f=1: %.4f   com f=%.4f: %.4f   (unidades do esqueleto)", en, f, eb);
            o.f("  referencia: 100/1024 = %.5f  (hipotese do conversor v12 para F3M do v11)", kMeshScaleHypothesis);
            if (f < 0.85 || f > 1.18) {
                o.f("  AVISO: a malha NAO esta na escala do esqueleto. Na pose de bind parece certa, mas qualquer animacao gira os vertices em torno de pivos errados.");
                o.f("         Teste no visualizador: tecla N (aplica f estimado), M (alterna 1/10.24) ou , . (ajuste fino); ou reconverta com o v12.");
                add_warn(fmt("malha fora da escala do esqueleto: fator extra estimado %.4f (esperado ~1.0)", f));
            } else {
                o.f("  ok: escala coerente com o esqueleto.");
            }
        }
    }

    // ---- animacoes
    o.f("[ANIMACOES] %zu", s.clips.size());
    int suspicious = 0;
    for (std::size_t ci = 0; ci < s.clips.size() && ci < 120; ++ci) {
        const Clip& c = s.clips[ci];
        int cnt[3] = {0, 0, 0};
        std::size_t keys = 0;
        std::set<std::string> with, unmatched;
        float tmax = 0;
        for (const Channel& ch : c.channels) {
            if (ch.path < 3) ++cnt[ch.path];
            keys += ch.times.size();
            if (!ch.times.empty()) tmax = std::max(tmax, ch.times.back());
            if (s.find_joint(ch.bone) >= 0) with.insert(ch.bone); else unmatched.insert(ch.bone);
        }
        o.f("  #%zu %-44s dur %.3fs (ultima chave %.3fs)  canais T%d R%d S%d  chaves %zu  ossos sem junta: %zu  juntas sem canal: %zu", ci, c.name.c_str(), static_cast<double>(c.duration), static_cast<double>(tmax), cnt[0], cnt[1], cnt[2], keys, unmatched.size(), nj - with.size());
        if (!unmatched.empty() && unmatched.size() * 2 > c.channels.size()) add_warn("animacao '" + c.name + "': a maioria dos canais nao casa com as juntas");
        if (!s.meshes.empty() && nj) {
            bool bad = false;
            for (double fr : {0.25, 0.50, 0.75}) {
                const PoseStats p = measure_pose(s, static_cast<int>(ci), c.duration * static_cast<float>(fr));
                o.f("      t=%2.0f%%: caixa x%.2f  arestas p50 %.2f p95 %.2f p99 %.2f  >2x %.1f%%  deslocamento max de junta %.4f  [%s]", fr * 100, p.bbox_growth, p.edge_p50, p.edge_p95, p.edge_p99, p.edge_gt2 * 100, p.max_joint_shift, verdict(p));
                if (std::string(verdict(p)) != "ok") bad = true;
            }
            if (bad) ++suspicious;
        }
    }
    if (s.clips.size() > 120) o.f("  ... (%zu animacoes nao listadas)", s.clips.size() - 120);
    if (suspicious) add_warn(std::to_string(suspicious) + " de " + std::to_string(s.clips.size()) + " animacoes com esticamento suspeito (arestas p95 > 2x ou p99 > 4x do comprimento no bind)");
    s.evaluate(-1, 0.f);

    // ---- cabecalho + resumo
    hd.f("=== ANALISE DO MODELO: %s%s%s ===", s.model_name.empty() ? "(sem nome)" : s.model_name.c_str(), source.empty() ? "" : "  arquivo: ", source.c_str());
    hd.f("texturas %zu | materiais %zu | malhas %zu (%zu vertices, %zu triangulos) | juntas %zu | animacoes %zu", s.textures.size(), s.materials.size(), s.meshes.size(), total_v, total_t, nj, s.clips.size());
    hd.f("[RESUMO] %zu aviso(s)%s", warn.size(), warn.empty() ? ": nenhum problema detectado pelas verificacoes automaticas" : ":");
    for (const std::string& w : warn) hd.f("  - %s", w.c_str());
    hd.s += "\n";
    return hd.s + o.s;
}

std::string analyze_pose(Scene& s, int clip, float t, bool bind_mode) {
    Out o;
    const bool use = !bind_mode && clip >= 0 && clip < static_cast<int>(s.clips.size());
    if (use) o.f("pose: animacao #%d \"%s\" t=%.3f/%.3fs", clip, s.clips[static_cast<std::size_t>(clip)].name.c_str(), static_cast<double>(t), static_cast<double>(s.clips[static_cast<std::size_t>(clip)].duration));
    else o.f("pose: BIND (sem animacao)");
    const PoseStats p = measure_pose(s, use ? clip : -1, t);
    if (!p.ok) { o.f("  sem malha para medir"); return o.s; }
    o.f("  caixa: min(%.4f, %.4f, %.4f) max(%.4f, %.4f, %.4f)  crescimento vs bind x%.2f", p.bb_min[0], p.bb_min[1], p.bb_min[2], p.bb_max[0], p.bb_max[1], p.bb_max[2], p.bbox_growth);
    o.f("  arestas (%d amostras): p50 %.2f p95 %.2f p99 %.2f  >2x %.1f%%   deslocamento max de junta %.4f   escala extra x%.5f   [%s]", p.edges, p.edge_p50, p.edge_p95, p.edge_p99, p.edge_gt2 * 100, p.max_joint_shift, static_cast<double>(s.mesh_scale), verdict(p));
    return o.s;
}

std::string dump_bones(Scene& s, int clip, float t) {
    Out o;
    const std::size_t nj = s.joints.size();
    if (!nj) { o.f("ossos: o modelo nao tem esqueleto"); return o.s; }
    s.evaluate(-1, 0.f);
    std::vector<Vec3> bind;
    for (const Mat4& w : s.joint_world) bind.push_back(jpos(w));
    const bool use = clip >= 0 && clip < static_cast<int>(s.clips.size());
    s.evaluate(use ? clip : -1, t);
    o.f("ossos (%zu), pose: %s", nj, use ? s.clips[static_cast<std::size_t>(clip)].name.c_str() : "bind");
    double worst = 0;
    std::size_t wj = 0;
    float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    for (std::size_t j = 0; j < nj; ++j) {
        const Vec3 p = jpos(s.joint_world[j]);
        const double sh = dist(p, bind[j]);
        if (sh > worst) { worst = sh; wj = j; }
        const float c[3] = {p.x, p.y, p.z};
        for (int i = 0; i < 3; ++i) { mn[i] = std::min(mn[i], c[i]); mx[i] = std::max(mx[i], c[i]); }
        const int par = s.joints[j].parent;
        const double len = par >= 0 && par < static_cast<int>(nj) ? dist(p, jpos(s.joint_world[static_cast<std::size_t>(par)])) : 0.0;
        o.f("  %3zu %-26s pai=%3d pos(%.4f, %.4f, %.4f)  deslocamento vs bind %.4f  comprimento ate o pai %.4f", j, s.joints[j].name.c_str(), par, p.x, p.y, p.z, sh, len);
    }
    o.f("  caixa dos ossos: min(%.4f, %.4f, %.4f) max(%.4f, %.4f, %.4f); maior deslocamento: %s (%.4f)", mn[0], mn[1], mn[2], mx[0], mx[1], mx[2], s.joints[wj].name.c_str(), worst);
    return o.s;
}

}  // namespace f3v
