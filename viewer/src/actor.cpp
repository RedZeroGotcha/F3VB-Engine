// v20: atores (ver actor.h).
#include "actor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

#include "b3d.h"
#include "gr2.h"
#include "model_build.h"
#include "proto.h"
#include "vfs.h"

namespace f3v {

namespace {

std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; }
std::string stem(const std::string& f) { const std::size_t d = f.rfind('.'); return d == std::string::npos ? f : f.substr(0, d); }
std::string upper_ext(const std::string& f) { const std::size_t d = f.rfind('.'); std::string e = d == std::string::npos ? "" : f.substr(d + 1); for (char& c : e) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A'); return e; }

std::uint32_t proto_type(const std::string& kind) {
    if (kind == "CRT") return RT_CRT;
    if (kind == "ARM") return RT_ARM;
    if (kind == "WEA") return RT_WEA;
    if (kind == "USE") return RT_USE;
    if (kind == "DOR") return RT_DOR;
    if (kind == "AMO") return RT_AMO;
    if (kind == "CON") return RT_CON;
    return 0;
}

bool read_proto(const Vfs& vfs, const std::string& file, Prototype& p) {
    const std::string kind = upper_ext(file);
    std::vector<std::uint8_t> d;
    const std::uint32_t t = proto_type(kind);
    // o mapa cita "Critters00_TMCpl_Armstrong.CRT"; o .rht guarda "Critters00_TMCpl_Armstrong" com tipo CRT
    if (!vfs.read(stem(file), t, d) && !vfs.read_file(file, d)) return false;
    return parse_prototype(d.data(), d.size(), kind.empty() ? "?" : kind, p);
}

bool read_gr2_res(const Vfs& vfs, const std::string& name, std::uint32_t type, Gr2File& g) {
    std::vector<std::uint8_t> d;
    if (!vfs.read(name, type, d)) return false;
    std::string err;
    return load_gr2(d.data(), d.size(), g, err);
}

bool read_b3d_res(const Vfs& vfs, const std::string& name, B3dModel& m, std::string& err) {
    std::vector<std::uint8_t> d;
    if (!vfs.read(name, RT_MODEL, d)) { err = "modelo '" + name + "' nao existe na demo"; return false; }
    return load_b3d(d.data(), d.size(), m, err);
}

// regioes do corpo base cobertas por cada encaixe de parte (hipotese conferida com os prints do menu original)
const std::set<std::string>& covered_by(const std::string& slot) {
    static const std::map<std::string, std::set<std::string>> k = {
        {"Bod", {"chest", "waist", "upperarms"}},
        {"Fee", {"legs", "shins", "feet"}},
        {"Han", {"hands", "forearms"}},
    };
    static const std::set<std::string> none;
    const auto it = k.find(slot);
    return it == k.end() ? none : it->second;
}

// Os humanos da demo usam um ATLAS montado pela engine (512x512 com as texturas _LG de 256): pele no quadrante superior
// esquerdo, roupa do corpo (Bod) no inferior esquerdo e as partes pequenas (pes, maos, ombros, cabeca, costas, cabelo, barba,
// Pip-Boy) em celulas de 1/4 (medido nos B3D: HumMNor, HumMStr_*, HumFWir). Aqui desfazemos o atlas: a UV de cada malha volta
// para a sua propria textura. Os modelos que nao sao humanos (CR_Dog...) usam a textura inteira e ficam como estao.
void unatlas_human(Scene& sc) {
    for (Mesh& m : sc.meshes) {
        if (m.uv.empty() || m.material < 0 || static_cast<std::size_t>(m.material) >= sc.materials.size()) continue;
        const Material& mat = sc.materials[static_cast<std::size_t>(m.material)];
        std::string tex = mat.texture >= 0 && static_cast<std::size_t>(mat.texture) < sc.textures.size() ? lower(sc.textures[static_cast<std::size_t>(mat.texture)].name) : std::string();
        float umin = 1e9f, umax = -1e9f, vmin = 1e9f, vmax = -1e9f;
        for (std::uint32_t i : m.idx) { const float u = m.uv[2 * i], v = m.uv[2 * i + 1]; umin = std::min(umin, u); umax = std::max(umax, u); vmin = std::min(vmin, v); vmax = std::max(vmax, v); }
        if (umin > umax) continue;
        float x0, y0, size;
        if (tex.compare(0, 5, "skin_") == 0) { x0 = 0.f; y0 = 0.f; size = 0.5f; }
        else if (tex.compare(0, 3, "bod") == 0) { x0 = 0.f; y0 = 0.5f; size = 0.5f; }
        else { size = 0.25f; x0 = std::floor(((umin + umax) * 0.5f) / size) * size; y0 = std::floor(((vmin + vmax) * 0.5f) / size) * size; }
        for (std::size_t i = 0; i + 1 < m.uv.size(); i += 2) { m.uv[i] = (m.uv[i] - x0) / size; m.uv[i + 1] = (m.uv[i + 1] - y0) / size; }
    }
}

}  // namespace

const char* actor_anim_name(int a) {
    static const char* n[AN_COUNT] = {"parado", "andar", "correr", "ataque", "dano", "morte"};
    return a >= 0 && a < AN_COUNT ? n[a] : "?";
}

void Actor::play(int anim, float now, bool loop) {
    if (anim < 0 || anim >= AN_COUNT) return;
    if (anim == cur_anim && loop && anim_loop) return;
    cur_anim = anim; anim_start = now; anim_loop = loop;
}

float Actor::anim_length(int anim) const {
    if (anim < 0 || anim >= AN_COUNT || clip_index[anim] < 0 || parts.empty()) return 0;
    const Scene& sc = parts[0].scene;
    const std::size_t ci = static_cast<std::size_t>(clip_index[anim]);
    return ci < sc.clips.size() ? sc.clips[ci].duration : 0;
}

bool Actor::anim_done(float now) const { return !anim_loop && now - anim_start >= anim_length(cur_anim); }

void Actor::animate(float t) {
    // v20: a animacao repete. O Scene::evaluate segura a ULTIMA pose depois do fim do clipe (6,67 s no Idle dos humanos), e o relogio
    // do jogo ja passa disso quando o menu aparece: por isso os personagens ficavam congelados. v21: animacao por papel (andar, ataque...);
    // as sem laco (ataque, dano, morte) param no ultimo quadro.
    int role = cur_anim >= 0 && cur_anim < AN_COUNT && clip_index[cur_anim] >= 0 ? cur_anim : AN_IDLE;
    const int ci = clip_index[role];
    const bool idle = role == AN_IDLE;
    const float local = idle ? t + phase : t - anim_start;
    const bool loop = idle || anim_loop;
    for (ActorPart& p : parts) {
        if (p.scene.clips.empty() || ci < 0 || static_cast<std::size_t>(ci) >= p.scene.clips.size()) { p.scene.evaluate(-1, 0); continue; }
        const float d = p.scene.clips[static_cast<std::size_t>(ci)].duration;
        float tt = local;
        if (d > 1e-4f) {
            if (loop) { tt = std::fmod(tt, d); if (tt < 0) tt += d; }
            else tt = std::clamp(tt, 0.f, d - 1e-4f);
        } else tt = 0;
        p.scene.evaluate(ci, tt);
    }
    if (has_weapon) weapon.scene.evaluate(-1, 0);
}

void Actor::model_matrix(float o[16]) const {
    const float c = std::cos(yaw), s = std::sin(yaw), k = scale;
    // colunas: X, Y, Z (com o espelho do Z do F3M de volta para o Z do jogo), translacao
    o[0] = c * k;  o[1] = 0; o[2] = -s * k; o[3] = 0;
    o[4] = 0;      o[5] = k; o[6] = 0;      o[7] = 0;
    o[8] = -s * k; o[9] = 0; o[10] = -c * k; o[11] = 0;
    o[12] = pos[0]; o[13] = pos[1]; o[14] = pos[2]; o[15] = 1;
}

void Actor::weapon_matrix(float o[16]) const {
    Mat4 a;
    model_matrix(a.m);
    if (hand_joint >= 0 && !parts.empty() && static_cast<std::size_t>(hand_joint) < parts[0].scene.joint_world.size()) a = mul(a, parts[0].scene.joint_world[static_cast<std::size_t>(hand_joint)]);
    std::memcpy(o, a.m, sizeof a.m);
}

bool build_actor(const Vfs& vfs, const MapEntity& e, Actor& out, std::string& err, const ActorBuildOptions& opt) {
    out = Actor();
    out.prototype = e.prototype;
    out.pos[0] = e.pos[0]; out.pos[1] = e.pos[1]; out.pos[2] = e.pos[2];
    out.yaw = e.yaw;
    auto note = [&](const std::string& s) { if (opt.log) opt.log->push_back(s); };
    Prototype P;
    if (!read_proto(vfs, e.prototype, P)) { err = "prototipo '" + e.prototype + "' nao encontrado"; return false; }
    if (P.model.empty()) { err = "prototipo sem modelo"; return false; }
    const bool human = P.kind == "CRT" && P.model.size() >= 4 && P.model.compare(0, 3, "Hum") == 0;
    const ActorLook* look = human && opt.look && !opt.look->empty() ? opt.look : nullptr;
    if (look && P.model.size() >= 7) {                                       // HumMNor -> HumF/HumM + Nor/Fat/Str/Wir
        std::string m = P.model;
        if (look->set_sex) m[3] = look->female ? 'F' : 'M';
        if (look->body.size() == 3) m.replace(4, 3, look->body);
        if (m != P.model) {
            if (vfs.find(m, RT_MODEL)) P.model = m; else note("corpo '" + m + "' nao existe na demo");
        }
        if (!look->skin.empty()) P.skin = "Skin_Hum" + std::string(1, P.model[3]) + "_" + look->skin + "_LG";
    }
    out.model = P.model;

    // partes (humano): as do prototipo, trocadas pelas da armadura no mesmo encaixe
    std::vector<std::string> parts = human ? P.parts : std::vector<std::string>();
    std::map<std::string, std::string> part_tex = P.part_texture;
    if (human && !P.armor.empty()) {
        Prototype A;
        if (read_proto(vfs, P.armor, A)) {
            std::set<std::string> slots;
            for (const std::string& p : A.parts) slots.insert(part_slot(p));
            parts.erase(std::remove_if(parts.begin(), parts.end(), [&](const std::string& p) { return slots.count(part_slot(p)) > 0; }), parts.end());
            for (const std::string& p : A.parts) parts.push_back(p);
            for (const auto& kv : A.part_texture) part_tex[kv.first] = kv.second;
        } else note("armadura '" + P.armor + "' nao encontrada");
    }
    if (look) {
        for (const auto& kv : look->slots) {                                   // troca a parte de cada encaixe escolhido
            parts.erase(std::remove_if(parts.begin(), parts.end(), [&](const std::string& p) { return part_slot(p) == kv.first; }), parts.end());
            if (!kv.second.empty()) parts.push_back(kv.second);
        }
        if (!look->hair_color.empty())                                        // cor do cabelo: textura <Parte>_<cor>_LG das partes de pelo
            for (const std::string& p : parts) {
                const std::string sl = part_slot(p);
                if (sl == "Hai" || sl == "Pon" || sl == "Mus" || sl == "Bea") part_tex[p] = p + "_" + look->hair_color + "_LG";
            }
    }
    // arma na mao
    std::string held, weapon_class = "Una";
    if (P.kind == "CRT" && !P.weapon.empty() && opt.with_weapon) {
        Prototype W;
        if (read_proto(vfs, P.weapon, W) && !W.held_model.empty()) {
            held = W.held_model;
            const std::string l = lower(held);
            weapon_class = (l.find("minigun") != std::string::npos || l.find("rocket") != std::string::npos || l.find("flame") != std::string::npos ||
                            l.find("gatling") != std::string::npos || l.find("bozar") != std::string::npos) ? "2HH" : "1H";
        }
    }

    // esqueleto do modelo base
    std::vector<RawJoint> joints;
    {
        Gr2File g;
        if (read_gr2_res(vfs, P.model, RT_SKEL, g)) raw_skeleton(g, joints);
    }
    // animacao parada
    std::vector<RawClip> clips;
    if (!joints.empty()) {
        std::vector<std::string> cands;
        if (human) {
            const std::string fam = P.model.substr(0, 4) + "_";
            for (const char* st : {"Def_", "Com_"}) cands.push_back(fam + st + weapon_class + "_Idle_01");
            cands.push_back(fam + "Def_Una_Idle_01");
            cands.push_back(fam + "Com_Una_Idle_01");
        } else {
            cands.push_back(P.model + "_Com_Una_Idle_01");
            cands.push_back(P.model + "_Def_Una_Idle_01");
        }
        for (const std::string& c : cands) {
            Gr2File g;
            if (!read_gr2_res(vfs, c, RT_GR2, g)) continue;
            raw_clips(g, joints, c, clips);
            if (!clips.empty()) { out.clip_name = c; break; }
        }
        if (clips.empty()) {                                   // qualquer "<modelo>_*Idle*"
            const std::string pre = lower(P.model + "_");
            for (const ResEntry* r : vfs.list(RT_GR2)) {
                const std::string n = lower(r->name);
                if (n.compare(0, pre.size(), pre) == 0 && n.find("idle") != std::string::npos) {
                    Gr2File g;
                    if (read_gr2_res(vfs, r->name, RT_GR2, g)) { raw_clips(g, joints, r->name, clips); if (!clips.empty()) { out.clip_name = r->name; break; } }
                }
            }
        }
    }

    // v21: os outros papeis (andar, correr, ataque, dano, morte), cada um com candidatos em ordem; o 1o que existir na demo vale
    if (!clips.empty()) {
        out.clip_index[AN_IDLE] = 0;
        out.clip_names[AN_IDLE] = out.clip_name;
        const std::string hl = lower(held);
        out.weapon_class = weapon_class;
        out.melee = held.empty() || hl.find("knife") != std::string::npos || hl.find("club") != std::string::npos || hl.find("sledge") != std::string::npos ||
                    hl.find("spear") != std::string::npos || hl.find("sword") != std::string::npos || hl.find("fist") != std::string::npos ||
                    hl.find("bat") != std::string::npos || hl.find("knuckle") != std::string::npos;
        std::vector<std::string> c[AN_COUNT];
        if (human) {
            const std::string f = P.model.substr(0, 4) + "_", w = weapon_class;
            c[AN_WALK] = {f + "Com_" + w + "_Walk_01", f + "Com_Una_Walk_01"};
            c[AN_RUN] = {f + "Com_Una_Run_01", f + "Com_" + w + "_Walk_01"};
            if (w == "Una") c[AN_ATTACK] = {f + "Com_Una_Punch_Jab_01", f + "Com_Una_Kick_Snap_01"};
            else if (w == "1H") c[AN_ATTACK] = out.melee ? std::vector<std::string>{f + "Com_1H_Melee_Slash_01", f + "Com_1H_Melee_Thrust_01"}
                                                         : std::vector<std::string>{f + "Com_1H_Gun_Shoot_01", f + "Com_1H_Gun_NoRecoil_01"};
            else c[AN_ATTACK] = {f + "Com_2HH_Gun_Burst_01", f + "Com_2HH_Gun_Shoot_01"};
            c[AN_HIT] = {f + "Com_" + w + "_DmgFr_01", f + "Com_Una_DmgFr_01"};
            c[AN_DEATH] = {f + "ProneB_Una_Death_01", f + "ProneF_Una_Death_01"};
        } else {
            const std::string m = P.model + "_";
            c[AN_WALK] = {m + "Com_Una_Walk_01", m + "Def_Una_Walk_01"};
            c[AN_RUN] = {m + "Com_Una_Run_01", m + "Com_Una_Walk_02", m + "Com_Una_Walk_01"};
            c[AN_ATTACK] = {m + "Com_Una_Attack1_01", m + "Com_Una_Attack_01", m + "Com_Una_Bite_01"};
            c[AN_HIT] = {m + "Com_Una_DmgFr_01"};
            c[AN_DEATH] = {m + "ProneB_Una_Death_01", m + "ProneF_Una_Death_01", m + "Com_Una_Death_01"};
        }
        for (int role = AN_WALK; role < AN_COUNT; ++role)
            for (const std::string& cand : c[role]) {
                Gr2File g;
                if (!read_gr2_res(vfs, cand, RT_GR2, g)) continue;
                const std::size_t before = clips.size();
                raw_clips(g, joints, cand, clips);
                if (clips.size() > before) { clips.resize(before + 1); out.clip_index[role] = static_cast<int>(before); out.clip_names[role] = cand; break; }
            }
    }

    // carregador de texturas com a troca de pele e das texturas proprias das partes
    const std::string skin = stem(P.skin);
    auto tex_loader = [&](const std::string& st) -> std::vector<std::uint8_t> {
        std::string want = st;
        const std::string l = lower(st);
        if (!skin.empty() && l.compare(0, 8, "skin_hum") == 0) want = skin;
        for (const auto& kv : part_tex) if (l.compare(0, kv.first.size() + 9, lower(kv.first) + "_default_") == 0) want = kv.second;
        std::vector<std::uint8_t> d;
        if (!vfs.read(want, RT_TGA, d) && want != st) vfs.read(st, RT_TGA, d);
        return d;
    };

    // corpo/modelo base
    B3dModel base;
    if (!read_b3d_res(vfs, P.model, base, err)) return false;
    const double ms = guess_mesh_scale(base, joints);
    ModelBuildReport rep;
    {
        ActorPart bp;
        bp.name = P.model;
        bp.scene.add(build_model(base, P.model, joints, clips, tex_loader, &rep, ms));
        if (human) unatlas_human(bp.scene);
        bp.hidden.assign(bp.scene.meshes.size(), 0);
        out.parts.push_back(std::move(bp));
    }
    // partes
    std::set<std::string> covered;
    for (const std::string& part : parts) {
        const std::string slot = part_slot(part);
        std::string mname = P.model + "_" + part;
        B3dModel pm;
        std::string e2;
        if (!read_b3d_res(vfs, mname, pm, e2)) {
            const std::string alt = P.model.substr(0, 4) + "Nor_" + part;          // ex.: HumMStr_X sem modelo -> HumMNor_X
            if (alt == mname || !read_b3d_res(vfs, alt, pm, e2)) { note("parte '" + mname + "' nao existe na demo"); continue; }
            mname = alt;
        }
        ActorPart ap;
        ap.name = mname;
        ap.scene.add(build_model(pm, mname, joints, clips, tex_loader, nullptr, ms));
        unatlas_human(ap.scene);
        ap.hidden.assign(ap.scene.meshes.size(), 0);
        for (const std::string& c : covered_by(slot)) covered.insert(c);
        for (const Mesh& m : ap.scene.meshes)                                       // a parte traz a propria pele desta regiao
            if (m.material >= 0 && static_cast<std::size_t>(m.material) < ap.scene.materials.size()) covered.insert(lower(ap.scene.materials[static_cast<std::size_t>(m.material)].name));
        out.parts.push_back(std::move(ap));
    }
    {
        ActorPart& bp = out.parts[0];
        for (std::size_t i = 0; i < bp.scene.meshes.size(); ++i) {
            const int mi = bp.scene.meshes[i].material;
            if (mi >= 0 && static_cast<std::size_t>(mi) < bp.scene.materials.size() && covered.count(lower(bp.scene.materials[static_cast<std::size_t>(mi)].name))) bp.hidden[i] = 1;
        }
    }
    // arma
    if (!held.empty()) {
        B3dModel wm;
        std::string e3;
        if (read_b3d_res(vfs, held, wm, e3)) {
            std::vector<RawJoint> wj;
            Gr2File g;
            if (read_gr2_res(vfs, held, RT_SKEL, g)) raw_skeleton(g, wj);
            out.weapon.name = held;
            out.weapon.scene.add(build_model(wm, held, wj, {}, tex_loader, nullptr, guess_mesh_scale(wm, wj)));
            out.weapon.hidden.assign(out.weapon.scene.meshes.size(), 0);
            out.hand_joint = out.parts[0].scene.find_joint("R Handle");
            out.has_weapon = out.hand_joint >= 0;
            if (!out.has_weapon) note("sem osso 'R Handle' para a arma");
        } else note("arma '" + held + "': " + e3);
    }
    // altura (pose de bind)
    out.animate(0);
    float mn[3], mx[3];
    if (out.parts[0].scene.bounds(mn, mx)) out.height = (mx[1] - mn[1]) * out.scale;
    // fase diferente por entidade (nao ficam todos sincronizados)
    out.phase = std::fmod(std::fabs(e.pos[0] * 0.37f + e.pos[2] * 0.73f), 7.f);
    out.script = !e.script.empty() ? e.script : P.script;   // o script da instancia no mapa vale mais que o do prototipo
    out.description = P.kind + " " + P.model + " (" + std::to_string(out.parts.size()) + " parte(s)" + (out.has_weapon ? ", arma " + held : std::string()) +
                      (out.clip_name.empty() ? ", sem animacao" : ", anim " + out.clip_name) + (ms == 1.0 ? ", malha em escala 1" : "") + ")";
    return true;
}

}  // namespace f3v
