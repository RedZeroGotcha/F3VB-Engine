// v20: monta o mapa em memoria (veja map_build.h). Cada parse_* corresponde ao decodificador de mesmo nome do mapfile.py.
#include "map_build.h"

#include <cstring>

#include "tre.h"
#include "vfs.h"

namespace f3v {

namespace {

std::uint32_t rd32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
float rdf(const std::uint8_t* p) { float f; std::uint32_t u = rd32(p); std::memcpy(&f, &u, 4); return f; }

// leitor de payload com limite: qualquer leitura fora marca 'bad' e devolve zero
struct P {
    const std::uint8_t* d;
    std::size_t n, pos = 0;
    bool bad = false;
    bool has(std::size_t k) const { return !bad && pos + k <= n; }
    std::uint8_t u8() { if (!has(1)) { bad = true; return 0; } return d[pos++]; }
    std::uint32_t u32() { if (!has(4)) { bad = true; return 0; } const std::uint32_t v = rd32(d + pos); pos += 4; return v; }
    float f32() { if (!has(4)) { bad = true; return 0; } const float v = rdf(d + pos); pos += 4; return v; }
    Vec3f vec3() { Vec3f v{}; v[0] = f32(); v[1] = f32(); v[2] = f32(); return v; }
    std::string str() {   // u16 comprimento + bytes
        if (!has(2)) { bad = true; return {}; }
        const std::size_t k = d[pos] | (d[pos + 1] << 8);
        if (pos + 2 + k > n) { bad = true; return {}; }
        std::string s(reinterpret_cast<const char*>(d + pos + 2), k);
        pos += 2 + k;
        return s;
    }
};

// _plausible_string_at: u16 n >= 3 e so ASCII imprimivel
bool plausible_string_at(const std::uint8_t* d, std::size_t n, std::size_t pos, std::string& s, std::size_t& end) {
    if (pos + 2 > n) return false;
    const std::size_t k = d[pos] | (d[pos + 1] << 8);
    if (k < 3 || pos + 2 + k > n) return false;
    for (std::size_t i = 0; i < k; ++i) { const std::uint8_t c = d[pos + 2 + i]; if (c < 32 || c >= 127) return false; }
    s.assign(reinterpret_cast<const char*>(d + pos + 2), k);
    end = pos + 2 + k;
    return true;
}

std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; }
std::string upper(std::string s) { for (char& c : s) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A'); return s; }

// parse_eeov: instancia, 3 flags e as strings plausiveis (classificadas pela extensao)
void parse_eeov(const std::uint8_t* d, std::size_t n, MapEntity& e) {
    P r{d, n};
    e.instance = r.str();
    if (r.bad) return;
    std::size_t pos = r.pos + 3;
    bool got_name = false, got_script = false, got_skin = false;
    while (pos < n) {
        std::string s;
        std::size_t end;
        if (!plausible_string_at(d, n, pos, s, end)) { ++pos; continue; }
        const std::size_t dot = s.find_last_of('.');
        const std::string ext = dot == std::string::npos ? std::string() : lower(s.substr(dot + 1));
        if (ext == "amx") { if (!got_script) { e.script = s; got_script = true; } }
        else if (ext == "dds" || ext == "tga") { if (!got_skin) { e.skin = s; got_skin = true; } }
        else if (ext == "veg" || ext == "psf" || ext == "amo" || ext == "wea" || ext == "arm" || ext == "itm" || ext == "use" || ext == "con") {}
        else if (!got_name) { e.name = s; got_name = true; }
        pos = end;
    }
}

}  // namespace

bool parse_map_file(const std::uint8_t* d, std::size_t size, MapData& out, std::string& tre, std::string& rle, std::string& err) {
    tre.clear(); rle.clear();
    if (!d || size < 12) { err = "arquivo .map vazio"; return false; }
    std::size_t pos = 0;
    int group = -1;            // indice do gatilho aberto pelo ultimo EMTR
    bool group_bound = false;
    while (pos + 12 <= size) {
        char tag[5] = {0, 0, 0, 0, 0};
        std::memcpy(tag, d + pos, 4);
        const std::uint32_t csize = rd32(d + pos + 8);
        if (csize < 12 || pos + csize > size) { err = std::string("enquadramento de chunks invalido em ") + std::to_string(pos); return false; }
        const std::uint8_t* p = d + pos + 12;
        const std::size_t n = csize - 12;
        pos += csize;
        const std::string t(tag);
        P r{p, n};
        if (t == "EMAP") {
            r.u32();
            tre = r.str(); rle = r.str(); r.str();
            if (r.bad) continue;
            if (n - r.pos == 27) {
                const std::uint8_t* tail = p + r.pos;
                std::memcpy(out.env_a, tail + 2, 4);
                std::memcpy(out.env_b, tail + 6, 4);
                for (int k = 0; k < 3; ++k) out.env_f[k] = rdf(tail + 15 + 4 * k);
                out.has_env = true;
            }
        } else if (t == "ECAM") {
            if (n == 16) { for (int k = 0; k < 4; ++k) out.camera[k] = rdf(p + 4 * k); out.has_camera = true; }
        } else if (t == "EME2") {
            MapEntity e;
            e.prototype = r.str();
            if (r.bad || r.pos + 25 + 12 > n) continue;
            const std::size_t dot = e.prototype.find_last_of('.');
            e.kind = upper(dot == std::string::npos ? e.prototype : e.prototype.substr(dot + 1));
            e.pos = r.vec3();
            e.yaw = r.f32();
            r.pos += 8 + 1;                                   // 2 f32 extras + flag
            if (r.pos + 12 <= n && !std::memcmp(p + r.pos, "EEOV", 4)) {
                const std::uint32_t es = rd32(p + r.pos + 8);
                if (r.pos + es == n && es >= 12) parse_eeov(p + r.pos + 12, es - 12, e);
            }
            out.entities.push_back(std::move(e));
        } else if (t == "EMEF") {
            MapEffect e;
            e.name = r.str();
            e.pos = r.vec3();
            r.f32(); r.pos += 8 + 1;
            e.effect = r.str();
            if (!r.bad) out.effects.push_back(std::move(e));
        } else if (t == "EMEP") {
            MapEntry e;
            if (n == 17) { e.id = p[0]; P q{p, n, 1}; e.pos.push_back(q.vec3()); e.angles.push_back(q.f32()); out.entries.push_back(e); }
            else if (n == 97) {
                e.id = p[0];
                P q{p, n, 1};
                for (int i = 0; i < 6; ++i) e.pos.push_back(q.vec3());
                for (int i = 0; i < 6; ++i) e.angles.push_back(q.f32());
                out.entries.push_back(e);
            }
        } else if (t == "EMNP") {
            const std::uint32_t cnt = r.u32();
            if (!r.bad && n == 4 + 30ull * cnt) {
                for (std::uint32_t i = 0; i < cnt; ++i) {
                    const std::uint8_t* o = p + 4 + 30 * static_cast<std::size_t>(i);
                    MapNavNode nd;
                    nd.kind = o[0];
                    for (int k = 0; k < 3; ++k) nd.pos[static_cast<std::size_t>(k)] = rdf(o + 1 + 4 * k);
                    for (int k = 25; k < 30; ++k) if (o[k] != 0xFF) nd.links.push_back(o[k]);
                    out.nav.push_back(std::move(nd));
                }
            }
        } else if (t == "EMNO") {
            MapMarker m;
            m.pos = r.vec3();
            m.icon = r.str();
            if (r.bad) continue;
            m.text_id = r.has(4) ? r.u32() : 0;
            out.markers.push_back(std::move(m));
        } else if (t == "EMTR") {
            MapTrigger g;
            g.kind = 0;
            const std::uint32_t kind = r.u32(), np = r.u32();
            (void)kind;
            if (r.bad) continue;
            if (n == 8 + 12ull * np) for (std::uint32_t i = 0; i < np; ++i) g.pts.push_back(r.vec3());
            out.triggers.push_back(std::move(g));
            group = static_cast<int>(out.triggers.size()) - 1;
            group_bound = false;
        } else if ((t == "EBTR" || t == "ESTR" || t == "ETTR") && group >= 0 && !group_bound) {
            MapTrigger& g = out.triggers[static_cast<std::size_t>(group)];
            if (t == "EBTR") { const std::uint32_t id = r.u32(); if (r.bad) continue; g.kind = 3; g.entry = static_cast<std::int32_t>(id); }
            else if (t == "ESTR") { const std::string s = r.str(); if (r.bad) continue; g.kind = 1; g.target = s; }
            else { const std::string s = r.str(); if (r.bad) continue; g.kind = 2; g.target = s; g.entry = r.pos < n ? p[r.pos] : 0; }
            group_bound = true;
        } else if (t == "EPTH") {
            MapPath pa;
            pa.name = r.str();
            const std::uint32_t np = r.u32();
            if (r.bad) continue;
            if (n == r.pos + 24ull * np) for (std::uint32_t i = 0; i < np; ++i) { pa.pts.push_back(r.vec3()); r.pos += 12; }
            out.paths.push_back(std::move(pa));
        } else if (t == "EMSD") {
            MapSound s;
            s.name = r.str();
            s.pos = r.vec3();
            if (r.bad) continue;
            std::string snd;
            std::size_t end;
            if (plausible_string_at(p, n, r.pos, snd, end)) s.sound = snd;
            out.sounds.push_back(std::move(s));
        } else if (t == "2MWT") {
            r.str();
            if (r.bad || r.pos + 144 > n) continue;
            r.pos += 140;
            const std::uint32_t cnt = r.u32();
            for (std::uint32_t i = 0; i < cnt; ++i) {
                MapWater w;
                w.pos = r.vec3();
                w.texture = r.str();
                r.f32(); r.f32();
                if (r.bad) break;
                out.water.push_back(std::move(w));
            }
        }
    }
    if (pos != size) { err = "sobraram " + std::to_string(size - pos) + " bytes depois do ultimo chunk do .map"; return false; }
    return true;
}

bool parse_rle(const std::uint8_t* d, std::size_t size, MapData& out, std::string& err) {
    if (!d || size < 13) { err = ".rle menor que o cabecalho"; return false; }
    const std::uint32_t a = rd32(d), b = rd32(d + 4);
    if (a == 0 || b < 1 || a > 65536 || b > 65536) { err = ".rle com dimensoes absurdas"; return false; }
    const std::size_t rows = b - 1, total = static_cast<std::size_t>(a) * rows;
    std::vector<std::uint8_t> cells;
    cells.reserve(total);
    std::size_t pos = 13;
    while (pos < size) {
        if (pos + 5 > size) { err = "run incompleto no .rle"; return false; }
        const std::uint32_t cnt = rd32(d + pos);
        const std::uint8_t v = d[pos + 4];
        if (cnt == 0 || cells.size() + cnt > total || (cells.size() % a) + cnt > a) { err = "run invalido no .rle em " + std::to_string(pos); return false; }
        cells.insert(cells.end(), cnt, v);
        pos += 5;
    }
    if (cells.size() != total) { err = ".rle com celulas faltando"; return false; }
    out.grid_w = a;
    out.grid_rows = static_cast<std::uint32_t>(rows);
    out.grid = std::move(cells);
    return true;
}

bool build_map(const Vfs& vfs, const std::string& map_name, MapData& out, std::string& err, MapBuildStats* stats) {
    out = MapData();
    const ResEntry* me = vfs.find(map_name, RT_MAP);
    if (!me) { err = "mapa '" + map_name + "' nao existe na demo"; return false; }
    std::vector<std::uint8_t> data;
    if (!vfs.read(*me, data, err)) return false;
    std::string tre_name, rle_name;
    if (!parse_map_file(data.data(), data.size(), out, tre_name, rle_name, err)) return false;
    out.version = 3;
    out.name = me->name;
    out.tre = tre_name;
    out.rle = rle_name;
    auto stem = [](const std::string& s) { const std::size_t dot = s.find_last_of('.'); return dot == std::string::npos ? s : s.substr(0, dot); };
    if (stats) { stats->tre_name = tre_name; stats->rle_name = rle_name; }
    if (!rle_name.empty()) {
        std::vector<std::uint8_t> rd;
        std::string e2;
        if (vfs.read(stem(rle_name), RT_RLE, rd) && !parse_rle(rd.data(), rd.size(), out, e2)) { out.grid.clear(); out.grid_w = out.grid_rows = 0; }
    }
    if (!tre_name.empty()) {
        std::vector<std::uint8_t> td;
        if (vfs.read(stem(tre_name), RT_TRE, td)) {
            TreData t;
            if (!read_tre(td.data(), td.size(), t, err)) { err = "geometria (" + tre_name + "): " + err; return false; }
            float mn[3], mx[3];
            if (t.bounds(mn, mx)) { out.origin[0] = mn[0]; out.origin[1] = mn[2]; }
            if (t.has_level && t.level_f[0] > 0) out.cell = t.level_f[0];
            TreStats ts;
            tre_triangle_lists(t, out.tris, &ts);
            out.blocks = std::move(t.blocks);
            out.materials = std::move(t.materials);
            out.textures = std::move(t.textures);
            for (const TreLight& l : t.lights) {
                MapLight ml;
                ml.type = l.type;
                for (int k = 0; k < 3; ++k) { ml.color[static_cast<std::size_t>(k)] = l.color[k]; ml.pos[static_cast<std::size_t>(k)] = l.pos[k]; ml.dir[static_cast<std::size_t>(k)] = l.dir[k]; }
                out.lights.push_back(ml);
            }
            if (stats) { stats->nodes = ts.nodes; stats->nodes_full = ts.nodes_full; stats->unparsed = ts.unparsed; stats->outside_bbox = ts.outside_bbox; stats->bad = ts.bad; }
        }
    }
    map_split_texture_names(out);
    return true;
}

}  // namespace f3v
