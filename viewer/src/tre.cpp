// v20: leitor de .8tre em C++ (veja tre.h). Cada funcao corresponde a uma do tools/py/f3vb/tre.py, com o mesmo nome em comentario.
#include "tre.h"

#include <cstring>
#include <map>
#include <tuple>

namespace f3v {

namespace {

std::uint32_t rd32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
std::uint16_t rd16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
float rdf(const std::uint8_t* p) { float f; std::uint32_t u = rd32(p); std::memcpy(&f, &u, 4); return f; }

bool is_tag(const std::uint8_t* b) {
    for (int i = 0; i < 4; ++i) { const std::uint8_t c = b[i]; if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false; }
    return true;
}

struct Chunk { char tag[5]; std::size_t pos, size; };

// _chunks: chunks consecutivos entre s e e
bool chunks(const std::uint8_t* d, std::size_t s, std::size_t e, std::vector<Chunk>& out, std::string& err) {
    out.clear();
    std::size_t p = s;
    while (p < e) {
        if (p + 8 > e || !is_tag(d + p)) { err = "chunk invalido em " + std::to_string(p); return false; }
        const std::uint32_t n = rd32(d + p + 4);
        if (p + 8 + static_cast<std::size_t>(n) > e) { err = "chunk em " + std::to_string(p) + " ultrapassa o pai"; return false; }
        Chunk c;
        std::memcpy(c.tag, d + p, 4);
        c.tag[4] = 0;
        c.pos = p + 8;
        c.size = n;
        out.push_back(c);
        p += 8 + static_cast<std::size_t>(n);
    }
    return true;
}

std::string cstr(const std::uint8_t* b, std::size_t n) {
    std::string s;
    for (std::size_t i = 0; i < n && b[i]; ++i) s.push_back(static_cast<char>(b[i]));
    return s;
}

// ------------------------------------------------------------------ gramatica dos lotes (identica ao Python)
struct Lim { std::uint32_t mats, ctx, tga; bool strict = false; };
using U = std::vector<std::uint16_t>;
struct Sub { std::uint32_t k; std::size_t from, count; };

// _sub_batches
bool sub_batches(const U& u, std::size_t q, std::size_t total, const Lim& lim, std::vector<Sub>& out) {
    const std::size_t end = q + total;
    out.clear();
    while (q < end) {
        if (q + 2 > end) return false;
        const std::uint16_t k = u[q], c = u[q + 1];
        if ((k & 0xC000) || (c & 0xC000) || c % 3 || k > lim.tga || q + 2 + c > end) return false;
        if (lim.strict && (c & 1) && (q + 2 + c >= end || u[q + 2 + c] != 0)) return false;
        out.push_back({k, q + 2, c});
        q += 2 + c + (c & 1);
    }
    return q == end;
}

// _page_group: devolve pagina e o B; subs preenchido
bool page_group(const U& u, std::size_t p, const Lim& lim, std::uint32_t& page, std::size_t& b, std::vector<Sub>& subs) {
    if (p + 2 > u.size()) return false;
    const std::uint16_t a = u[p];
    b = u[p + 1];
    if ((a & 0xC000) != 0x4000 || (b & 0xC000) || (a & 0x3FFFu) > lim.ctx || p + 2 + b > u.size()) return false;
    if (!sub_batches(u, p + 2, b, lim, subs)) return false;
    page = a & 0x3FFFu;
    return true;
}

// _mat_hdr: -1 = nao e cabecalho de material
long mat_hdr(const U& u, std::size_t p, const Lim& lim) {
    if (p + 2 > u.size()) return -1;
    const std::uint16_t a = u[p], b = u[p + 1];
    if ((a & 0xC000) != 0x8000 || (b & 0xC000) || (a & 0x3FFFu) >= lim.mats) return -1;
    std::uint32_t pg;
    std::size_t bb;
    std::vector<Sub> subs;
    if (!page_group(u, p + 2, lim, pg, bb, subs)) return -1;
    return a & 0x3FFF;
}

TreBatch make_batch(const U& u, std::uint32_t mat, std::uint32_t page, std::uint32_t k, std::size_t from, std::size_t count) {
    TreBatch b;
    b.material = mat; b.page = page; b.texture = k;
    b.idx.assign(u.begin() + static_cast<std::ptrdiff_t>(from), u.begin() + static_cast<std::ptrdiff_t>(from + count));
    return b;
}

// _parse_rest: true se fechou; partial (se nao nulo) recebe o que deu para ler mesmo sem fechar
bool parse_rest(const U& u, std::size_t p, const Lim& lim, std::vector<TreBatch>& out, std::vector<TreBatch>* partial = nullptr) {
    const std::size_t n = u.size();
    out.clear();
    std::uint32_t mat = 0, page = 0;
    auto fail = [&]() { if (partial) partial->insert(partial->end(), out.begin(), out.end()); return false; };
    while (n >= p + 3) {
        const long m = mat_hdr(u, p, lim);
        if (m >= 0) { mat = static_cast<std::uint32_t>(m); p += 2; }
        std::uint32_t pg;
        std::size_t b;
        std::vector<Sub> subs;
        if (page_group(u, p, lim, pg, b, subs)) {
            page = pg;
            for (const Sub& s : subs) out.push_back(make_batch(u, mat, page, s.k, s.from, s.count));
            p += 2 + b;
        } else {
            if (p + 2 > n) break;
            const std::uint16_t k = u[p], c = u[p + 1];
            if ((k & 0xC000) || (c & 0xC000) || c % 3 || k > lim.tga || p + 2 + c > n) return fail();
            if (lim.strict && (c & 1) && (p + 2 + c >= n || u[p + 2 + c] != 0)) return fail();
            out.push_back(make_batch(u, mat, page, k, p + 2, c));
            p += 2 + c + (c & 1);
        }
        if (p & 1) {
            if (lim.strict && (p >= n || u[p] != 0)) return fail();
            p += 1;
        }
    }
    if (partial) partial->insert(partial->end(), out.begin(), out.end());
    return n <= p + 1;
}

std::size_t total_idx(const std::vector<TreBatch>& v) { std::size_t s = 0; for (const TreBatch& b : v) s += b.idx.size(); return s; }

// split_full (v19)
bool split_full(const TreNode& nd, Lim lim, std::vector<TreBatch>& out) {
    if (!nd.has_head || (nd.head[0] & 0xC000) != 0x8000) return false;
    U full(nd.head, nd.head + 6);
    full.insert(full.end(), nd.indices.begin(), nd.indices.end());
    for (int strict = 1; strict >= 0; --strict) {
        lim.strict = strict != 0;
        if (!parse_rest(full, 0, lim, out)) continue;
        if (nd.tri_count && total_idx(out) != 3 * nd.tri_count) continue;
        return true;
    }
    return false;
}

// split_batches (v16, reserva)
bool split_batches(const U& u, std::size_t tri_count, Lim lim, std::vector<TreBatch>& out) {
    const std::size_t n = u.size(), want = 3 * tri_count;
    for (int strict = 1; strict >= 0; --strict) {
        lim.strict = strict != 0;
        for (std::size_t q = 0; q <= n; q += 2) {
            long l0 = -1;
            if (q % 3 == 0) l0 = static_cast<long>(q);
            else if (q >= 1 && (q - 1) % 3 == 0 && (q - 1) % 2 == 1) l0 = static_cast<long>(q - 1);
            if (l0 < 0) continue;
            if (lim.strict && static_cast<std::size_t>(l0) != q && u[q - 1] != 0) continue;
            if (q < n && !((u[q] & 0xC000) == 0x4000 || (u[q] & 0xC000) == 0x8000 ||
                           (q + 1 < n && (u[q] & 0xC000) == 0 && u[q] <= lim.tga && u[q + 1] % 3 == 0 && u[q + 1] > 0)))
                continue;
            std::vector<TreBatch> rest;
            if (!parse_rest(u, q, lim, rest)) continue;
            if (tri_count && static_cast<std::size_t>(l0) + total_idx(rest) != want) continue;
            out.clear();
            if (l0) out.push_back(make_batch(u, 0, 0, 0, 0, static_cast<std::size_t>(l0)));
            out.insert(out.end(), rest.begin(), rest.end());
            return true;
        }
    }
    // nao fechou: guarda o lote inicial ate o 1o cabecalho plausivel + o que der
    lim.strict = false;
    std::size_t first = n;
    for (std::size_t q = 0; q < n; q += 2) {
        if ((u[q] & 0xC000) == 0x4000 || (u[q] & 0xC000) == 0x8000) {
            std::uint32_t pg; std::size_t b; std::vector<Sub> subs;
            if (mat_hdr(u, q, lim) >= 0 || page_group(u, q, lim, pg, b, subs)) { first = q; break; }
        }
    }
    first -= first % 3;
    std::vector<TreBatch> partial, tmp;
    parse_rest(u, first + (first & 1), lim, tmp, &partial);
    out.clear();
    if (first) out.push_back(make_batch(u, 0, 0, 0, 0, first));
    out.insert(out.end(), partial.begin(), partial.end());
    return false;
}

// _read_node
bool read_node(const std::uint8_t* d, std::size_t size, std::size_t p, TreNode& nd, std::size_t& next, std::string& err) {
    if (p + 16 > size) { err = "no cortado em " + std::to_string(p); return false; }
    nd.leaf = std::memcmp(d + p, "LEAF", 4) == 0;
    const std::uint32_t declared = rd32(d + p + 4);
    std::size_t q = p + 8;
    if (std::memcmp(d + q, "INFO", 4) != 0) { err = "no em " + std::to_string(p) + " sem INFO"; return false; }
    const std::uint32_t ni = rd32(d + q + 4);
    if (q + 8 + static_cast<std::size_t>(ni) > size) { err = "INFO cortado"; return false; }
    const std::uint8_t* info = d + q + 8;
    q += 8 + ni;
    if (ni < 32) { err = "INFO de no pequeno em " + std::to_string(p); return false; }
    for (int i = 0; i < 6; ++i) nd.bbox[i] = rdf(info + 4 * i);
    nd.block = rd32(info + 24);
    bool have_flg = false;
    if (q + 8 <= size && std::memcmp(d + q, "IDXD", 4) == 0) {
        const std::uint32_t n = rd32(d + q + 4);
        const std::size_t end = q + 8 + n;
        std::size_t r = q + 8;
        if (end > size || r + 8 > size || std::memcmp(d + r, "IDXS", 4) != 0) { err = "IDXD sem IDXS em " + std::to_string(q); return false; }
        const std::uint32_t m = rd32(d + r + 4);
        if (m < 12 || r + 8 + m > end) { err = "IDXS invalido em " + std::to_string(r); return false; }
        const std::uint8_t* body = d + r + 8;
        for (int i = 0; i < 6; ++i) nd.head[i] = rd16(body + 2 * i);
        nd.has_head = true;
        const std::size_t cnt = (m - 12) / 2;
        nd.indices.resize(cnt);
        for (std::size_t i = 0; i < cnt; ++i) nd.indices[i] = rd16(body + 12 + 2 * i);
        r += 8 + m;
        if (r + 8 <= end && std::memcmp(d + r, "FLGS", 4) == 0) { nd.tri_count = rd32(d + r + 4); have_flg = true; }
        q = end;
    }
    if (!have_flg && q + 8 <= size && std::memcmp(d + q, "FLGS", 4) == 0) nd.tri_count = rd32(d + q + 4);
    next = p + 8 + declared + (nd.leaf ? 4 : 0);
    return true;
}

}  // namespace

bool TreData::bounds(float mn[3], float mx[3]) const {
    bool any = false;
    for (const MapBlock& b : blocks)
        for (std::size_t i = 0; i + 2 < b.xyz.size(); i += 3)
            for (int k = 0; k < 3; ++k) {
                const float v = b.xyz[i + k];
                if (!any || v < mn[k]) mn[k] = v;
                if (!any || v > mx[k]) mx[k] = v;
                if (k == 2) any = true;
            }
    return any;
}

bool read_tre(const std::uint8_t* d, std::size_t size, TreData& t, std::string& err) {
    t = TreData();
    if (!d || size < 16 || std::memcmp(d, "8TRE", 4) != 0) { err = "nao e um 8TRE"; return false; }
    const std::uint32_t n = rd32(d + 4);
    if (8 + static_cast<std::size_t>(n) > size) { err = "tamanho da raiz ultrapassa o arquivo"; return false; }
    std::vector<Chunk> top, sub;
    if (!chunks(d, 8, 8 + n, top, err)) return false;
    auto find = [&](const char* tag) -> const Chunk* { const Chunk* f = nullptr; for (const Chunk& c : top) if (!std::strcmp(c.tag, tag)) f = &c; return f; };
    const Chunk *ch = find("HEAD"), *cv = find("VTXD"), *ct = find("TREE");
    if (!ch || !cv || !ct) { err = "falta HEAD, VTXD ou TREE"; return false; }
    if (ch->size >= 16) for (int i = 0; i < 4; ++i) t.head[i] = rd32(d + ch->pos + 4 * i);
    if (const Chunk* c = find("LGTD")) {
        if (chunks(d, c->pos, c->pos + c->size, sub, err))
            for (const Chunk& s : sub) if (!std::strcmp(s.tag, "LGTR") && s.size >= 64) {
                TreLight l;
                l.type = rd32(d + s.pos); l.flags = rd32(d + s.pos + 4);
                for (int k = 0; k < 3; ++k) { l.color[k] = rdf(d + s.pos + 8 + 4 * k); l.pos[k] = rdf(d + s.pos + 20 + 4 * k); l.dir[k] = rdf(d + s.pos + 32 + 4 * k); }
                t.lights.push_back(l);
            }
    }
    if (const Chunk* c = find("LVLD")) {
        if (chunks(d, c->pos, c->pos + c->size, sub, err))
            for (const Chunk& s : sub) if (!std::strcmp(s.tag, "INFO") && s.size >= 16) {
                t.has_level = true; t.level_w = rd32(d + s.pos); t.level_h = rd32(d + s.pos + 4); t.level_f[0] = rdf(d + s.pos + 8); t.level_f[1] = rdf(d + s.pos + 12);
                break;
            }
    }
    if (const Chunk* c = find("MATD")) {
        if (chunks(d, c->pos, c->pos + c->size, sub, err))
            for (const Chunk& s : sub) if (!std::strcmp(s.tag, "MATR") && s.size >= 196) {
                const std::uint8_t* b = d + s.pos;
                MapMaterial m;
                m.id = rd32(b);
                m.name = cstr(b + 4, 80);
                for (int k = 0; k < 4; ++k) m.rgba[k] = b[84 + k];
                m.blend = cstr(b + 88, 16);
                m.surface = cstr(b + 104, 16);
                m.flags = rd32(b + s.size - 4);
                t.materials.push_back(m);
            }
    }
    if (const Chunk* c = find("TXTD")) {
        if (c->size >= 8 && chunks(d, c->pos + 8, c->pos + c->size, sub, err))
            for (const Chunk& s : sub) if (!std::strcmp(s.tag, "TXTR")) t.textures.push_back(cstr(d + s.pos, s.size));
    }
    // VTXD
    if (cv->size < 4) { err = "VTXD vazio"; return false; }
    const std::uint32_t nb = rd32(d + cv->pos);
    if (!chunks(d, cv->pos + 4, cv->pos + cv->size, sub, err)) return false;
    for (const Chunk& s : sub) {
        if (std::strcmp(s.tag, "VTXB")) continue;
        if (s.size < 4) { err = "VTXB cortado"; return false; }
        const std::uint32_t cnt = rd32(d + s.pos);
        std::size_t pos = s.pos + 4;
        struct St { std::size_t at = 0, len = 0; bool ok = false; } xyz, nrm, dif, uv, lm;
        while (pos + 8 <= s.pos + s.size) {
            const std::uint8_t* tok = d + pos;
            const std::uint32_t ln = rd32(d + pos + 4);
            if (pos + 8 + static_cast<std::size_t>(ln) > s.pos + s.size) { err = "fluxo ultrapassa o VTXB"; return false; }
            St st{pos + 8, ln, true};
            if (!std::memcmp(tok, "XYZ ", 4)) xyz = st;
            else if (!std::memcmp(tok, "NRML", 4)) nrm = st;
            else if (!std::memcmp(tok, "DIFU", 4)) dif = st;
            else if (!std::memcmp(tok, "TXUV", 4)) uv = st;
            else if (!std::memcmp(tok, "LMUV", 4)) lm = st;
            pos += 8 + ln;
        }
        if (!xyz.ok || xyz.len != 12ull * cnt) { err = "VTXB sem XYZ coerente"; return false; }
        MapBlock b;
        b.nv = cnt;
        auto floats = [&](const St& st, int per, std::vector<float>& out) {
            out.assign(static_cast<std::size_t>(per) * cnt, 0.f);         // ausente: zeros (como o build_f3map do Python)
            if (!st.ok || st.len != 4ull * per * cnt) return;
            for (std::size_t i = 0; i < out.size(); ++i) out[i] = rdf(d + st.at + 4 * i);
        };
        floats(xyz, 3, b.xyz);
        floats(nrm, 3, b.nrm);
        floats(uv, 2, b.uv);
        floats(lm, 2, b.lmuv);
        b.rgba.assign(4ull * cnt, 0xFF);
        if (dif.ok && dif.len == 4ull * cnt) std::memcpy(b.rgba.data(), d + dif.at, 4ull * cnt);
        t.blocks.push_back(std::move(b));
    }
    if (t.blocks.size() != nb) { err = "VTXD declara " + std::to_string(nb) + " blocos, achei " + std::to_string(t.blocks.size()); return false; }
    // TREE (sequencia plana ate o fim do arquivo)
    std::size_t q = ct->pos;
    while (q < size) {
        if (q + 4 > size || (std::memcmp(d + q, "NODE", 4) && std::memcmp(d + q, "LEAF", 4))) { err = "ficha inesperada em " + std::to_string(q); return false; }
        TreNode nd;
        std::size_t next = 0;
        if (!read_node(d, size, q, nd, next, err)) return false;
        if (next <= q) { err = "no sem progresso em " + std::to_string(q); return false; }
        t.nodes.push_back(std::move(nd));
        q = next;
    }
    if (q != size) { err = "arvore termina em " + std::to_string(q) + ", arquivo tem " + std::to_string(size); return false; }
    Lim lim{0, 0, 0, false};
    lim.mats = t.materials.empty() ? 1u : static_cast<std::uint32_t>(t.materials.size());
    for (const std::string& x : t.textures) {
        std::string l = x;
        for (char& c : l) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (l.size() >= 4 && l.compare(l.size() - 4, 4, ".ctx") == 0) ++lim.ctx;
        if (l.size() >= 4 && l.compare(l.size() - 4, 4, ".tga") == 0) ++lim.tga;
    }
    for (TreNode& nd : t.nodes) {
        if (nd.indices.empty()) continue;
        if (split_full(nd, lim, nd.batches)) { nd.parsed = true; nd.full_grammar = true; }
        else nd.parsed = split_batches(nd.indices, nd.tri_count, lim, nd.batches);
    }
    return true;
}

void tre_triangle_lists(const TreData& t, std::vector<MapTriList>& out, TreStats* stats, float eps) {
    TreStats st;
    std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>, std::vector<std::uint32_t>> per;
    for (const TreNode& nd : t.nodes) {
        if (nd.indices.empty()) continue;
        ++st.nodes;
        if (nd.full_grammar) ++st.nodes_full;
        if (!nd.parsed) ++st.unparsed;
        if (nd.block >= t.blocks.size()) { st.bad += nd.indices.size(); continue; }
        const MapBlock& blk = t.blocks[nd.block];
        const std::uint32_t n = blk.nv;
        const float* bb = nd.bbox;
        for (const TreBatch& ba : nd.batches) {
            std::vector<std::uint32_t>& dst = per[{nd.block, ba.material, ba.page, ba.texture}];
            for (std::size_t i = 0; i + 2 < ba.idx.size(); i += 3) {
                for (int j = 0; j < 3; ++j) {
                    const std::uint32_t x = ba.idx[i + static_cast<std::size_t>(j)];
                    long pick = -1;
                    const std::uint32_t cand[4] = {x, x & 0x3FFFu, x & 0x7FFFu, x & 0xBFFFu};
                    for (std::uint32_t c : cand) {
                        if (c < n) {
                            const float px = blk.xyz[3 * c], py = blk.xyz[3 * c + 1], pz = blk.xyz[3 * c + 2];
                            if (bb[0] - eps <= px && px <= bb[3] + eps && bb[1] - eps <= py && py <= bb[4] + eps && bb[2] - eps <= pz && pz <= bb[5] + eps) { pick = c; break; }
                        }
                    }
                    ++st.indices;
                    if (pick < 0) {
                        ++st.outside_bbox;
                        pick = x < n ? x : (x & 0x3FFFu);
                        if (static_cast<std::uint32_t>(pick) >= n) { ++st.bad; pick = 0; }
                    } else if (static_cast<std::uint32_t>(pick) != x) ++st.masked;
                    dst.push_back(static_cast<std::uint32_t>(pick));
                }
            }
        }
    }
    out.clear();
    for (auto& kv : per) {
        if (kv.second.empty()) continue;
        MapTriList tl;
        tl.block = std::get<0>(kv.first); tl.material = std::get<1>(kv.first); tl.page = std::get<2>(kv.first); tl.tex = std::get<3>(kv.first);
        tl.idx = std::move(kv.second);
        out.push_back(std::move(tl));
    }
    if (stats) *stats = st;
}

}  // namespace f3v
