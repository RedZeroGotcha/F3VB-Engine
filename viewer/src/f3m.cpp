#include "f3m.h"

#include <cstdio>
#include <cstring>

namespace f3v {
namespace {

struct Reader {
    const std::uint8_t* d;
    std::size_t n, p = 0;
    bool ok = true;
    bool need(std::size_t k) { if (!ok || k > n - p || p > n) { ok = false; } return ok; }
    template <class T> T get() {
        T v{};
        if (need(sizeof(T))) { std::memcpy(&v, d + p, sizeof(T)); p += sizeof(T); }
        return v;
    }
    void skip(std::size_t k) { if (need(k)) p += k; }
    std::string str() {
        const auto len = get<std::uint16_t>();
        std::string s;
        if (need(len)) { s.assign(reinterpret_cast<const char*>(d + p), len); p += len; }
        return s;
    }
    template <class T> bool vec(std::vector<T>& v, std::size_t count) {
        if (!ok) return false;
        if (count > (n - p) / sizeof(T)) { ok = false; return false; }      // nao ha bytes suficientes
        v.resize(count);
        if (count) std::memcpy(v.data(), d + p, count * sizeof(T));
        p += count * sizeof(T);
        return true;
    }
};

constexpr std::uint32_t kMaxItems = 1u << 22;

}  // namespace

bool load_f3m(const std::uint8_t* data, std::size_t size, F3MFile& f, std::string& err) {
    Reader r{data, size};
    f = F3MFile{};
    if (size < 4 || std::memcmp(data, "F3M1", 4) != 0) { err = "nao e um arquivo F3M1"; return false; }
    r.p = 4;
    f.flags = r.get<std::uint32_t>();
    const auto nt = r.get<std::uint32_t>(), nm = r.get<std::uint32_t>(), nme = r.get<std::uint32_t>(),
               nj = r.get<std::uint32_t>(), nc = r.get<std::uint32_t>();
    f.name = r.str();
    if (!r.ok || nt > kMaxItems || nm > kMaxItems || nme > kMaxItems || nj > 65535 || nc > kMaxItems) { err = "cabecalho invalido"; return false; }
    for (std::uint32_t i = 0; i < nt && r.ok; ++i) {
        Texture t;
        t.name = r.str(); t.w = r.get<std::uint32_t>(); t.h = r.get<std::uint32_t>();
        if (!r.ok || t.w == 0 || t.h == 0 || t.w > 16384 || t.h > 16384) { err = "textura invalida"; return false; }
        if (!r.vec(t.rgba, static_cast<std::size_t>(t.w) * t.h * 4)) break;
        f.textures.push_back(std::move(t));
    }
    for (std::uint32_t i = 0; i < nm && r.ok; ++i) {
        Material m;
        m.name = r.str(); m.texture = r.get<std::int32_t>();
        for (float& c : m.color) c = r.get<float>();
        m.alpha_mode = r.get<std::uint8_t>(); m.double_sided = r.get<std::uint8_t>(); r.skip(2);
        if (m.texture >= static_cast<int>(f.textures.size())) m.texture = -1;
        f.materials.push_back(std::move(m));
    }
    for (std::uint32_t i = 0; i < nme && r.ok; ++i) {
        Mesh me;
        me.name = r.str(); me.material = r.get<std::int32_t>();
        const auto nv = r.get<std::uint32_t>(), ni = r.get<std::uint32_t>(), nb = r.get<std::uint32_t>();
        if (!r.ok || nv > kMaxItems || ni > kMaxItems * 3 || nb > 65535) { err = "malha invalida"; return false; }
        for (std::uint32_t b = 0; b < nb && r.ok; ++b) me.bones.push_back(r.str());
        me.has_skin = r.get<std::uint8_t>() != 0; r.skip(3);
        if (!r.vec(me.pos, std::size_t{nv} * 3) || !r.vec(me.nrm, std::size_t{nv} * 3) || !r.vec(me.uv, std::size_t{nv} * 2) ||
            !r.vec(me.col, std::size_t{nv} * 4)) break;
        if (me.has_skin && (!r.vec(me.joints, std::size_t{nv} * 4) || !r.vec(me.weights, std::size_t{nv} * 4))) break;
        if (!r.vec(me.idx, ni)) break;
        for (std::uint32_t x : me.idx) if (x >= nv) { err = "indice de vertice fora do intervalo"; return false; }
        if (me.material >= static_cast<int>(f.materials.size())) me.material = -1;
        f.meshes.push_back(std::move(me));
    }
    for (std::uint32_t i = 0; i < nj && r.ok; ++i) {
        Joint j;
        j.name = r.str(); j.parent = r.get<std::int32_t>();
        for (float& v : j.t) v = r.get<float>();
        for (float& v : j.q) v = r.get<float>();
        for (float& v : j.s) v = r.get<float>();
        for (float& v : j.ibm) v = r.get<float>();
        if (j.parent >= static_cast<int>(i)) j.parent = -1;       // pais sempre antes dos filhos
        f.joints.push_back(std::move(j));
    }
    for (std::uint32_t i = 0; i < nc && r.ok; ++i) {
        Clip c;
        c.name = r.str(); c.duration = r.get<float>();
        const auto nch = r.get<std::uint32_t>();
        if (!r.ok || nch > kMaxItems) { err = "clipe invalido"; return false; }
        for (std::uint32_t k = 0; k < nch && r.ok; ++k) {
            Channel ch;
            ch.bone = r.str(); ch.path = r.get<std::uint8_t>(); r.skip(3);
            const auto nk = r.get<std::uint32_t>();
            if (!r.ok || ch.path > 2 || nk > kMaxItems) { err = "canal invalido"; return false; }
            if (!r.vec(ch.times, nk) || !r.vec(ch.values, std::size_t{nk} * (ch.path == 1 ? 4 : 3))) break;
            c.channels.push_back(std::move(ch));
        }
        f.clips.push_back(std::move(c));
    }
    if (!r.ok) { err = "arquivo truncado"; return false; }
    if (r.p != size) { err = "bytes sobrando no final"; return false; }
    return true;
}

bool load_f3m_file(const std::string& path, F3MFile& out, std::string& error) {
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) { error = "nao consegui abrir " + path; return false; }
    std::vector<std::uint8_t> buf;
    std::uint8_t tmp[65536];
    std::size_t k;
    while ((k = std::fread(tmp, 1, sizeof tmp, fp)) > 0) buf.insert(buf.end(), tmp, tmp + k);
    std::fclose(fp);
    return load_f3m(buf.data(), buf.size(), out, error);
}

}  // namespace f3v
