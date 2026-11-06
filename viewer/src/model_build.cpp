// v20: B3D + GR2 -> F3MFile em C++ (ver model_build.h). Cada funcao indica a funcao Python de origem.
#include "model_build.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_map>

#include "image.h"

namespace f3v {

namespace {

std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; }
bool ignored_group(const std::string& n) { const std::string l = lower(n); return l == "light" || l == "camera"; }

// export.py: _quat_to_mat3 / _joint_local / _mat4_mul (forma coluna, 4x4 row-major em [r][c])
using M4 = std::array<std::array<double, 4>, 4>;
M4 mul(const M4& a, const M4& b) {
    M4 r{};
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) { double s = 0; for (int k = 0; k < 4; ++k) s += a[i][k] * b[k][j]; r[i][j] = s; }
    return r;
}
M4 joint_local(const RawJoint& j) {
    double x = j.quat[0], y = j.quat[1], z = j.quat[2], w = j.quat[3];
    double n = std::sqrt(x * x + y * y + z * z + w * w);
    if (n == 0) n = 1.0;
    x /= n; y /= n; z /= n; w /= n;
    const double r[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
                            {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
                            {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
    M4 m{};
    for (int i = 0; i < 3; ++i) { for (int c = 0; c < 3; ++c) m[i][c] = r[i][c] * j.scale[c]; m[i][3] = j.pos[i]; }
    m[3] = {0, 0, 0, 1};
    return m;
}

// export.py: detect_initial_placement
bool detect_initial_placement(const std::vector<RawJoint>& joints, double out[3], double tol = 1e-3) {
    if (joints.empty()) return false;
    std::vector<M4> world(joints.size());
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const M4 loc = joint_local(joints[i]);
        const int p = joints[i].parent;
        world[i] = (p < 0 || static_cast<std::size_t>(p) >= i) ? loc : mul(world[static_cast<std::size_t>(p)], loc);
    }
    bool have = false;
    double t0[3] = {0, 0, 0};
    for (std::size_t i = 0; i < joints.size(); ++i) {
        M4 ibm_col{};
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) ibm_col[r][c] = joints[i].inv_world[c * 4 + r];
        const M4 p = mul(world[i], ibm_col);
        double dev = 0;
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) dev = std::max(dev, std::fabs(p[r][c] - (r == c ? 1.0 : 0.0)));
        if (dev > tol) return false;
        const double t[3] = {p[0][3], p[1][3], p[2][3]};
        if (!have) { for (int k = 0; k < 3; ++k) t0[k] = t[k]; have = true; }
        else { double d = 0; for (int k = 0; k < 3; ++k) d = std::max(d, std::fabs(t[k] - t0[k])); if (d > tol) return false; }
    }
    if (!have || std::max({std::fabs(t0[0]), std::fabs(t0[1]), std::fabs(t0[2])}) < 1e-5) return false;
    for (int k = 0; k < 3; ++k) out[k] = -t0[k];
    return true;
}

// export.py: resolve_bone_aliases
std::vector<std::string> resolve_bone_aliases(const std::vector<std::string>& bones, const std::vector<RawJoint>& joints, int& aliased) {
    aliased = 0;
    if (joints.empty()) return bones;
    std::map<std::string, int> jn;
    for (const RawJoint& j : joints) jn[j.name] = 1;
    std::map<std::string, std::string> low;                       // sorted(jn): o primeiro em ordem alfabetica vence
    for (const auto& kv : jn) low.emplace(lower(kv.first), kv.first);
    std::vector<std::string> out;
    for (const std::string& b : bones) {
        std::string pick;
        if (jn.count(b)) pick = b;
        if (pick.empty()) for (int k = 1; k < 10; ++k) { const std::string c = b + "_" + std::to_string(k); if (jn.count(c)) { pick = c; break; } }
        if (pick.empty()) {
            const std::size_t u = b.rfind('_');
            if (u != std::string::npos && u + 1 < b.size() && std::all_of(b.begin() + static_cast<long>(u) + 1, b.end(), [](char c) { return c >= '0' && c <= '9'; })) {
                const std::string base = b.substr(0, u);
                if (jn.count(base)) pick = base;
            }
        }
        if (pick.empty()) { const auto it = low.find(lower(b)); if (it != low.end()) pick = it->second; }
        if (!pick.empty() && pick != b) ++aliased;
        out.push_back(pick.empty() ? b : pick);
    }
    return out;
}

// conversoes do Exporter (k = 1, convert = True)
void cv_p(const double* v, float* o) { o[0] = static_cast<float>(v[0]); o[1] = static_cast<float>(v[1]); o[2] = static_cast<float>(-v[2]); }
void cv_q(const double* q, float* o) {
    double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n == 0) n = 1.0;
    o[0] = static_cast<float>(-(q[0] / n)); o[1] = static_cast<float>(-(q[1] / n)); o[2] = static_cast<float>(q[2] / n); o[3] = static_cast<float>(q[3] / n);
}
void cv_m4(const double* m, float* o) {
    static const double s[4] = {1, 1, -1, 1};
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) o[r * 4 + c] = static_cast<float>(m[r * 4 + c] * s[r] * s[c]);
}

// export.py: _fix_winding (sobre as posicoes/normais JA convertidas, em double como no Python)
void fix_winding(std::vector<std::uint32_t>& idx, const std::vector<double>& pos, const std::vector<double>& nor) {
    long agree = 0, total = 0;
    for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
        const double* a = &pos[3 * idx[i]]; const double* b = &pos[3 * idx[i + 1]]; const double* c = &pos[3 * idx[i + 2]];
        const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const double cr[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        double nn[3] = {0, 0, 0};
        for (int k = 0; k < 3; ++k) nn[k] = (0.0 + nor[3 * idx[i] + k]) + nor[3 * idx[i + 1] + k] + nor[3 * idx[i + 2] + k];
        const double d = cr[0] * nn[0] + cr[1] * nn[1] + cr[2] * nn[2];
        if (std::fabs(d) > 1e-12) { ++total; if (d > 0) ++agree; }
    }
    if (total && static_cast<double>(agree) < static_cast<double>(total) / 2.0)
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3) std::swap(idx[i + 1], idx[i + 2]);
}

// gr2export.py: _curve_keys
bool curve_keys(const Gr2Curve& c, double duration, double step, int dim, std::vector<double>& times, std::vector<double>& vals) {
    times.clear(); vals.clear();
    if (!c.present || c.knots.empty() || c.dim < dim) return false;
    const long n = std::max(1L, static_cast<long>(std::nearbyint(duration / step)) + 1);
    double tmp[16];
    for (long i = 0; i < n; ++i) {
        const double t = std::min(duration, static_cast<double>(i) * step);
        c.eval(t, tmp);
        times.push_back(t);
        for (int k = 0; k < dim; ++k) vals.push_back(tmp[k]);
    }
    return true;
}

}  // namespace

std::vector<std::string> anim_prefixes(const std::string& name) {
    std::vector<std::string> out{name + "_"};
    if (name.size() >= 4 && name.compare(0, 3, "Hum") == 0 && (name[3] == 'M' || name[3] == 'F')) {
        const std::string f = name.substr(0, 4) + "_";
        if (f != out[0]) out.push_back(f);
    }
    return out;
}

bool raw_skeleton(const Gr2File& f, std::vector<RawJoint>& out, std::string* name) {
    out.clear();
    for (const Gr2Skeleton& s : f.skeletons) {
        if (ignored_group(s.name) || s.bones.size() <= 1) continue;
        for (std::size_t i = 0; i < s.bones.size(); ++i) {
            const Gr2Bone& b = s.bones[i];
            RawJoint j;
            j.name = b.name;
            j.parent = (b.parent >= 0 && b.parent != static_cast<int>(i)) ? b.parent : -1;
            for (int k = 0; k < 3; ++k) j.pos[k] = b.pos[k];
            for (int k = 0; k < 4; ++k) j.quat[k] = b.quat[k];
            j.scale[0] = b.scale_shear[0]; j.scale[1] = b.scale_shear[4]; j.scale[2] = b.scale_shear[8];
            for (int k = 0; k < 16; ++k) j.inv_world[k] = b.inv_world[k];
            out.push_back(std::move(j));
        }
        if (name) *name = s.name;
        return true;
    }
    return false;
}

void raw_clips(const Gr2File& f, const std::vector<RawJoint>& joints, const std::string& clip_name, std::vector<RawClip>& out) {
    std::unordered_map<std::string, int> bones;
    for (const RawJoint& j : joints) bones[j.name] = 1;
    std::unordered_map<std::string, const Gr2TrackGroup*> groups;
    for (const Gr2TrackGroup& g : f.track_groups) groups[g.name] = &g;          // como o dict do Python: o ultimo vence
    for (const Gr2Animation& an : f.animations) {
        RawClip c;
        if (!clip_name.empty()) c.name = clip_name;
        else {
            std::string n = an.name;
            for (char& ch : n) if (ch == '\\') ch = '/';
            const std::size_t sl = n.rfind('/');
            if (sl != std::string::npos) n = n.substr(sl + 1);
            const std::size_t dot = n.rfind('.');
            if (dot != std::string::npos) n = n.substr(0, dot);
            c.name = n;
        }
        c.duration = an.duration;
        const double step = an.time_step > 0 ? static_cast<double>(an.time_step) : 1.0 / 30.0;
        std::vector<std::string> names;
        for (const std::string& n : an.track_groups) if (!ignored_group(n)) names.push_back(n);
        if (names.empty()) for (const Gr2TrackGroup& g : f.track_groups) if (!ignored_group(g.name)) names.push_back(g.name);
        std::unordered_map<std::string, std::size_t> pos;
        for (const std::string& gn : names) {
            const auto git = groups.find(gn);
            if (git == groups.end()) continue;
            for (const Gr2TransformTrack& tr : git->second->tracks) {
                if (!bones.count(tr.name)) continue;
                RawTrack t;
                t.bone = tr.name;
                t.has_r = curve_keys(tr.orientation, c.duration, step, 4, t.tr, t.vr);
                t.has_t = curve_keys(tr.position, c.duration, step, 3, t.tt, t.vt);
                if (tr.scale_shear.present) {
                    const int dim = tr.scale_shear.dim;
                    std::vector<double> tt, vv;
                    const int want = (dim == 3 || dim == 9) ? dim : 3;
                    if (curve_keys(tr.scale_shear, c.duration, step, want, tt, vv)) {
                        t.has_s = true;
                        t.ts = tt;
                        const std::size_t nk = tt.size();
                        for (std::size_t i = 0; i < nk; ++i) {
                            if (want == 9) { t.vs.push_back(vv[i * 9]); t.vs.push_back(vv[i * 9 + 4]); t.vs.push_back(vv[i * 9 + 8]); }
                            else for (int k = 0; k < 3; ++k) t.vs.push_back(vv[i * 3 + static_cast<std::size_t>(k)]);
                        }
                    }
                }
                if (!t.has_r && !t.has_t && !t.has_s) continue;
                const auto it = pos.find(t.bone);
                if (it == pos.end()) { pos[t.bone] = c.tracks.size(); c.tracks.push_back(std::move(t)); }
                else c.tracks[it->second] = std::move(t);                      // dict do Python: mantem a posicao, troca o valor
            }
        }
        out.push_back(std::move(c));
    }
}

F3MFile build_model(const B3dModel& model, const std::string& name, const std::vector<RawJoint>& joints_in, const std::vector<RawClip>& clips_in,
                    const TextureLoader& tex_loader, ModelBuildReport* rep, double mesh_scale) {
    ModelBuildReport local;
    ModelBuildReport& R = rep ? *rep : local;
    std::vector<RawJoint> joints = joints_in;
    std::vector<RawClip> clips = clips_in;
    // export.py: apply_initial_placement
    double place[3];
    if (!joints.empty() && detect_initial_placement(joints, place)) {
        R.placement_applied = true;
        for (int k = 0; k < 3; ++k) R.placement[k] = place[k];
        std::unordered_map<std::string, int> roots;
        for (RawJoint& j : joints) if (j.parent < 0) { for (int k = 0; k < 3; ++k) j.pos[k] = j.pos[k] + place[k]; roots[j.name] = 1; }
        for (RawClip& c : clips)
            for (RawTrack& t : c.tracks)
                if (roots.count(t.bone) && t.has_t)
                    for (std::size_t i = 0; i + 2 < t.vt.size(); i += 3) for (int k = 0; k < 3; ++k) t.vt[i + static_cast<std::size_t>(k)] = t.vt[i + static_cast<std::size_t>(k)] + place[k];
    }
    std::vector<std::string> bone_names;
    for (const B3dBone& b : model.bones) bone_names.push_back(b.name);
    bone_names = resolve_bone_aliases(bone_names, joints, R.aliased_bones);
    if (R.aliased_bones) R.warnings.push_back(name + ": " + std::to_string(R.aliased_bones) + " ossos casados por sufixo");

    F3MFile f;
    f.name = name;
    std::map<std::string, int> tex_idx;
    auto texture = [&](const std::string& file) -> int {
        const std::string key = lower(file);
        const auto it = tex_idx.find(key);
        if (it != tex_idx.end()) return it->second;
        const std::size_t dot = file.rfind('.');
        const std::string stem = dot == std::string::npos ? file : file.substr(0, dot);
        int idx = -1;
        std::vector<std::uint8_t> raw = tex_loader ? tex_loader(stem) : std::vector<std::uint8_t>();
        if (raw.empty()) R.warnings.push_back("textura '" + file + "' nao encontrada");
        else {
            Image im; std::string e;
            if (decode_image(raw.data(), raw.size(), im, e)) {
                Texture t;
                t.name = stem; t.w = im.w; t.h = im.h; t.rgba = std::move(im.rgba);
                f.textures.push_back(std::move(t));
                idx = static_cast<int>(f.textures.size()) - 1;
            } else R.warnings.push_back("textura '" + file + "': " + e);
        }
        tex_idx[key] = idx;
        return idx;
    };
    std::map<std::string, int> mat_idx;
    auto material = [&](const B3dMaterial& m) -> int {
        char key_floats[128] = {0};
        float f4[4] = {1, 1, 1, 1};
        const bool has4 = m.floats.size() >= 4;
        if (has4) for (int k = 0; k < 4; ++k) f4[k] = m.floats[static_cast<std::size_t>(k)];
        std::snprintf(key_floats, sizeof key_floats, "%a|%a|%a|%a|%d", static_cast<double>(f4[0]), static_cast<double>(f4[1]), static_cast<double>(f4[2]), static_cast<double>(f4[3]), has4 ? 1 : 0);
        const std::string key = m.name + "\x01" + m.blend + "\x01" + (m.textures.empty() ? std::string("\x02") : m.textures[0]) + "\x01" + key_floats;
        const auto it = mat_idx.find(key);
        if (it != mat_idx.end()) return it->second;
        Material out;
        out.name = m.name.empty() ? "material" : m.name;
        out.texture = m.textures.empty() ? -1 : texture(m.textures[0]);
        for (int k = 0; k < 4; ++k) out.color[k] = f4[k];
        out.alpha_mode = m.blend == "ALPHABLEND" ? 1 : (m.blend == "ADD" ? 2 : 0);
        out.double_sided = 0;
        out.shader = m.id;
        f.materials.push_back(out);
        const int i = static_cast<int>(f.materials.size()) - 1;
        mat_idx[key] = i;
        return i;
    };

    const double k = mesh_scale;
    model.each_surface([&](const B3dNode& node, const B3dSurface& s) {
        const std::uint32_t nv = s.nv();
        std::vector<double> pos(static_cast<std::size_t>(nv) * 3), nor(static_cast<std::size_t>(nv) * 3);
        for (std::uint32_t i = 0; i < nv; ++i) {
            const std::size_t o = static_cast<std::size_t>(i) * 3;
            pos[o] = static_cast<double>(s.pos[o]) * k; pos[o + 1] = static_cast<double>(s.pos[o + 1]) * k; pos[o + 2] = -static_cast<double>(s.pos[o + 2]) * k;
            nor[o] = s.nrm[o]; nor[o + 1] = s.nrm[o + 1]; nor[o + 2] = -static_cast<double>(s.nrm[o + 2]);
        }
        const bool skinned = !s.influences.empty();
        std::vector<std::uint16_t> jj;
        std::vector<float> ww;
        if (skinned) {
            jj.resize(static_cast<std::size_t>(nv) * 4, 0);
            ww.resize(static_cast<std::size_t>(nv) * 4, 0.f);
            int missing = 0;
            for (std::uint32_t v = 0; v < nv; ++v) {
                std::vector<int> js; std::vector<double> ws;
                for (const auto& inf : s.influences[v]) {
                    if (inf.first < 0 || static_cast<std::size_t>(inf.first) >= bone_names.size()) { ++missing; continue; }
                    js.push_back(inf.first); ws.push_back(inf.second);
                }
                while (js.size() < 4) { js.push_back(0); ws.push_back(0.0); }
                js.resize(4); ws.resize(4);
                double tot = 0;
                for (double x : ws) tot += x;
                for (int q = 0; q < 4; ++q) {
                    jj[static_cast<std::size_t>(v) * 4 + static_cast<std::size_t>(q)] = static_cast<std::uint16_t>(js[static_cast<std::size_t>(q)]);
                    ww[static_cast<std::size_t>(v) * 4 + static_cast<std::size_t>(q)] = tot > 0 ? static_cast<float>(ws[static_cast<std::size_t>(q)] / tot) : (q == 0 ? 1.f : 0.f);
                }
            }
            if (missing) R.warnings.push_back((node.name.empty() ? name : node.name) + ": " + std::to_string(missing) + " influencias de osso sem correspondencia");
        }
        for (const B3dSurface::Prim& pr : s.prims) {
            if (pr.idx.empty()) continue;
            Mesh m;
            m.name = node.name.empty() ? name : node.name;
            m.material = (pr.material >= 0 && static_cast<std::size_t>(pr.material) < s.materials.size()) ? material(s.materials[static_cast<std::size_t>(pr.material)]) : -1;
            m.pos.resize(pos.size()); m.nrm.resize(nor.size());
            for (std::size_t i = 0; i < pos.size(); ++i) { m.pos[i] = static_cast<float>(pos[i]); m.nrm[i] = static_cast<float>(nor[i]); }
            m.uv = s.uv0;
            m.col = s.col.empty() ? std::vector<float>(static_cast<std::size_t>(nv) * 4, 1.f) : s.col;
            if (skinned) { m.has_skin = true; m.bones = bone_names; m.joints = jj; m.weights = ww; }
            m.idx = pr.idx;
            fix_winding(m.idx, pos, nor);
            f.meshes.push_back(std::move(m));
        }
    });

    for (const RawJoint& j : joints) {
        Joint o;
        o.name = j.name; o.parent = j.parent;
        cv_p(j.pos, o.t); cv_q(j.quat, o.q);
        for (int q = 0; q < 3; ++q) o.s[q] = static_cast<float>(j.scale[q]);
        cv_m4(j.inv_world, o.ibm);
        f.joints.push_back(o);
    }
    for (const RawClip& c : clips) {
        Clip o;
        o.name = c.name;
        o.duration = static_cast<float>(c.duration);
        for (const RawTrack& t : c.tracks) {
            if (t.has_t) {
                Channel ch; ch.bone = t.bone; ch.path = 0;
                for (double x : t.tt) ch.times.push_back(static_cast<float>(x));
                for (std::size_t i = 0; i + 2 < t.vt.size(); i += 3) { float v[3]; cv_p(&t.vt[i], v); ch.values.insert(ch.values.end(), v, v + 3); }
                o.channels.push_back(std::move(ch));
            }
            if (t.has_r) {
                Channel ch; ch.bone = t.bone; ch.path = 1;
                for (double x : t.tr) ch.times.push_back(static_cast<float>(x));
                for (std::size_t i = 0; i + 3 < t.vr.size(); i += 4) { float v[4]; cv_q(&t.vr[i], v); ch.values.insert(ch.values.end(), v, v + 4); }
                o.channels.push_back(std::move(ch));
            }
            if (t.has_s) {
                Channel ch; ch.bone = t.bone; ch.path = 2;
                for (double x : t.ts) ch.times.push_back(static_cast<float>(x));
                for (double x : t.vs) ch.values.push_back(static_cast<float>(x));
                o.channels.push_back(std::move(ch));
            }
        }
        if (!o.channels.empty()) f.clips.push_back(std::move(o));
    }
    f.flags = (f.meshes.empty() ? 0u : kFlagModel) | (f.joints.empty() ? 0u : kFlagSkeleton) | (f.clips.empty() ? 0u : kFlagClips);
    return f;
}

double guess_mesh_scale(const B3dModel& model, const std::vector<RawJoint>& joints) {
    if (joints.size() < 2) return kMeshScale;
    std::vector<M4> world(joints.size());
    double jmn[3] = {1e30, 1e30, 1e30}, jmx[3] = {-1e30, -1e30, -1e30};
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const M4 loc = joint_local(joints[i]);
        const int p = joints[i].parent;
        world[i] = (p < 0 || static_cast<std::size_t>(p) >= i) ? loc : mul(world[static_cast<std::size_t>(p)], loc);
        for (int k = 0; k < 3; ++k) { jmn[k] = std::min(jmn[k], world[i][k][3]); jmx[k] = std::max(jmx[k], world[i][k][3]); }
    }
    double mmn[3] = {1e30, 1e30, 1e30}, mmx[3] = {-1e30, -1e30, -1e30};
    bool any = false;
    model.each_surface([&](const B3dNode&, const B3dSurface& s) {
        for (std::size_t i = 0; i + 2 < s.pos.size(); i += 3) { any = true; for (int k = 0; k < 3; ++k) { mmn[k] = std::min(mmn[k], static_cast<double>(s.pos[i + static_cast<std::size_t>(k)])); mmx[k] = std::max(mmx[k], static_cast<double>(s.pos[i + static_cast<std::size_t>(k)])); } }
    });
    if (!any) return kMeshScale;
    auto diag = [](const double* a, const double* b) { double s = 0; for (int k = 0; k < 3; ++k) s += (b[k] - a[k]) * (b[k] - a[k]); return std::sqrt(s); };
    const double jd = diag(jmn, jmx), md = diag(mmn, mmx);
    if (jd < 1e-6 || md < 1e-6) return kMeshScale;
    const double e_small = std::fabs(std::log(md * kMeshScale / jd)), e_one = std::fabs(std::log(md / jd));
    return e_one < e_small ? 1.0 : static_cast<double>(kMeshScale);
}

std::vector<std::uint8_t> save_f3m(const F3MFile& f) {
    std::vector<std::uint8_t> o;
    auto u8 = [&](std::uint8_t v) { o.push_back(v); };
    auto u16 = [&](std::uint16_t v) { o.push_back(static_cast<std::uint8_t>(v)); o.push_back(static_cast<std::uint8_t>(v >> 8)); };
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) o.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto f32 = [&](float v) { std::uint32_t u; std::memcpy(&u, &v, 4); u32(u); };
    auto str = [&](const std::string& s) { const std::size_t n = std::min<std::size_t>(s.size(), 65535); u16(static_cast<std::uint16_t>(n)); o.insert(o.end(), s.begin(), s.begin() + static_cast<long>(n)); };
    o.insert(o.end(), {'F', '3', 'M', '1'});
    u32(f.flags);
    u32(static_cast<std::uint32_t>(f.textures.size())); u32(static_cast<std::uint32_t>(f.materials.size())); u32(static_cast<std::uint32_t>(f.meshes.size()));
    u32(static_cast<std::uint32_t>(f.joints.size())); u32(static_cast<std::uint32_t>(f.clips.size()));
    str(f.name);
    for (const Texture& t : f.textures) { str(t.name); u32(t.w); u32(t.h); o.insert(o.end(), t.rgba.begin(), t.rgba.end()); }
    for (const Material& m : f.materials) { str(m.name); u32(static_cast<std::uint32_t>(m.texture)); for (float c : m.color) f32(c); u8(m.alpha_mode); u8(m.double_sided); u8(0); u8(0); }
    for (const Mesh& m : f.meshes) {
        str(m.name); u32(static_cast<std::uint32_t>(m.material)); u32(m.nv()); u32(static_cast<std::uint32_t>(m.idx.size()));
        u32(static_cast<std::uint32_t>(m.has_skin ? m.bones.size() : 0));
        if (m.has_skin) for (const std::string& b : m.bones) str(b);
        u8(m.has_skin ? 1 : 0); u8(0); u8(0); u8(0);
        for (float x : m.pos) f32(x);
        for (float x : m.nrm) f32(x);
        for (float x : m.uv) f32(x);
        for (float x : m.col) f32(x);
        if (m.has_skin) { for (std::uint16_t x : m.joints) u16(x); for (float x : m.weights) f32(x); }
        for (std::uint32_t x : m.idx) u32(x);
    }
    for (const Joint& j : f.joints) {
        str(j.name); u32(static_cast<std::uint32_t>(j.parent));
        for (float x : j.t) f32(x);
        for (float x : j.q) f32(x);
        for (float x : j.s) f32(x);
        for (float x : j.ibm) f32(x);
    }
    for (const Clip& c : f.clips) {
        str(c.name); f32(c.duration); u32(static_cast<std::uint32_t>(c.channels.size()));
        for (const Channel& ch : c.channels) {
            str(ch.bone); u8(ch.path); u8(0); u8(0); u8(0);
            u32(static_cast<std::uint32_t>(ch.times.size()));
            for (float x : ch.times) f32(x);
            for (float x : ch.values) f32(x);
        }
    }
    return o;
}

}  // namespace f3v
