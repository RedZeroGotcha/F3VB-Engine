#include "f3map.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace f3v {

namespace {

struct Rd {
    const std::uint8_t* p;
    std::size_t n, i = 0;
    bool ok = true;
    Rd(const std::uint8_t* d, std::size_t s) : p(d), n(s) {}
    std::size_t left() const { return n - i; }
    bool need(std::size_t k) { if (!ok || k > left()) { ok = false; return false; } return true; }
    std::uint8_t u8() { if (!need(1)) return 0; return p[i++]; }
    std::uint32_t u32() { if (!need(4)) return 0; std::uint32_t v; std::memcpy(&v, p + i, 4); i += 4; return v; }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    float f32() { if (!need(4)) return 0; float v; std::memcpy(&v, p + i, 4); i += 4; return v; }
    Vec3f vec() { Vec3f v{}; for (int k = 0; k < 3; ++k) v[static_cast<std::size_t>(k)] = f32(); return v; }
    std::string str() {
        if (!need(2)) return {};
        const std::size_t len = static_cast<std::size_t>(p[i]) | (static_cast<std::size_t>(p[i + 1]) << 8);
        i += 2;
        if (!need(len)) return {};
        std::string s(reinterpret_cast<const char*>(p + i), len);
        i += len;
        return s;
    }
    // contagem de itens que ocupam >= min_bytes cada: rejeita contagens impossiveis antes de alocar
    std::uint32_t count(std::size_t min_bytes) {
        const std::uint32_t c = u32();
        if (!ok) return 0;
        if (min_bytes && c > left() / min_bytes) { ok = false; return 0; }
        return c;
    }
};

bool finite3(const Vec3f& v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

}  // namespace

std::size_t MapData::triangle_count() const {
    std::size_t n = 0;
    for (const MapTriList& t : tris) n += t.idx.size() / 3;
    return n;
}

bool MapData::bounds(float mn[3], float mx[3]) const {
    bool any = false;
    auto add = [&](const Vec3f& p) {
        if (!finite3(p)) return;
        for (int i = 0; i < 3; ++i) {
            const std::size_t k = static_cast<std::size_t>(i);
            if (!any) { mn[i] = mx[i] = p[k]; } else { mn[i] = std::min(mn[i], p[k]); mx[i] = std::max(mx[i], p[k]); }
        }
        any = true;
    };
    for (const MapBlock& b : blocks) for (std::uint32_t v = 0; v < b.nv; ++v) add({b.xyz[v * 3], b.xyz[v * 3 + 1], b.xyz[v * 3 + 2]});
    for (const MapEntity& e : entities) add(e.pos);
    for (const MapNavNode& n : nav) add(n.pos);
    for (const MapEntry& e : entries) for (const Vec3f& p : e.pos) add(p);
    for (const MapTrigger& t : triggers) for (const Vec3f& p : t.pts) add(p);
    for (const MapPath& t : paths) for (const Vec3f& p : t.pts) add(p);
    for (const MapSound& s : sounds) add(s.pos);
    for (const MapMarker& s : markers) add(s.pos);
    for (const MapEffect& s : effects) add(s.pos);
    for (const MapWater& s : water) add(s.pos);
    return any;
}

bool MapData::cell_of(const Vec3f& p, int& col, int& row) const {
    if (grid_w == 0 || grid_rows == 0 || !(cell > 0) || !finite3(p)) return false;
    const float fc = (p[0] - origin[0]) / cell, fr = (p[2] - origin[1]) / cell;
    if (fc < 0 || fr < 0 || fc >= static_cast<float>(grid_w) || fr >= static_cast<float>(grid_rows)) return false;
    col = static_cast<int>(fc);
    row = static_cast<int>(fr);
    return true;
}

bool MapData::on_floor(const Vec3f& p) const {
    int c = 0, r = 0;
    return cell_of(p, c, r) && grid[static_cast<std::size_t>(r) * grid_w + static_cast<std::size_t>(c)] != 0x2F;
}

void MapData::floor_hits(std::size_t& hits, std::size_t& total) const {
    hits = total = 0;
    auto test = [&](const Vec3f& p) { ++total; if (on_floor(p)) ++hits; };
    for (const MapEntity& e : entities) test(e.pos);
    for (const MapNavNode& n : nav) test(n.pos);
    for (const MapEntry& e : entries) for (const Vec3f& p : e.pos) if (p[0] != 0 || p[2] != 0) test(p);
}

bool load_f3map(const std::uint8_t* data, std::size_t size, MapData& out, std::string& err) {
    out = MapData();
    if (size < 8 || std::memcmp(data, "F3MP", 4) != 0) { err = "assinatura F3MP ausente"; return false; }
    std::uint32_t ver;
    std::memcpy(&ver, data + 4, 4);
    if (ver < 1 || ver > 3) { err = "versao do .f3map nao suportada: " + std::to_string(ver); return false; }
    out.version = ver;
    std::size_t pos = 8;
    while (pos + 8 <= size) {
        char tag[5] = {0, 0, 0, 0, 0};
        std::memcpy(tag, data + pos, 4);
        std::uint32_t n;
        std::memcpy(&n, data + pos + 4, 4);
        if (n > size - pos - 8) { err = std::string("chunk ") + tag + " ultrapassa o arquivo"; return false; }
        Rd r(data + pos + 8, n);
        pos += 8 + static_cast<std::size_t>(n);
        const std::string t = tag;
        if (t == "INFO") {
            out.name = r.str(); out.tre = r.str(); out.rle = r.str();
            out.origin[0] = r.f32(); out.origin[1] = r.f32(); out.cell = r.f32(); r.u32();
        } else if (t == "GRID") {
            out.grid_w = r.u32(); out.grid_rows = r.u32();
            const std::uint64_t cells = static_cast<std::uint64_t>(out.grid_w) * out.grid_rows;
            if (!r.ok || cells > r.left()) { err = "GRID inconsistente"; return false; }
            out.grid.assign(r.p + r.i, r.p + r.i + cells);
            r.i += static_cast<std::size_t>(cells);
        } else if (t == "GEOM") {
            const std::uint32_t nb = r.count(4);
            for (std::uint32_t b = 0; b < nb && r.ok; ++b) {
                MapBlock blk;
                blk.nv = r.u32();
                if (!r.ok || blk.nv > r.left() / (ver >= 3 ? 44 : 28)) { err = "bloco de vertices inconsistente"; return false; }
                blk.xyz.resize(static_cast<std::size_t>(blk.nv) * 3);
                blk.nrm.resize(static_cast<std::size_t>(blk.nv) * 3);
                for (float& f : blk.xyz) f = r.f32();
                for (float& f : blk.nrm) f = r.f32();
                if (!r.need(static_cast<std::size_t>(blk.nv) * 4)) break;
                blk.rgba.assign(r.p + r.i, r.p + r.i + static_cast<std::size_t>(blk.nv) * 4);
                r.i += static_cast<std::size_t>(blk.nv) * 4;
                if (ver >= 3) {
                    blk.uv.resize(static_cast<std::size_t>(blk.nv) * 2);
                    blk.lmuv.resize(static_cast<std::size_t>(blk.nv) * 2);
                    for (float& f : blk.uv) f = r.f32();
                    for (float& f : blk.lmuv) f = r.f32();
                }
                out.blocks.push_back(std::move(blk));
            }
            const std::uint32_t nl = r.count(ver >= 2 ? 20 : 8);
            for (std::uint32_t l = 0; l < nl && r.ok; ++l) {
                MapTriList tl;
                tl.block = r.u32();
                if (ver >= 2) { tl.material = r.u32(); tl.page = r.u32(); tl.tex = r.u32(); }
                const std::uint32_t nt = r.u32();
                if (!r.ok || tl.block >= out.blocks.size() || static_cast<std::uint64_t>(nt) * 12 > r.left()) { err = "lista de triangulos inconsistente"; return false; }
                tl.idx.resize(static_cast<std::size_t>(nt) * 3);
                const std::uint32_t nv = out.blocks[tl.block].nv;
                for (std::uint32_t& x : tl.idx) { x = r.u32(); if (x >= nv) { err = "indice de triangulo fora do bloco"; return false; } }
                out.tris.push_back(std::move(tl));
            }
        } else if (t == "MATL") {
            const std::uint32_t c = r.count(20);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) {
                MapMaterial m;
                m.id = r.u32(); m.name = r.str();
                for (std::uint8_t& q : m.rgba) q = r.u8();
                m.blend = r.str(); m.surface = r.str(); m.flags = r.u32();
                if (r.ok) out.materials.push_back(std::move(m));
            }
        } else if (t == "TEXN") {
            const std::uint32_t c = r.count(2);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) { std::string s = r.str(); if (r.ok) out.textures.push_back(std::move(s)); }
        } else if (t == "ENTS") {
            const std::uint32_t c = r.count(6 * 2 + 16);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) {
                MapEntity e;
                e.kind = r.str(); e.prototype = r.str(); e.instance = r.str(); e.name = r.str(); e.script = r.str(); e.skin = r.str();
                e.pos = r.vec(); e.yaw = r.f32();
                if (r.ok) out.entities.push_back(std::move(e));
            }
        } else if (t == "NAVN") {
            const std::uint32_t c = r.count(14);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) {
                MapNavNode nd;
                nd.kind = r.u8(); nd.pos = r.vec();
                const std::uint8_t nl = r.u8();
                if (!r.need(nl)) break;
                nd.links.assign(r.p + r.i, r.p + r.i + nl);
                r.i += nl;
                out.nav.push_back(std::move(nd));
            }
        } else if (t == "ENTR") {
            const std::uint32_t c = r.count(2);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) {
                MapEntry e;
                e.id = r.u8();
                const std::uint8_t np = r.u8();
                for (std::uint8_t q = 0; q < np && r.ok; ++q) e.pos.push_back(r.vec());
                for (std::uint8_t q = 0; q < np && r.ok; ++q) e.angles.push_back(r.f32());
                if (r.ok) out.entries.push_back(std::move(e));
            }
        } else if (t == "TRIG") {
            const std::uint32_t c = r.count(11);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) {
                MapTrigger g;
                g.kind = r.u8(); g.target = r.str(); g.entry = r.i32();
                const std::uint32_t np = r.count(12);
                for (std::uint32_t q = 0; q < np && r.ok; ++q) g.pts.push_back(r.vec());
                if (r.ok) out.triggers.push_back(std::move(g));
            }
        } else if (t == "PATH") {
            const std::uint32_t c = r.count(6);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) {
                MapPath pa;
                pa.name = r.str();
                const std::uint32_t np = r.count(12);
                for (std::uint32_t q = 0; q < np && r.ok; ++q) pa.pts.push_back(r.vec());
                if (r.ok) out.paths.push_back(std::move(pa));
            }
        } else if (t == "SOND") {
            const std::uint32_t c = r.count(16);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) { MapSound s; s.name = r.str(); s.pos = r.vec(); s.sound = r.str(); if (r.ok) out.sounds.push_back(std::move(s)); }
        } else if (t == "MARK") {
            const std::uint32_t c = r.count(18);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) { MapMarker s; s.pos = r.vec(); s.icon = r.str(); s.text_id = r.u32(); if (r.ok) out.markers.push_back(std::move(s)); }
        } else if (t == "EFFT") {
            const std::uint32_t c = r.count(16);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) { MapEffect s; s.name = r.str(); s.pos = r.vec(); s.effect = r.str(); if (r.ok) out.effects.push_back(std::move(s)); }
        } else if (t == "WATR") {
            const std::uint32_t c = r.count(14);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) { MapWater s; s.pos = r.vec(); s.texture = r.str(); if (r.ok) out.water.push_back(std::move(s)); }
        } else if (t == "LGHT") {
            const std::uint32_t c = r.count(40);
            for (std::uint32_t k = 0; k < c && r.ok; ++k) { MapLight l; l.type = r.u32(); l.color = r.vec(); l.pos = r.vec(); l.dir = r.vec(); if (r.ok) out.lights.push_back(l); }
        } else if (t == "CAMR") {
            for (float& f : out.camera) f = r.f32();
            out.has_camera = r.ok;
        } else if (t == "ENVI") {
            for (auto& c : out.env_a) c = r.u8();
            for (auto& c : out.env_b) c = r.u8();
            for (float& f : out.env_f) f = r.f32();
            out.has_env = r.ok;
        }   // tags desconhecidas sao ignoradas (compatibilidade futura)
        if (!r.ok) { err = std::string("chunk ") + tag + " cortado ou inconsistente"; return false; }
    }
    if (pos != size) { err = "bytes sobrando no fim do arquivo"; return false; }
    map_split_texture_names(out);
    return true;
}

void map_split_texture_names(MapData& out) {
    out.tga_names.clear();
    out.ctx_names.clear();
    for (const std::string& s : out.textures) {         // separa difusas (.tga) e paginas de lightmap (.ctx), preservando a ordem (indices 1-based)
        std::string low = s;
        for (char& ch : low) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        const std::size_t dot = s.find_last_of('.');
        const std::string stem = dot == std::string::npos ? s : s.substr(0, dot);
        if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".tga") == 0) out.tga_names.push_back(stem);
        else if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".ctx") == 0) out.ctx_names.push_back(stem);
    }
}

bool load_f3map_file(const std::string& path, MapData& out, std::string& err) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { err = "nao consegui abrir " + path; return false; }
    std::vector<std::uint8_t> buf;
    std::uint8_t tmp[65536];
    std::size_t n;
    while ((n = std::fread(tmp, 1, sizeof tmp, f)) > 0) buf.insert(buf.end(), tmp, tmp + n);
    std::fclose(f);
    return load_f3map(buf.data(), buf.size(), out, err);
}

}  // namespace f3v
