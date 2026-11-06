// v20: leitor de .b3d em C++ (ver b3d.h). Mesma logica do tools/py/f3vb/b3d.py, mais a variante "64 com pesos".
#include "b3d.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace f3v {

namespace {

struct R {
    const std::uint8_t* d; std::size_t n; std::size_t p = 0;
    std::string* err;
    bool bad = false;
    bool need(std::size_t k) { if (bad) return false; if (p + k > n) { bad = true; if (err && err->empty()) *err = "fim do arquivo em " + std::to_string(p) + " (precisava de " + std::to_string(k) + " bytes)"; return false; } return true; }
    void fail(const std::string& m) { if (!bad) { bad = true; if (err && err->empty()) *err = m; } }
    std::uint8_t u8() { if (!need(1)) return 0; return d[p++]; }
    std::uint16_t u16() { if (!need(2)) return 0; const std::uint16_t v = static_cast<std::uint16_t>(d[p] | (d[p + 1] << 8)); p += 2; return v; }
    std::uint32_t u32() { if (!need(4)) return 0; std::uint32_t v; std::memcpy(&v, d + p, 4); p += 4; return v; }
    std::int32_t s32() { return static_cast<std::int32_t>(u32()); }
    float f32() { if (!need(4)) return 0; float v; std::memcpy(&v, d + p, 4); p += 4; return v; }
    std::string str16(std::size_t limit = 4096) {
        const std::uint16_t k = u16();
        if (bad) return "";
        if (k > limit) { fail("string de " + std::to_string(k) + " bytes em " + std::to_string(p - 2)); return ""; }
        if (!need(k)) return "";
        std::string s(reinterpret_cast<const char*>(d + p), k);
        p += k;
        return s;
    }
    std::vector<float> floats(std::size_t k) { std::vector<float> v(k); for (auto& x : v) x = f32(); return v; }
};

float fat(const std::uint8_t* p) { float v; std::memcpy(&v, p, 4); return v; }
std::int32_t iat(const std::uint8_t* p) { std::int32_t v; std::memcpy(&v, p, 4); return v; }

B3dMaterial material_top(R& r) {
    B3dMaterial m;
    m.id = r.str16(); m.name = r.str16();
    m.floats = r.floats(18);
    m.blend = r.str16(); m.type = r.str16();
    r.s32(); r.s32();
    return m;
}

B3dMaterial material44(R& r) {
    B3dMaterial m;
    m.name = r.str16(); m.id = r.str16();
    m.floats = r.floats(17);
    m.blend = r.str16(); m.type = r.str16();
    std::int32_t words[16];
    for (auto& w : words) w = r.s32();
    const std::int32_t nt = words[15];
    if (!r.bad && (nt < 0 || nt > 8)) { r.fail("contagem de texturas absurda (" + std::to_string(nt) + ") em " + std::to_string(r.p)); return m; }
    for (std::int32_t i = 0; i < nt && !r.bad; ++i) m.textures.push_back(r.str16());
    r.u32();
    return m;
}

// familia 64 a partir de r.p == inicio dos vertices; weighted = variante com pesos
bool surface64(R& r, std::int32_t nv, bool weighted, int nbones, B3dSurface& s, const std::vector<B3dMaterial>* tops) {
    if (nv < 0 || nv > 2000000 || !r.need(static_cast<std::size_t>(nv) * 64)) return false;
    s.family = 64;
    s.weighted64 = weighted;
    const std::uint8_t* v = r.d + r.p;
    s.pos.resize(static_cast<std::size_t>(nv) * 3); s.nrm.resize(static_cast<std::size_t>(nv) * 3); s.col.resize(static_cast<std::size_t>(nv) * 4);
    s.uv0.resize(static_cast<std::size_t>(nv) * 2);
    if (!weighted) s.uv1.resize(static_cast<std::size_t>(nv) * 2);
    else { s.influences.resize(static_cast<std::size_t>(nv)); s.bones_per_vertex = 1; }
    for (std::int32_t i = 0; i < nv; ++i) {
        const std::uint8_t* q = v + static_cast<std::size_t>(i) * 64;
        for (int k = 0; k < 3; ++k) { s.pos[static_cast<std::size_t>(i) * 3 + k] = fat(q + 4 * k); s.nrm[static_cast<std::size_t>(i) * 3 + k] = fat(q + 12 + 4 * k); }
        for (int k = 0; k < 4; ++k) s.col[static_cast<std::size_t>(i) * 4 + k] = fat(q + 24 + 4 * k);
        s.uv0[static_cast<std::size_t>(i) * 2] = fat(q + 44); s.uv0[static_cast<std::size_t>(i) * 2 + 1] = fat(q + 48);
        if (!weighted) { s.uv1[static_cast<std::size_t>(i) * 2] = fat(q + 52); s.uv1[static_cast<std::size_t>(i) * 2 + 1] = fat(q + 56); }
        else {
            const std::int32_t cnt = iat(q + 52), bone = iat(q + 56);
            const float w = fat(q + 60);
            if (cnt >= 1 && bone >= 0 && bone < nbones && w > 0) s.influences[static_cast<std::size_t>(i)].push_back({bone, w});
        }
    }
    r.p += static_cast<std::size_t>(nv) * 64;
    r.u32(); r.u32();
    s.lightmap = r.str16();
    const std::int32_t nt = r.s32();
    if (r.bad || nt < 0 || nt > 8) return false;
    std::vector<std::string> tex;
    for (std::int32_t i = 0; i < nt && !r.bad; ++i) tex.push_back(r.str16());
    B3dMaterial m;
    m.name = s.lightmap; m.blend = "OPAQUE"; m.textures = tex;
    // v20: o tipo de sombreamento (BASE_2X...) vem do material de mesmo nome na lista do topo do arquivo (so o id: o resto fica
    // como no conversor Python, para os .f3m continuarem iguais byte a byte)
    if (tops) for (const B3dMaterial& t : *tops) if (t.name == s.lightmap || t.id == s.lightmap) { m.id = t.id; break; }
    s.materials = {m};
    r.u32(); r.u8();
    const std::int32_t kk = r.s32();
    if (r.bad || kk < 0 || kk % 3 || r.p + static_cast<std::size_t>(kk) * 5 > r.n) return false;
    B3dSurface::Prim pr;
    pr.material = 0;
    pr.idx.resize(static_cast<std::size_t>(kk));
    for (std::int32_t i = 0; i < kk; ++i) {
        const std::int32_t ix = iat(r.d + r.p + 4 * static_cast<std::size_t>(i));
        if (ix < 0 || ix >= std::max(nv, 1)) return false;
        pr.idx[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(ix);
    }
    r.p += static_cast<std::size_t>(kk) * 4;
    r.p += static_cast<std::size_t>(kk);
    s.prims.push_back(std::move(pr));
    return !r.bad;
}

bool surface(R& r, int nbones, B3dSurface& s, const std::vector<B3dMaterial>* tops) {
    const std::size_t start = r.p;
    r.u8(); r.u8();
    const std::uint8_t vflag = r.u8();
    if (r.bad) return false;
    if (vflag == 1) {
        const std::int32_t nv = r.s32();
        if (nv < 0 || nv > 2000000) { r.fail("numVerts absurdo (" + std::to_string(nv) + ") na superficie em " + std::to_string(start)); return false; }
        r.s32();                                       // sizeOfVerts (44)
        if (!r.need(static_cast<std::size_t>(nv) * 44)) return false;
        s.family = 44;
        s.pos.resize(static_cast<std::size_t>(nv) * 3); s.nrm.resize(static_cast<std::size_t>(nv) * 3); s.col.resize(static_cast<std::size_t>(nv) * 4); s.uv0.resize(static_cast<std::size_t>(nv) * 2);
        for (std::int32_t i = 0; i < nv; ++i) {
            const std::uint8_t* q = r.d + r.p + static_cast<std::size_t>(i) * 44;
            for (int k = 0; k < 3; ++k) { s.pos[static_cast<std::size_t>(i) * 3 + k] = fat(q + 4 * k); s.nrm[static_cast<std::size_t>(i) * 3 + k] = fat(q + 12 + 4 * k); }
            for (int k = 0; k < 4; ++k) s.col[static_cast<std::size_t>(i) * 4 + k] = static_cast<float>(q[24 + k]) / 255.0f;
            s.uv0[static_cast<std::size_t>(i) * 2] = fat(q + 28); s.uv0[static_cast<std::size_t>(i) * 2 + 1] = fat(q + 32);
        }
        r.p += static_cast<std::size_t>(nv) * 44;
        const std::vector<float> bb = r.floats(6);
        if (r.bad) return false;
        if (nv) {
            for (int k = 0; k < 3; ++k) {
                float mn = s.pos[static_cast<std::size_t>(k)], mx = mn;
                for (std::int32_t i = 1; i < nv; ++i) { const float x = s.pos[static_cast<std::size_t>(i) * 3 + k]; mn = std::min(mn, x); mx = std::max(mx, x); }
                if (std::fabs(bb[static_cast<std::size_t>(k)] - mn) > 1e-4f || std::fabs(bb[static_cast<std::size_t>(k) + 3] - mx) > 1e-4f) { r.fail("caixa envolvente nao confere com as posicoes (superficie em " + std::to_string(start) + ")"); return false; }
            }
        }
        if (r.u8() == 1) {
            const std::int32_t bpv = r.s32();
            if (bpv < 0 || bpv > 16) { r.fail("bonesPerVertex absurdo (" + std::to_string(bpv) + ")"); return false; }
            if (!r.need(static_cast<std::size_t>(nv) * static_cast<std::size_t>(bpv) * 8)) return false;
            s.bones_per_vertex = bpv;
            s.influences.resize(static_cast<std::size_t>(nv));
            const std::uint8_t* pr = r.d + r.p;
            for (std::int32_t v = 0; v < nv; ++v)
                for (std::int32_t k = 0; k < bpv; ++k) {
                    const std::size_t o = 8 * (static_cast<std::size_t>(k) * static_cast<std::size_t>(nv) + static_cast<std::size_t>(v));
                    const std::int32_t bi = iat(pr + o);
                    const float bw = fat(pr + o + 4);
                    if (bi >= 0 && bw > 0) s.influences[static_cast<std::size_t>(v)].push_back({bi, bw});
                }
            r.p += static_cast<std::size_t>(nv) * static_cast<std::size_t>(bpv) * 8;
        }
        const std::int32_t nm = r.s32();
        if (r.bad || nm < 0 || nm > 64) { r.fail("numMaterials absurdo (" + std::to_string(nm) + ")"); return false; }
        for (std::int32_t i = 0; i < nm && !r.bad; ++i) s.materials.push_back(material44(r));
        for (std::int32_t i = 0; i < nm && !r.bad; ++i) { const std::string n = r.str16(); (void)n; }
        std::vector<std::int32_t> ml(static_cast<std::size_t>(nm)), fc(static_cast<std::size_t>(nm));
        for (auto& x : ml) x = r.s32();
        for (auto& x : fc) x = r.s32();
        for (std::int32_t mi = 0; mi < nm && !r.bad; ++mi) {
            const std::int32_t ni = ml[static_cast<std::size_t>(mi)];
            if (ni < 0 || ni % 3) { r.fail("lista de indices invalida (" + std::to_string(ni) + ")"); return false; }
            if (!r.need(2 * static_cast<std::size_t>(ni))) return false;
            B3dSurface::Prim p;
            p.material = mi;
            p.idx.resize(static_cast<std::size_t>(ni));
            for (std::int32_t i = 0; i < ni; ++i) {
                const std::uint32_t ix = static_cast<std::uint32_t>(r.d[r.p + 2 * static_cast<std::size_t>(i)] | (r.d[r.p + 2 * static_cast<std::size_t>(i) + 1] << 8));
                if (ix >= static_cast<std::uint32_t>(std::max(nv, 1))) { r.fail("indice de vertice fora do intervalo"); return false; }
                p.idx[static_cast<std::size_t>(i)] = ix;
            }
            r.p += 2 * static_cast<std::size_t>(ni);
            s.prims.push_back(std::move(p));
        }
        return !r.bad;
    }
    // familia 64. Tentativa A (terreno, como no Python): numVerts depois do byte de tipo.
    {
        R a = r;
        std::string ea;
        a.err = &ea;
        const std::int32_t nv = a.s32();
        B3dSurface sa;
        if (!a.bad && nv >= 0 && nv <= 2000000 && surface64(a, nv, false, nbones, sa, tops)) {
            std::string* keep = r.err;
            r = a;
            r.err = keep;                              // `a` apontava para o erro local da tentativa
            s = std::move(sa);
            return true;
        }
    }
    // Tentativa B (v20, "64 com pesos"): o byte que seria o tipo ja e o 1o byte de numVerts.
    {
        r.p -= 1;
        const std::int32_t nv = r.s32();
        if (r.bad || nv < 0 || nv > 2000000) { r.fail("numVerts absurdo (" + std::to_string(nv) + ") na superficie em " + std::to_string(start)); return false; }
        // a variante com pesos: (s32 n, s32 osso, f32 peso) em todos os vertices
        bool weighted = nbones > 0 && r.p + static_cast<std::size_t>(nv) * 64 <= r.n;
        for (std::int32_t i = 0; weighted && i < nv; ++i) {
            const std::uint8_t* q = r.d + r.p + static_cast<std::size_t>(i) * 64;
            const std::int32_t c = iat(q + 52), b = iat(q + 56);
            if (c < 0 || c > 4 || b < -1 || b >= nbones) weighted = false;
        }
        if (!surface64(r, nv, weighted, nbones, s, tops)) { r.fail("superficie de 64 bytes nao decodificou (em " + std::to_string(start) + ")"); return false; }
        return true;
    }
}

bool node(R& r, B3dNode& n, int nbones, int depth, const std::vector<B3dMaterial>* tops) {
    while (!r.bad) {
        if (r.p >= r.n) { r.fail("arquivo terminou dentro de um no"); return false; }
        const std::uint8_t t = r.u8();
        if (t == 0x0B) return true;
        if (t == 0x0C) n.name = r.str16();
        else if (t == 0x0D) {
            r.str16();
            const std::uint8_t fl = r.u8();
            if (fl & 1) { n.has_t = true; for (float& x : n.t) x = r.f32(); }
            if (fl & 2) { n.has_r = true; for (float& x : n.r) x = r.f32(); }
            if (fl & 4) { n.has_s = true; for (float& x : n.s) x = r.f32(); }
        } else if (t == 0x0F || t == 0x19) {
            B3dSurface s;
            if (!surface(r, nbones, s, tops)) return false;
            n.surfaces.push_back(std::move(s));
        } else if (t == 0x1B) {
            const std::int32_t k = r.s32();
            if (k < 0 || k > 4096) { r.fail("lista de zonas absurda"); return false; }
            for (std::int32_t i = 0; i < k && !r.bad; ++i) r.str16();
        } else if (t == 0x0A || t == 0x13) {
            if (depth > 32) { r.fail("nos aninhados demais"); return false; }
            B3dNode c;
            if (!node(r, c, nbones, depth + 1, tops)) return false;
            n.children.push_back(std::move(c));
        } else { r.fail("token de no desconhecido " + std::to_string(t) + " em " + std::to_string(r.p - 1)); return false; }
    }
    return false;
}

}  // namespace

bool load_b3d(const std::uint8_t* data, std::size_t size, B3dModel& out, std::string& err) {
    out = B3dModel();
    err.clear();
    if (!data || size < 8 || std::memcmp(data, "B3D ", 4) != 0) { err = "nao comeca com 'B3D '"; return false; }
    out.signature.assign(reinterpret_cast<const char*>(data), 8);
    R r{data, size, 8, &err};
    while (r.p < size && !r.bad) {
        const std::uint8_t t = r.u8();
        if (t == 0x01) return true;
        if (t == 0x03) out.global_scale = r.f32();
        else if (t == 0x04) out.coordinate_scale = r.f32();
        else if (t == 0x05) out.coordinate_system = r.str16();
        else if (t == 0x06) { r.s32(); r.s32(); r.s32(); }
        else if (t == 0x16) r.s32();
        else if (t == 0x1C || t == 0x35) r.u8();
        else if (t == 0x07 || t == 0x0F || t == 0x10) out.materials.push_back(material_top(r));
        else if (t == 0x08 || t == 0x09) { r.u8(); r.str16(); r.str16(); r.u32(); r.u32(); }
        else if (t == 0x0E || t == 0x18) {
            B3dBone b;
            b.name = r.str16(); b.unk = r.u16(); b.flag = r.u8();
            if (b.flag) for (float& x : b.t) x = r.f32();
            if (b.flag == 3) for (float& x : b.r) x = r.f32();
            if (b.flag == 2) b.extra = r.f32();
            out.bones.push_back(std::move(b));
        } else if (t == 0x0A || t == 0x13) {
            B3dNode n;
            if (!node(r, n, static_cast<int>(out.bones.size()), 0, &out.materials)) break;
            out.roots.push_back(std::move(n));
        } else { r.fail("token de topo desconhecido " + std::to_string(t) + " em " + std::to_string(r.p - 1)); break; }
    }
    if (err.empty()) err = "arquivo terminou sem o token final 0x01";
    return false;
}

}  // namespace f3v
