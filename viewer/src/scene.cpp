#include "scene.h"

#include <algorithm>
#include <cmath>

namespace f3v {

void Scene::clear_clips() {
    clips.clear();
    clip_bind_.clear();
}

LoadReport Scene::add(F3MFile&& f) {
    LoadReport rep;
    const bool has_model = !f.meshes.empty();
    if (has_model) {
        textures = std::move(f.textures);
        materials = std::move(f.materials);
        meshes = std::move(f.meshes);
        model_name = f.name;
        rep.replaced_model = true;
        if (!f.joints.empty()) {
            joints = std::move(f.joints);
            clips.clear();
            rep.replaced_skeleton = true;
        }
    } else if (!f.joints.empty() && joints.empty()) {
        joints = std::move(f.joints);
        rep.adopted_skeleton = true;
    }
    for (Clip& c : f.clips) {
        int dup = 1;
        std::string base = c.name;
        auto exists = [&](const std::string& n) { return std::any_of(clips.begin(), clips.end(), [&](const Clip& o) { return o.name == n; }); };
        while (exists(c.name)) c.name = base + " (" + std::to_string(++dup) + ")";
        clips.push_back(std::move(c));
        ++rep.clips_added;
    }
    rebuild();
    for (std::size_t i = clips.size() - static_cast<std::size_t>(rep.clips_added); i < clips.size(); ++i)
        for (const Channel& ch : clips[i].channels)
            if (find_joint(ch.bone) < 0) ++rep.channels_unmatched;
    rep.message = std::string(rep.replaced_model ? "modelo carregado" : "complemento carregado") + "; " + std::to_string(rep.clips_added) +
                  " animacoes adicionadas" + (rep.adopted_skeleton ? "; esqueleto adotado" : "") +
                  (rep.channels_unmatched ? "; " + std::to_string(rep.channels_unmatched) + " canais sem osso correspondente" : "");
    return rep;
}

void Scene::rebuild() {
    joint_index_.clear();
    for (std::size_t i = 0; i < joints.size(); ++i) joint_index_.emplace(joints[i].name, static_cast<int>(i));
    mesh_map_.assign(meshes.size(), {});
    for (std::size_t m = 0; m < meshes.size(); ++m)
        for (const std::string& b : meshes[m].bones) {
            mesh_map_[m].push_back(find_joint(b));
        }
    clip_bind_.assign(clips.size(), {});
    joint_world.assign(joints.size(), Mat4());
    joint_skin.assign(joints.size(), Mat4());
}

void Scene::bind_clip(int c) {
    if (static_cast<int>(clip_bind_.size()) <= c) clip_bind_.resize(clips.size());
    auto& b = clip_bind_[static_cast<std::size_t>(c)];
    if (!b.empty()) return;
    b.assign(joints.size() * 3, -1);
    const Clip& clip = clips[static_cast<std::size_t>(c)];
    for (std::size_t k = 0; k < clip.channels.size(); ++k) {
        const int j = find_joint(clip.channels[k].bone);
        if (j >= 0) b[static_cast<std::size_t>(j) * 3 + clip.channels[k].path] = static_cast<int>(k);
    }
}

static bool sample(const Channel& ch, float t, float* out) {
    const int dim = ch.path == 1 ? 4 : 3;
    const std::size_t n = ch.times.size();
    if (n == 0 || ch.values.size() < n * static_cast<std::size_t>(dim)) return false;
    if (n == 1 || t <= ch.times[0]) { for (int i = 0; i < dim; ++i) out[i] = ch.values[static_cast<std::size_t>(i)]; return true; }
    if (t >= ch.times[n - 1]) { for (int i = 0; i < dim; ++i) out[i] = ch.values[(n - 1) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(i)]; return true; }
    std::size_t hi = static_cast<std::size_t>(std::upper_bound(ch.times.begin(), ch.times.end(), t) - ch.times.begin());
    if (hi == 0) hi = 1;
    if (hi >= n) hi = n - 1;
    const std::size_t lo = hi - 1;
    const float span = ch.times[hi] - ch.times[lo];
    const float u = span > 0 ? (t - ch.times[lo]) / span : 0.f;
    const float* a = &ch.values[lo * static_cast<std::size_t>(dim)];
    const float* b = &ch.values[hi * static_cast<std::size_t>(dim)];
    if (ch.path == 1) slerp(a, b, u, out);
    else for (int i = 0; i < dim; ++i) out[i] = a[i] + u * (b[i] - a[i]);
    return true;
}

void Scene::evaluate(int clip, float t) {
    const bool use = clip >= 0 && clip < static_cast<int>(clips.size());
    if (use) bind_clip(clip);
    joint_world.resize(joints.size());
    joint_skin.resize(joints.size());
    for (std::size_t j = 0; j < joints.size(); ++j) {
        const Joint& jt = joints[j];
        float tr[3] = {jt.t[0], jt.t[1], jt.t[2]}, q[4] = {jt.q[0], jt.q[1], jt.q[2], jt.q[3]}, s[3] = {jt.s[0], jt.s[1], jt.s[2]};
        if (use) {
            const auto& b = clip_bind_[static_cast<std::size_t>(clip)];
            const Clip& c = clips[static_cast<std::size_t>(clip)];
            if (b[j * 3 + 0] >= 0) sample(c.channels[static_cast<std::size_t>(b[j * 3 + 0])], t, tr);
            if (b[j * 3 + 1] >= 0) sample(c.channels[static_cast<std::size_t>(b[j * 3 + 1])], t, q);
            if (b[j * 3 + 2] >= 0) sample(c.channels[static_cast<std::size_t>(b[j * 3 + 2])], t, s);
        }
        const Mat4 local = compose_trs(tr, q, s);
        joint_world[j] = (jt.parent >= 0 && jt.parent < static_cast<int>(j)) ? mul(joint_world[static_cast<std::size_t>(jt.parent)], local) : local;
        Mat4 ibm;
        std::memcpy(ibm.m, jt.ibm, sizeof ibm.m);
        joint_skin[j] = mul(joint_world[j], ibm);
    }
}

int Scene::find_joint(const std::string& name) const {
    auto it = joint_index_.find(name);
    if (it != joint_index_.end()) return it->second;
    for (int k = 1; k <= 9; ++k) {
        it = joint_index_.find(name + "_" + std::to_string(k));
        if (it != joint_index_.end()) return it->second;
    }
    const std::size_t p = name.rfind('_');
    if (p != std::string::npos && p + 1 < name.size() && name.find_first_not_of("0123456789", p + 1) == std::string::npos) {
        it = joint_index_.find(name.substr(0, p));
        if (it != joint_index_.end()) return it->second;
    }
    auto lower = [](std::string s) { for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a'); return s; };
    const std::string ln = lower(name);
    for (std::size_t i = 0; i < joints.size(); ++i)
        if (lower(joints[i].name) == ln) return static_cast<int>(i);
    for (std::size_t i = 0; i < joints.size(); ++i) {                    // maiusculas + sufixo "_N" da junta
        const std::string jl = lower(joints[i].name);
        const std::size_t q = jl.rfind('_');
        if (q != std::string::npos && q + 1 < jl.size() && jl.find_first_not_of("0123456789", q + 1) == std::string::npos && jl.substr(0, q) == ln) return static_cast<int>(i);
    }
    return -1;
}

int Scene::bone_to_joint(int mesh, int bone_index) const {
    if (mesh < 0 || mesh >= static_cast<int>(mesh_map_.size())) return -1;
    const auto& m = mesh_map_[static_cast<std::size_t>(mesh)];
    return (bone_index >= 0 && bone_index < static_cast<int>(m.size())) ? m[static_cast<std::size_t>(bone_index)] : -1;
}

void Scene::skin(int mi, std::vector<float>& pos, std::vector<float>& nrm) const {
    const Mesh& m = meshes[static_cast<std::size_t>(mi)];
    const float ms = mesh_scale;
    pos = m.pos;
    nrm = m.nrm;
    if (ms != 1.f) for (float& x : pos) x *= ms;
    if (!m.has_skin || joints.empty() || joint_skin.size() != joints.size()) return;
    const std::uint32_t nv = m.nv();
    for (std::uint32_t v = 0; v < nv; ++v) {
        float p[3] = {0, 0, 0}, n[3] = {0, 0, 0}, wsum = 0, wmiss = 0;
        for (int k = 0; k < 4; ++k) {
            const float w = m.weights[v * 4 + static_cast<std::uint32_t>(k)];
            if (w <= 0) continue;
            const int j = bone_to_joint(mi, m.joints[v * 4 + static_cast<std::uint32_t>(k)]);
            if (j < 0) { wmiss += w; continue; }               // osso sem junta: essa parte do peso fica no bind (nao colapsa para a origem)
            const Mat4& M = joint_skin[static_cast<std::size_t>(j)];
            const Vec3 pp = transform_point(M, m.pos[v * 3] * ms, m.pos[v * 3 + 1] * ms, m.pos[v * 3 + 2] * ms);
            const Vec3 nn = transform_dir(M, m.nrm[v * 3], m.nrm[v * 3 + 1], m.nrm[v * 3 + 2]);
            p[0] += w * pp.x; p[1] += w * pp.y; p[2] += w * pp.z;
            n[0] += w * nn.x; n[1] += w * nn.y; n[2] += w * nn.z;
            wsum += w;
        }
        if (wsum <= 1e-6f) continue;                       // sem influencia valida: mantem o bind
        if (wmiss > 0) {
            p[0] += wmiss * m.pos[v * 3] * ms; p[1] += wmiss * m.pos[v * 3 + 1] * ms; p[2] += wmiss * m.pos[v * 3 + 2] * ms;
            n[0] += wmiss * m.nrm[v * 3]; n[1] += wmiss * m.nrm[v * 3 + 1]; n[2] += wmiss * m.nrm[v * 3 + 2];
        }
        const float wt = wsum + wmiss;
        for (int i = 0; i < 3; ++i) p[i] /= wt;
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        for (int i = 0; i < 3; ++i) { pos[v * 3 + static_cast<std::uint32_t>(i)] = p[i]; nrm[v * 3 + static_cast<std::uint32_t>(i)] = len > 1e-9f ? n[i] / len : 0.f; }
    }
}

bool Scene::bounds(float mn[3], float mx[3]) const {
    bool any = false;
    for (const Mesh& m : meshes)
        for (std::uint32_t v = 0; v < m.nv(); ++v)
            for (int i = 0; i < 3; ++i) {
                const float x = m.pos[v * 3 + static_cast<std::uint32_t>(i)] * mesh_scale;
                if (!any) { mn[i] = mx[i] = x; } else { mn[i] = std::min(mn[i], x); mx[i] = std::max(mx[i], x); }
                if (i == 2) any = true;
            }
    return any;
}

void resize_pow2(const Texture& t, std::vector<std::uint8_t>& out, std::uint32_t& w, std::uint32_t& h) {
    auto nearest_pot = [](std::uint32_t v) {
        std::uint32_t p = 1;
        while (p < v && p < 2048) p <<= 1;
        if (p > 1 && p < 2048 && (p - v) > (v - p / 2)) p >>= 1;   // mais perto da potencia de baixo
        return std::min<std::uint32_t>(p, 2048);
    };
    w = nearest_pot(t.w);
    h = nearest_pot(t.h);
    if (w == t.w && h == t.h) { out = t.rgba; return; }
    out.assign(static_cast<std::size_t>(w) * h * 4, 0);
    for (std::uint32_t y = 0; y < h; ++y) {
        const float sy = (static_cast<float>(y) + 0.5f) * static_cast<float>(t.h) / static_cast<float>(h) - 0.5f;
        const int y0 = std::max(0, static_cast<int>(std::floor(sy))), y1 = std::min<int>(static_cast<int>(t.h) - 1, y0 + 1);
        const float fy = std::min(1.f, std::max(0.f, sy - static_cast<float>(y0)));
        for (std::uint32_t x = 0; x < w; ++x) {
            const float sx = (static_cast<float>(x) + 0.5f) * static_cast<float>(t.w) / static_cast<float>(w) - 0.5f;
            const int x0 = std::max(0, static_cast<int>(std::floor(sx))), x1 = std::min<int>(static_cast<int>(t.w) - 1, x0 + 1);
            const float fx = std::min(1.f, std::max(0.f, sx - static_cast<float>(x0)));
            for (int c = 0; c < 4; ++c) {
                auto px = [&](int xx, int yy) { return static_cast<float>(t.rgba[(static_cast<std::size_t>(yy) * t.w + static_cast<std::size_t>(xx)) * 4 + static_cast<std::size_t>(c)]); };
                const float top = px(x0, y0) + fx * (px(x1, y0) - px(x0, y0));
                const float bot = px(x0, y1) + fx * (px(x1, y1) - px(x0, y1));
                out[(static_cast<std::size_t>(y) * w + x) * 4 + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(top + fy * (bot - top) + 0.5f);
            }
        }
    }
}

}  // namespace f3v
