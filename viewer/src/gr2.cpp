// v20: leitor de Granny 2 em C++ (ver gr2.h). Portado do parser MIT "blendergranny" (third_party/blendergranny):
// decompress/oodle0.py, fixup.py, types.py, skeleton.py e animation.py. A logica e a mesma, linha por linha onde importa
// (o descompressor e aritmetico: qualquer diferenca de largura de inteiro muda tudo; por isso uso 64 bits onde o Python
// tem inteiros sem limite e mascaro como ele mascara).
#include "gr2.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace f3v {

namespace {

constexpr std::uint32_t kMask31 = 0x7FFFFFFFu;
constexpr std::uint64_t kMask32 = 0xFFFFFFFFull;

std::uint32_t rd32(const std::uint8_t* p) { return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
float rdf(const std::uint8_t* p) { const std::uint32_t u = rd32(p); float f; std::memcpy(&f, &u, 4); return f; }

std::uint32_t bit_reverse(std::uint32_t v, int nbits) {
    std::uint32_t r = 0;
    for (int i = 0; i < nbits; ++i) r = (r << 1) | ((v >> i) & 1u);
    return r;
}

// ------------------------------------------------------------------------------------------------ Oodle0
struct VarBits {
    const std::uint8_t* data; std::size_t size; std::size_t cur;
    std::uint64_t bits = 0; int bitlen = 0;
    std::uint32_t word() {
        std::uint8_t b[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; ++i) if (cur + static_cast<std::size_t>(i) < size) b[i] = data[cur + static_cast<std::size_t>(i)];
        cur += 4;
        return rd32(b);
    }
    std::uint32_t get(int n) {
        if (n == 0) return 0;
        const std::uint64_t mask = (1ull << n) - 1;
        if (bitlen >= n) { const std::uint32_t v = static_cast<std::uint32_t>(bits & mask); bits >>= n; bitlen -= n; return v; }
        const std::uint64_t w = word();
        const std::uint32_t v = static_cast<std::uint32_t>((bits | (w << bitlen)) & mask);
        bits = w >> (n - bitlen);
        bitlen = bitlen + 32 - n;
        return v;
    }
    std::uint32_t get1() {
        if (bitlen) { const std::uint32_t v = static_cast<std::uint32_t>(bits & 1u); bits >>= 1; --bitlen; return v; }
        const std::uint32_t w = word();
        bits = w >> 1; bitlen = 31;
        return w & 1u;
    }
};

struct ArithBits {
    VarBits vb;
    std::uint32_t high = kMask31, low = 0, code = 0;
    ArithBits(const std::uint8_t* d, std::size_t n, std::size_t off) : vb{d, n, off} { code = bit_reverse(vb.get(31), 31); }
    std::uint32_t get_count(std::uint32_t scale) const {
        if (scale == 0) return 0;
        const std::int64_t num = (static_cast<std::int64_t>(code) - static_cast<std::int64_t>(low) + 1) * static_cast<std::int64_t>(scale) - 1;
        const std::int64_t den = static_cast<std::int64_t>(high) - static_cast<std::int64_t>(low) + 1;
        if (den <= 0) return 0;
        std::int64_t q = num / den;
        if (num < 0 && num % den) --q;                   // divisao inteira do Python arredonda para baixo
        return q < 0 ? 0u : static_cast<std::uint32_t>(q);
    }
    std::uint32_t get_value(std::uint32_t scale) {
        std::uint32_t v = get_count(scale);
        if (v >= scale) v = scale - 1;
        remove(v, 1, scale);
        return v;
    }
    void remove(std::uint32_t start, std::uint32_t count, std::uint32_t scale) {
        if (scale == 0) return;
        std::uint64_t h = high, l = low, c = code;
        const std::uint64_t width = (h - l) + 1;
        h = (l + (width * (start + count)) / scale - 1) & kMask32;
        l = (l + (width * start) / scale) & kMask32;
        if (((h ^ l) & 0x40000000ull) == 0) {
            while (((h ^ l) & 0x7F800000ull) == 0) {
                l = (l << 8) & kMask32;
                h = ((h << 8) | 0xFF) & kMask32;
                const std::uint32_t b = vb.get(8);
                c = ((c << 8) | (bit_reverse(b & 0xF, 4) << 4) | bit_reverse(b >> 4, 4)) & kMask32;
            }
            if (((h ^ l) & 0x78000000ull) == 0) {
                l = (l << 4) & kMask32;
                h = ((h << 4) | 0xF) & kMask32;
                c = ((c << 4) | bit_reverse(vb.get(4), 4)) & kMask32;
            }
            while (((h ^ l) & 0x40000000ull) == 0) {
                l = (l << 1) & kMask32;
                h = ((h << 1) | 1) & kMask32;
                c = ((c << 1) | vb.get1()) & kMask32;
            }
        }
        while ((l & 0x20000000ull) && !(h & 0x20000000ull)) {
            c = (c ^ 0x20000000ull) & kMask32;
            l = ((l & 0x1FFFFFFFull) << 1) & kMask32;
            h = ((h << 1) | 0x40000001ull) & kMask32;
            c = ((c << 1) | vb.get1()) & kMask32;
        }
        high = static_cast<std::uint32_t>(h & kMask31);
        low = static_cast<std::uint32_t>(l & kMask31);
        code = static_cast<std::uint32_t>(c & kMask31);
    }
};

void best_shift(int value, int& bin_size, int& bin_shift, int& last_bin_start) {
    if (value < 6) { bin_size = 0; bin_shift = 15; last_bin_start = 0; return; }
    std::int64_t best_max = static_cast<std::int64_t>(kMask32);
    int best_bin = 0;
    for (int i = 0; i < 16; ++i) {
        const std::int64_t size = 1ll << i;
        const std::int64_t bins = std::min<std::int64_t>((value + size - 1) / size, 16);
        std::int64_t last = value - size * (bins - 1);
        if (last < size) last = size;
        if (last < best_max) { best_bin = i; best_max = last; }
        if (size > value) break;
    }
    bin_size = 1 << best_bin; bin_shift = best_bin; last_bin_start = 15 * (1 << best_bin);
}

struct ArithModel {
    int unique_values = 0;
    std::uint16_t totals[16] = {0};
    std::vector<std::uint16_t> counts, values;
    int number = 0;
    int bin_size = 0, bin_shift = 0, last_bin_start = 0;
    bool failed = false;
    explicit ArithModel(int uv = 0) : unique_values(uv) {
        const int count = (uv + 5) & ~3;
        counts.assign(static_cast<std::size_t>(count), 0);
        values.assign(static_cast<std::size_t>(count), 0);
        best_shift(uv + 1, bin_size, bin_shift, last_bin_start);
        quick_increment(0, 0x30003);
    }
    void add_pair(int pair, std::uint64_t amount) {
        const int i = pair * 2;
        const std::uint64_t old = static_cast<std::uint64_t>(totals[i]) | (static_cast<std::uint64_t>(totals[i + 1]) << 16);
        const std::uint64_t nw = (old + amount) & kMask32;
        totals[i] = static_cast<std::uint16_t>(nw & 0xFFFF);
        totals[i + 1] = static_cast<std::uint16_t>((nw >> 16) & 0xFFFF);
    }
    void increment_totals(int value, std::uint64_t amount) {
        amount &= kMask32;
        const std::uint16_t lo = static_cast<std::uint16_t>(amount & 0xFFFF);
        if (value >= last_bin_start) { totals[15] = static_cast<std::uint16_t>(totals[15] + lo); return; }
        int bin = value >> bin_shift;
        if (bin & 1) { totals[bin] = static_cast<std::uint16_t>(totals[bin] + lo); ++bin; }
        for (int p = bin >> 1; p < 8; ++p) add_pair(p, amount);
    }
    void quick_increment(int value, std::uint64_t amount) {
        if (value < 0 || static_cast<std::size_t>(value) >= counts.size()) { failed = true; return; }
        increment_totals(value, amount);
        counts[static_cast<std::size_t>(value)] = static_cast<std::uint16_t>(counts[static_cast<std::size_t>(value)] + (amount & 0xFFFF));
    }
    void decrement_counts(int value, std::uint32_t amount) {
        const std::uint64_t neg = (0ull - amount) & kMask32;
        const std::uint64_t packed = (((neg - 1) & kMask32) << 16) | (neg & 0xFFFF);
        quick_increment(value, packed);
    }
    int bin_of(int index) const { return index < last_bin_start ? (index >> bin_shift) : 15; }
    void rescale() {
        best_shift(number + 1, bin_size, bin_shift, last_bin_start);
        std::int64_t bins[16] = {0};
        counts[0] >>= 1;
        bins[0 < last_bin_start ? 0 : 15] += counts[0];
        int max_count = 0, max_pos = 0, index = 1;
        bool done = false;
        while (index <= number && !done) {
            while (counts[static_cast<std::size_t>(index)] <= 1) {
                if (index < number) {
                    counts[static_cast<std::size_t>(index)] = counts[static_cast<std::size_t>(number)];
                    values[static_cast<std::size_t>(index)] = values[static_cast<std::size_t>(number)];
                    counts[static_cast<std::size_t>(number)] = 0;
                    --number;
                } else {
                    counts[static_cast<std::size_t>(index)] = 0;
                    --number;
                    done = true;
                    break;
                }
            }
            if (done) break;
            counts[static_cast<std::size_t>(index)] >>= 1;
            if (counts[static_cast<std::size_t>(index)] > max_count) { max_count = counts[static_cast<std::size_t>(index)]; max_pos = index; }
            bins[bin_of(index)] += counts[static_cast<std::size_t>(index)];
            ++index;
        }
        if (max_count) {
            int swap_pos = number < last_bin_start ? ((number >> bin_shift) << bin_shift) : last_bin_start;
            if (swap_pos == 0) swap_pos = 1;
            if (max_pos != swap_pos) {
                const int old_count = counts[static_cast<std::size_t>(swap_pos)];
                counts[static_cast<std::size_t>(swap_pos)] = counts[static_cast<std::size_t>(max_pos)];
                bins[bin_of(swap_pos)] += -old_count + counts[static_cast<std::size_t>(swap_pos)];
                bins[bin_of(max_pos)] += old_count - counts[static_cast<std::size_t>(swap_pos)];
                counts[static_cast<std::size_t>(max_pos)] = static_cast<std::uint16_t>(old_count);
                std::swap(values[static_cast<std::size_t>(swap_pos)], values[static_cast<std::size_t>(max_pos)]);
            }
        }
        if (number != unique_values && counts[0] == 0) {
            counts[0] = static_cast<std::uint16_t>(counts[0] + 2);
            bins[0 < last_bin_start ? 0 : 15] += 2;
        }
        std::int64_t running = 0;
        for (int i = 0; i < 16; ++i) { running += bins[i]; totals[i] = static_cast<std::uint16_t>(running & 0xFFFF); }
    }
    // devolve o valor; escape = true quando o simbolo e novo (o chamador le o valor e chama set_escaped)
    std::uint32_t decompress(ArithBits& b, bool& escape, int& escape_index) {
        escape = false;
        if (totals[15] >= 16384) rescale();
        const std::uint32_t scale = totals[15];
        const std::uint32_t count = b.get_count(scale);
        std::uint32_t start = 0;
        std::size_t pos = 0;
        bool found = false;
        for (std::size_t i = 0; i < counts.size(); ++i) {
            if (count < start + counts[i]) { pos = i; found = true; break; }
            start += counts[i];
        }
        if (!found) { failed = true; return 0; }
        const std::uint32_t old_count = counts[pos];
        increment_totals(static_cast<int>(pos), 0x10001);
        b.remove(start, old_count, static_cast<std::uint32_t>(totals[15]) - 1u);
        counts[pos] = static_cast<std::uint16_t>(counts[pos] + 1);
        if (pos == 0) {
            ++number;
            if (static_cast<std::size_t>(number) >= counts.size()) { failed = true; return 0; }
            quick_increment(number, 0x20002);
            if (number == unique_values) decrement_counts(0, counts[0]);
            escape = true; escape_index = number;
            return 0;
        }
        return values[pos];
    }
};

std::uint32_t read_symbol(ArithModel& m, ArithBits& b, std::uint32_t escape_scale) {
    bool esc = false; int idx = 0;
    const std::uint32_t v = m.decompress(b, esc, idx);
    if (esc) {
        const std::uint32_t e = b.get_value(escape_scale);
        m.values[static_cast<std::size_t>(idx)] = static_cast<std::uint16_t>(e);
        return e;
    }
    return v;
}

}  // namespace

bool oodle0_decompress(const std::uint8_t* src, std::size_t n, std::size_t expanded, std::size_t first16, std::size_t first8,
                       std::vector<std::uint8_t>& out, std::string& err) {
    out.clear();
    if (expanded == 0) return true;
    if (!src || n < 36) { err = "oodle0: dados menores que os 3 cabecalhos de bloco"; return false; }
    if (expanded > (256u << 20)) { err = "oodle0: tamanho expandido absurdo"; return false; }
    std::uint32_t w[9];
    for (int i = 0; i < 9; ++i) w[i] = rd32(src + 4 * i);
    auto clamp_stop = [&](std::size_t v) { return v == 0 ? std::size_t(0) : std::min(v, expanded); };
    std::size_t stops[4] = {0, clamp_stop(first16), clamp_stop(first8), expanded};
    if (stops[2] < stops[1]) stops[2] = stops[1];
    out.reserve(expanded);
    ArithBits bits(src, n, 36);
    static const std::uint32_t kLong[4] = {128, 192, 256, 512};
    for (int blk = 0; blk < 3; ++blk) {
        if (stops[blk + 1] <= stops[blk]) continue;
        const std::uint32_t h0 = w[blk * 3], h1 = w[blk * 3 + 1], h2 = w[blk * 3 + 2];
        const std::uint32_t max_bytes = h0 & 0x1FF, max_offsets = h0 >> 9;
        const std::uint32_t max_offset_low = std::min<std::uint32_t>(max_offsets, 4);
        ArithModel bytes_m(static_cast<int>(h1 & 0x1FF));
        std::vector<ArithModel> lengths;
        lengths.reserve(65);
        for (int i = 0; i <= 64; ++i) {
            const int group = std::min(i / 16, 3);
            lengths.emplace_back(static_cast<int>((h2 >> ((3 - group) * 8)) & 0xFF));
        }
        ArithModel off_low(static_cast<int>(max_offset_low));
        ArithModel off_high(static_cast<int>(h1 >> 9));
        std::uint64_t done_bytes = 0;
        std::uint32_t last_len = 0;
        const std::size_t stop = stops[blk + 1];
        while (out.size() < stop) {
            const std::uint32_t ls = read_symbol(lengths[last_len], bits, 65);
            if (ls > 64) { err = "oodle0: simbolo de comprimento invalido"; return false; }
            last_len = ls;
            if (ls) {
                const std::uint32_t len = ls >= 61 ? kLong[ls - 61] : ls + 1;
                const std::uint32_t lo = read_symbol(off_low, bits, max_offset_low);
                const std::uint32_t hs = static_cast<std::uint32_t>((std::min<std::uint64_t>(max_offsets, done_bytes) >> 2) + 1);
                const std::uint32_t hi = read_symbol(off_high, bits, hs);
                const std::size_t dist = static_cast<std::size_t>(lo) + 1 + (static_cast<std::size_t>(hi) << 2);
                if (dist == 0 || dist > out.size()) { err = "oodle0: distancia de copia invalida"; return false; }
                if (out.size() + len > expanded + 1024) { err = "oodle0: passou do tamanho esperado"; return false; }
                for (std::uint32_t k = 0; k < len; ++k) out.push_back(out[out.size() - dist]);
                done_bytes += len;
            } else {
                const std::uint32_t lit = read_symbol(bytes_m, bits, max_bytes);
                if (lit > 255) { err = "oodle0: literal invalido"; return false; }
                out.push_back(static_cast<std::uint8_t>(lit));
                ++done_bytes;
            }
            if (bytes_m.failed || off_low.failed || off_high.failed || lengths[last_len].failed) { err = "oodle0: modelo aritmetico inconsistente"; return false; }
        }
    }
    if (out.size() != expanded) { err = "oodle0: descomprimiu " + std::to_string(out.size()) + " bytes, esperado " + std::to_string(expanded); return false; }
    return true;
}

// ------------------------------------------------------------------------------------------------ arquivo, secoes e ponteiros
namespace {

enum : std::uint32_t {
    MT_END = 0, MT_INLINE = 1, MT_REFERENCE = 2, MT_REFERENCE_TO_ARRAY = 3, MT_ARRAY_OF_REFERENCES = 4, MT_VARIANT_REFERENCE = 5,
    MT_REFERENCE_TO_VARIANT_ARRAY = 7, MT_STRING = 8, MT_TRANSFORM = 9, MT_REAL32 = 10, MT_INT8 = 11, MT_UINT8 = 12, MT_BINORMAL_INT8 = 13,
    MT_NORMAL_UINT8 = 14, MT_INT16 = 15, MT_UINT16 = 16, MT_BINORMAL_INT16 = 17, MT_NORMAL_UINT16 = 18, MT_INT32 = 19, MT_UINT32 = 20,
    MT_REAL16 = 21, MT_EMPTY_REFERENCE = 22
};

struct Ref { int sec = -1; std::uint32_t off = 0; bool ok() const { return sec >= 0; } };

struct Member { std::uint32_t type = 0; std::string name; Ref ref; std::uint32_t width = 1; };

struct Loaded {
    int version = 0;
    std::vector<std::vector<std::uint8_t>> sec;                  // secoes descomprimidas (os ponteiros ficam como estao)
    std::unordered_map<std::uint64_t, Ref> fix;                  // (secao << 32 | offset do ponteiro) -> alvo
    std::unordered_map<std::uint64_t, std::vector<Member>> type_cache;
    std::unordered_map<std::uint64_t, std::uint32_t> size_cache;

    static std::uint64_t key(const Ref& r) { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(r.sec)) << 32) | r.off; }
    bool has(const Ref& r, std::size_t n) const { return r.sec >= 0 && static_cast<std::size_t>(r.sec) < sec.size() && r.off + n <= sec[static_cast<std::size_t>(r.sec)].size(); }
    const std::uint8_t* at(const Ref& r) const { return sec[static_cast<std::size_t>(r.sec)].data() + r.off; }
    std::uint32_t u32(const Ref& r) const { return has(r, 4) ? rd32(at(r)) : 0; }
    std::int32_t i32(const Ref& r) const { return static_cast<std::int32_t>(u32(r)); }
    float f32(const Ref& r) const { return has(r, 4) ? rdf(at(r)) : 0.f; }
    Ref ptr(const Ref& r) const { const auto it = fix.find(key(r)); return it == fix.end() ? Ref{} : it->second; }
    static Ref plus(Ref r, std::uint32_t d) { r.off += d; return r; }

    std::string str_at(const Ref& r) const {
        if (!r.ok() || static_cast<std::size_t>(r.sec) >= sec.size()) return "";
        const std::vector<std::uint8_t>& d = sec[static_cast<std::size_t>(r.sec)];
        if (r.off >= d.size()) return "";
        auto looks_text = [](const std::uint8_t* p, std::size_t n) {
            if (!n) return false;
            for (std::size_t i = 0; i < n; ++i) { const std::uint8_t c = p[i]; if (c == 9 || c == 10 || c == 13) continue; if (c < 32 || c == 127) return false; }
            return true;
        };
        if (r.off + 4 <= d.size()) {
            const std::uint32_t len = rd32(d.data() + r.off);
            if (len > 0 && len <= 1024 && r.off + 4 + len <= d.size() && looks_text(d.data() + r.off + 4, len)) {
                std::string s(reinterpret_cast<const char*>(d.data() + r.off + 4), len);
                while (!s.empty() && s.back() == '\0') s.pop_back();
                return s;
            }
        }
        std::size_t e = r.off;
        const std::size_t lim = std::min<std::size_t>(d.size(), r.off + 1024);
        while (e < lim && d[e] != 0) ++e;
        if (!looks_text(d.data() + r.off, e - r.off)) return "";
        return std::string(reinterpret_cast<const char*>(d.data() + r.off), e - r.off);
    }
    std::string str_ptr(const Ref& slot) const { return str_at(ptr(slot)); }

    const std::vector<Member>& members(const Ref& type) {
        const std::uint64_t k = key(type);
        auto it = type_cache.find(k);
        if (it != type_cache.end()) return it->second;
        std::vector<Member> v;
        Ref r = type;
        for (int i = 0; i < 512 && has(r, 32); ++i) {
            const std::uint32_t mt = u32(r);
            if (mt == MT_END) break;
            Member m;
            m.type = mt;
            m.name = str_ptr(plus(r, 4));
            if (m.name.empty()) m.name = "member_" + std::to_string(i);
            m.ref = ptr(plus(r, 8));
            m.width = u32(plus(r, 12));
            if (!m.width) m.width = 1;
            v.push_back(std::move(m));
            r.off += 32;
        }
        return type_cache.emplace(k, std::move(v)).first->second;
    }
    std::uint32_t member_size(const Member& m, int depth = 0) {
        switch (m.type) {
            case MT_REAL32: case MT_INT32: case MT_UINT32: return 4 * m.width;
            case MT_INT8: case MT_UINT8: case MT_BINORMAL_INT8: case MT_NORMAL_UINT8: return 1 * m.width;
            case MT_INT16: case MT_UINT16: case MT_BINORMAL_INT16: case MT_NORMAL_UINT16: case MT_REAL16: return 2 * m.width;
            case MT_REFERENCE: case MT_EMPTY_REFERENCE: case MT_STRING: return 4;
            case MT_REFERENCE_TO_ARRAY: case MT_ARRAY_OF_REFERENCES: return 8;
            case MT_VARIANT_REFERENCE: return 8;
            case MT_REFERENCE_TO_VARIANT_ARRAY: return 12;
            case MT_TRANSFORM: return 68;
            case MT_INLINE: return m.ref.ok() && depth < 16 ? type_size(m.ref, depth + 1) * m.width : 0;
            default: return 4;
        }
    }
    std::uint32_t type_size(const Ref& type, int depth = 0) {
        const std::uint64_t k = key(type);
        const auto it = size_cache.find(k);
        if (it != size_cache.end()) return it->second;
        std::uint32_t s = 0;
        const std::vector<Member> ms = members(type);             // copia: members() pode crescer o cache
        for (const Member& m : ms) s += member_size(m, depth);
        size_cache[k] = s;
        return s;
    }
    // offset de um membro pelo nome (-1 se nao existir); devolve tambem o membro
    long offset_of(const Ref& type, const char* name, Member* out = nullptr) {
        std::uint32_t o = 0;
        const std::vector<Member> ms = members(type);
        for (const Member& m : ms) {
            if (m.name == name) { if (out) *out = m; return static_cast<long>(o); }
            o += member_size(m);
        }
        return -1;
    }
};

std::uint64_t fkey(int s, std::uint32_t o) { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(s)) << 32) | o; }

struct SectionHdr { std::uint32_t comp, data_off, data_size, expanded, align, first16, first8, fix_off, fix_count, mm_off, mm_count; };

bool load_sections(const std::uint8_t* d, std::size_t n, Loaded& L, Ref& root_type, Ref& root_obj, std::string& err) {
    static const std::uint32_t kOld[4] = {0xCAB067B8u, 0x0FB16DF8u, 0x7E8C7284u, 0x1E00195Eu};
    static const std::uint32_t k32le[4] = {0xC06CDE29u, 0x2B53A4BAu, 0xA5B7F525u, 0xEEE266F6u};
    if (n < 32 + 56) { err = "gr2: arquivo pequeno demais"; return false; }
    bool ok_old = true, ok_le = true;
    for (int i = 0; i < 4; ++i) { if (rd32(d + 4 * i) != kOld[i]) ok_old = false; if (rd32(d + 4 * i) != k32le[i]) ok_le = false; }
    if (!ok_old && !ok_le) { err = "gr2: assinatura nao reconhecida (so 32 bits little-endian e suportado)"; return false; }
    const std::uint8_t* h = d + 32;
    L.version = static_cast<int>(rd32(h));
    const std::uint32_t sec_off = rd32(h + 12), nsec = rd32(h + 16);
    root_type = Ref{static_cast<int>(rd32(h + 20)), rd32(h + 24)};
    root_obj = Ref{static_cast<int>(rd32(h + 28)), rd32(h + 32)};
    if (nsec == 0 || nsec > 64) { err = "gr2: numero de secoes absurdo"; return false; }
    std::vector<SectionHdr> sh(nsec);
    for (std::uint32_t i = 0; i < nsec; ++i) {
        const std::size_t o = 32 + static_cast<std::size_t>(sec_off) + 44u * i;
        if (o + 44 > n) { err = "gr2: tabela de secoes truncada"; return false; }
        std::uint32_t v[11];
        for (int k = 0; k < 11; ++k) v[k] = rd32(d + o + 4 * k);
        sh[i] = SectionHdr{v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]};
    }
    L.sec.resize(nsec);
    for (std::uint32_t i = 0; i < nsec; ++i) {
        const SectionHdr& s = sh[i];
        if (s.expanded == 0) continue;
        if (static_cast<std::size_t>(s.data_off) + s.data_size > n) { err = "gr2: secao " + std::to_string(i) + " fora do arquivo"; return false; }
        const std::uint8_t* src = d + s.data_off;
        if (s.comp == 0) {
            if (s.data_size < s.expanded) { err = "gr2: secao " + std::to_string(i) + " curta"; return false; }
            L.sec[i].assign(src, src + s.expanded);
        } else if (s.comp == 1) {
            std::string e;
            if (!oodle0_decompress(src, s.data_size, s.expanded, s.first16, s.first8, L.sec[i], e)) { err = "gr2: secao " + std::to_string(i) + ": " + e; return false; }
        } else {
            err = "gr2: compressao " + std::to_string(s.comp) + " nao suportada (a demo so usa Oodle0)";
            return false;
        }
    }
    // span ate o proximo bloco de metadados (mesma heuristica do parser Python)
    auto span_after = [&](std::uint32_t off) -> long {
        long best = -1;
        for (const SectionHdr& s : sh) for (std::uint32_t v : {s.fix_off, s.mm_off, s.data_off}) if (v > off && (best < 0 || static_cast<long>(v - off) < best)) best = static_cast<long>(v - off);
        return best;
    };
    for (std::uint32_t i = 0; i < nsec; ++i) {
        const SectionHdr& s = sh[i];
        if (s.fix_count == 0 || s.fix_off == 0) continue;
        const long span = span_after(s.fix_off);
        std::uint32_t es = 0;
        if (span >= 0 && static_cast<std::uint64_t>(span) >= static_cast<std::uint64_t>(s.fix_count) * 12) es = 12;
        else if (span >= 0 && static_cast<std::uint64_t>(span) >= static_cast<std::uint64_t>(s.fix_count) * 8) es = 8;
        else if (L.version >= 7) es = 12;
        else es = static_cast<std::uint64_t>(s.fix_off) + static_cast<std::uint64_t>(s.fix_count) * 12 <= n ? 12 : 8;
        const std::size_t expected = static_cast<std::size_t>(s.fix_count) * es;
        // blob: igual ao Python (prefixo de tamanho so se bater exatamente; senao os bytes crus)
        const std::size_t raw_len = span >= 0 ? std::min<std::size_t>(static_cast<std::size_t>(span), n - s.fix_off) : n - s.fix_off;
        const std::uint8_t* raw = d + s.fix_off;
        const std::uint8_t* blob = raw;
        std::size_t blob_len = raw_len;
        if (raw_len >= 4) {
            const std::uint32_t psz = rd32(raw);
            const std::size_t plen = std::min<std::size_t>(psz, raw_len - 4);
            if (plen == expected) { blob = raw + 4; blob_len = plen; }
            else if (raw_len >= expected) blob_len = expected;
        }
        for (std::uint32_t k = 0; k < s.fix_count; ++k) {
            const std::size_t o = static_cast<std::size_t>(k) * es;
            if (o + es > blob_len) break;
            const std::uint32_t src_off = rd32(blob + o);
            Ref tgt;
            if (es == 12) tgt = Ref{static_cast<int>(rd32(blob + o + 4)), rd32(blob + o + 8)};
            else {
                const std::uint32_t p = rd32(blob + o + 4);
                const std::uint32_t base = 0x10000000u, stride = 0x100000u;
                if (p >= base && (p - base) / stride < nsec) tgt = Ref{static_cast<int>((p - base) / stride), (p - base) % stride};
                else tgt = Ref{static_cast<int>(p / stride), p % stride};
            }
            if (tgt.sec < 0 || static_cast<std::uint32_t>(tgt.sec) >= nsec) continue;
            L.fix[fkey(static_cast<int>(i), src_off)] = tgt;
        }
    }
    return true;
}

// --------------------------------------------------------------------------------------------- curvas (LegacyCurve32f)
bool read_legacy_curve(Loaded& L, const Ref& at, Gr2Curve& c) {
    c = Gr2Curve();
    if (!L.has(at, 20)) return false;
    const std::int32_t degree = L.i32(at);
    if (degree < 0 || degree > 3) return false;
    const std::uint32_t kn = L.u32(Loaded::plus(at, 4)), cn = L.u32(Loaded::plus(at, 12));
    if (kn == 0 && cn == 0) return false;
    if (kn == 0 || cn == 0 || cn % kn) return false;
    const std::uint32_t dim = cn / kn;
    if (dim == 0 || dim > 16) return false;
    const Ref kr = L.ptr(Loaded::plus(at, 8)), cr = L.ptr(Loaded::plus(at, 16));
    if (!L.has(kr, 4ull * kn) || !L.has(cr, 4ull * cn)) return false;
    c.present = true;
    c.degree = degree;
    c.dim = static_cast<int>(dim);
    c.knots.resize(kn);
    c.controls.resize(cn);
    for (std::uint32_t i = 0; i < kn; ++i) c.knots[i] = rdf(L.at(kr) + 4 * i);
    for (std::uint32_t i = 0; i < cn; ++i) c.controls[i] = rdf(L.at(cr) + 4 * i);
    return true;
}

}  // namespace

void Gr2Curve::eval(double t, double* out) const {
    for (int k = 0; k < dim; ++k) out[k] = 0.0;
    if (!present || knots.empty() || dim <= 0) return;
    const int nk = static_cast<int>(knots.size()), nc = static_cast<int>(controls.size()) / dim;
    auto ctrl = [&](int i) { return &controls[static_cast<std::size_t>(std::min(std::max(i, 0), nc - 1)) * static_cast<std::size_t>(dim)]; };
    auto find_knot = [&](double tt) { for (int i = 0; i < nk; ++i) if (static_cast<double>(knots[static_cast<std::size_t>(i)]) > tt) return i; return std::max(0, nk - 1); };
    auto normalize = [&](double* v) {
        double l2 = 0;
        for (int k = 0; k < dim; ++k) l2 += v[k] * v[k];
        if (l2 <= 0) return;
        const double inv = std::pow(l2, -0.5);         // o Python faz length_sq ** -0.5
        for (int k = 0; k < dim; ++k) v[k] = v[k] * inv;
    };
    const int deg = std::max(0, degree);
    if (deg == 0 || nk == 1) { const float* p = ctrl(std::min(find_knot(t), nc - 1)); for (int k = 0; k < dim; ++k) out[k] = p[k]; return; }
    const int ki = std::min(find_knot(t), nk - 1);
    const int base = ki - deg, window = 2 * deg;
    double ti[6];
    const float* pi[6];
    for (int l = 0; l < window; ++l) {
        const int s = std::min(std::max(base + l, 0), nk - 1);
        ti[l] = knots[static_cast<std::size_t>(s)];
        pi[l] = ctrl(std::min(s, nc - 1));
    }
    auto sdiv = [](double a, double b) { return std::fabs(b) < 1e-12 ? 0.0 : a / b; };
    const int c = deg;
    if (deg == 1) {
        const double bl = sdiv(t - ti[c - 1], ti[c] - ti[c - 1]);
        const double cp = 1.0 - bl;
        for (int k = 0; k < dim; ++k) out[k] = cp * pi[c - 1][k] + bl * pi[c][k];
    } else if (deg == 2) {
        const double t2 = ti[c - 2], t1 = ti[c - 1], t0 = ti[c], tp = ti[c + 1];
        const double l0 = sdiv(t - t1, t0 - t1), l11 = sdiv(t - t2, t0 - t2), l12 = sdiv(t - t1, tp - t1);
        const double s21 = (l11 + l0) - l0 * l11, ci = l0 * l12, ci1 = s21 - ci, ci2 = 1.0 - s21;
        for (int k = 0; k < dim; ++k) out[k] = ci2 * pi[c - 2][k] + ci1 * pi[c - 1][k] + ci * pi[c][k];
    } else {
        for (int k = 0; k < dim; ++k) out[k] = pi[c][k];
    }
    if (dim == 4) { normalize(out); normalize(out); }
}

void Gr2Curve::eval(float t, float* out) const {
    double d[16];
    eval(static_cast<double>(t), d);
    for (int k = 0; k < dim && k < 16; ++k) out[k] = static_cast<float>(d[k]);
}

bool load_gr2(const std::uint8_t* data, std::size_t size, Gr2File& out, std::string& err) {
    out = Gr2File();
    if (!data) { err = "gr2: sem dados"; return false; }
    Loaded L;
    Ref root_type, root_obj;
    if (!load_sections(data, size, L, root_type, root_obj, err)) return false;
    out.version = L.version;
    if (!L.has(root_type, 32) || !L.has(root_obj, 4)) { err = "gr2: objeto raiz fora das secoes"; return false; }

    auto array_refs = [&](const Ref& obj, const Ref& type, const char* name, Ref& elem_type) -> std::vector<Ref> {
        std::vector<Ref> v;
        Member m;
        const long o = L.offset_of(type, name, &m);
        if (o < 0) return v;
        elem_type = m.ref;
        const Ref slot = Loaded::plus(obj, static_cast<std::uint32_t>(o));
        const std::uint32_t cnt = L.u32(slot);
        const Ref arr = L.ptr(Loaded::plus(slot, 4));
        if (!arr.ok() || cnt > 100000) return v;
        if (m.type == MT_ARRAY_OF_REFERENCES) {
            for (std::uint32_t i = 0; i < cnt; ++i) { const Ref r = L.ptr(Loaded::plus(arr, 4 * i)); if (r.ok()) v.push_back(r); }
        } else if (m.type == MT_REFERENCE_TO_ARRAY && m.ref.ok()) {
            const std::uint32_t stride = L.type_size(m.ref);
            if (stride) for (std::uint32_t i = 0; i < cnt; ++i) v.push_back(Loaded::plus(arr, stride * i));
        }
        return v;
    };
    auto field_ref = [&](const Ref& obj, const Ref& type, const char* name, Member* m = nullptr) -> Ref {
        const long o = L.offset_of(type, name, m);
        return o < 0 ? Ref{} : Loaded::plus(obj, static_cast<std::uint32_t>(o));
    };

    // esqueletos
    Ref skel_type;
    for (const Ref& sk : array_refs(root_obj, root_type, "Skeletons", skel_type)) {
        if (!skel_type.ok()) break;
        Gr2Skeleton S;
        const Ref nm = field_ref(sk, skel_type, "Name");
        if (nm.ok()) S.name = L.str_ptr(nm);
        Ref bone_type;
        for (const Ref& b : array_refs(sk, skel_type, "Bones", bone_type)) {
            if (!bone_type.ok()) break;
            Gr2Bone B;
            const Ref bn = field_ref(b, bone_type, "Name");
            if (bn.ok()) B.name = L.str_ptr(bn);
            if (B.name.empty()) B.name = "Bone_" + std::to_string(S.bones.size());
            const Ref pr = field_ref(b, bone_type, "ParentIndex");
            B.parent = pr.ok() ? L.i32(pr) : -1;
            const Ref tr = field_ref(b, bone_type, "Transform");
            if (tr.ok() && L.has(tr, 68)) {
                B.flags = L.u32(tr);
                for (int k = 0; k < 3; ++k) B.pos[k] = L.f32(Loaded::plus(tr, 4 + 4 * k));
                for (int k = 0; k < 4; ++k) B.quat[k] = L.f32(Loaded::plus(tr, 16 + 4 * k));
                for (int k = 0; k < 9; ++k) B.scale_shear[k] = L.f32(Loaded::plus(tr, 32 + 4 * k));
            }
            const Ref iw = field_ref(b, bone_type, "InverseWorldTransform");
            if (iw.ok() && L.has(iw, 64)) for (int k = 0; k < 16; ++k) B.inv_world[k] = L.f32(Loaded::plus(iw, 4 * k));
            S.bones.push_back(std::move(B));
        }
        if (S.name.empty()) S.name = "Skeleton_" + std::to_string(out.skeletons.size());
        out.skeletons.push_back(std::move(S));
    }

    // grupos de trilhas
    Ref tg_type;
    for (const Ref& g : array_refs(root_obj, root_type, "TrackGroups", tg_type)) {
        if (!tg_type.ok()) break;
        Gr2TrackGroup G;
        const Ref gn = field_ref(g, tg_type, "Name");
        if (gn.ok()) G.name = L.str_ptr(gn);
        if (G.name.empty()) G.name = "TrackGroup_" + std::to_string(out.track_groups.size());
        Member tm;
        const Ref tslot = field_ref(g, tg_type, "TransformTracks", &tm);
        if (tslot.ok() && tm.ref.ok()) {
            const std::uint32_t cnt = L.u32(tslot);
            const Ref arr = L.ptr(Loaded::plus(tslot, 4));
            const std::uint32_t stride = L.type_size(tm.ref);
            const long o_name = L.offset_of(tm.ref, "Name"), o_flags = L.offset_of(tm.ref, "Flags");
            const long o_ori = L.offset_of(tm.ref, "OrientationCurve"), o_pos = L.offset_of(tm.ref, "PositionCurve"), o_ss = L.offset_of(tm.ref, "ScaleShearCurve");
            if (arr.ok() && stride && cnt < 100000) {
                for (std::uint32_t i = 0; i < cnt; ++i) {
                    const Ref t = Loaded::plus(arr, stride * i);
                    Gr2TransformTrack T;
                    T.name = L.str_ptr(Loaded::plus(t, static_cast<std::uint32_t>(o_name >= 0 ? o_name : 0)));
                    if (T.name.empty()) T.name = "TransformTrack_" + std::to_string(i);
                    T.flags = L.i32(Loaded::plus(t, static_cast<std::uint32_t>(o_flags >= 0 ? o_flags : 4)));
                    read_legacy_curve(L, Loaded::plus(t, static_cast<std::uint32_t>(o_ori >= 0 ? o_ori : 8)), T.orientation);
                    read_legacy_curve(L, Loaded::plus(t, static_cast<std::uint32_t>(o_pos >= 0 ? o_pos : 16)), T.position);
                    read_legacy_curve(L, Loaded::plus(t, static_cast<std::uint32_t>(o_ss >= 0 ? o_ss : 24)), T.scale_shear);
                    G.tracks.push_back(std::move(T));
                }
            }
        }
        out.track_groups.push_back(std::move(G));
    }

    // animacoes
    Ref an_type;
    for (const Ref& a : array_refs(root_obj, root_type, "Animations", an_type)) {
        if (!an_type.ok()) break;
        Gr2Animation A;
        const Ref nm = field_ref(a, an_type, "Name");
        if (nm.ok()) A.name = L.str_ptr(nm);
        if (A.name.empty()) A.name = "Animation_" + std::to_string(out.animations.size());
        const Ref du = field_ref(a, an_type, "Duration"), ts = field_ref(a, an_type, "TimeStep"), ov = field_ref(a, an_type, "Oversampling");
        if (du.ok()) A.duration = L.f32(du);
        if (ts.ok()) A.time_step = L.f32(ts);
        if (ov.ok()) A.oversampling = L.f32(ov);
        Ref gtype;
        for (const Ref& g : array_refs(a, an_type, "TrackGroups", gtype)) {
            std::string n;
            if (gtype.ok()) { const Ref gn = field_ref(g, gtype, "Name"); if (gn.ok()) n = L.str_ptr(gn); }
            if (n.empty()) n = "TrackGroup_" + std::to_string(A.track_groups.size());
            A.track_groups.push_back(n);
        }
        out.animations.push_back(std::move(A));
    }
    return true;
}

}  // namespace f3v
