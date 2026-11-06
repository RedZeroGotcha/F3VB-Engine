#include "stf.h"

#include <cstdio>
#include <fstream>
#include <iterator>

namespace f3v {

static std::uint32_t rd32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }

bool Stf::load(const std::uint8_t* data, std::size_t size, std::string& err) {
    loaded_ = false;
    entries_.clear();
    data_.clear();
    if (size < 12) { err = "arquivo menor que o cabecalho (12 bytes)"; return false; }
    const std::uint32_t count = rd32(data + 8);
    if (count > (size - 12) / 16) { err = std::to_string(count) + " entradas nao cabem no arquivo (" + std::to_string(size) + " bytes)"; return false; }
    data_.assign(data, data + size);
    entries_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) entries_.push_back(Entry{rd32(data + 12 + 16 * static_cast<std::size_t>(i)), rd32(data + 12 + 16 * static_cast<std::size_t>(i) + 4)});
    loaded_ = true;
    return true;
}

bool Stf::load_file(const std::string& path, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "nao consegui abrir " + path; return false; }
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load(b.data(), b.size(), err);
}

std::string Stf::text(std::uint32_t id) const {
    if (!loaded_ || id >= entries_.size()) return std::string();
    return ascii_only(raw_entry(id));
}

std::string Stf::text_cp1252(std::uint32_t id1) const {
    if (!loaded_ || id1 == 0 || id1 > entries_.size()) return std::string();
    std::string s = raw_entry(id1 - 1);
    for (char& c : s) if (c == '\r' || c == '\t') c = ' ';
    return s;
}

std::string Stf::raw_entry(std::uint32_t id) const {
    if (!loaded_ || id >= entries_.size()) return std::string();
    const Entry& e = entries_[id];
    std::size_t width = 1;
    for (; width <= 2; ++width)                                   // tenta length em bytes; se nao couber, em unidades de 2 bytes
        if (static_cast<std::uint64_t>(e.off) + static_cast<std::uint64_t>(e.len) * width <= data_.size()) break;
    if (width > 2) return std::string();
    const std::size_t n = static_cast<std::size_t>(e.len) * width;
    const std::uint8_t* p = data_.data() + e.off;
    std::string out;
    bool utf16 = false;
    if (n >= 2 && n % 2 == 0) {
        std::size_t zeros = 0;
        for (std::size_t i = 1; i < n; i += 2) zeros += p[i] == 0;
        utf16 = zeros >= n / 2 - 1 && zeros > 0;
    }
    if (utf16) { for (std::size_t i = 0; i + 1 < n; i += 2) { const unsigned c = p[i] | (p[i + 1] << 8); out.push_back(c < 256 ? static_cast<char>(c) : '?'); } }
    else for (std::size_t i = 0; i < n; ++i) out.push_back(static_cast<char>(p[i]));
    while (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

std::string ascii_only(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= 32 && c < 127) out.push_back(static_cast<char>(c));
        else if (c == '\n' || c == '\r' || c == '\t') out.push_back(' ');
        else if ((c == 0xC2 || c == 0xC3) && i + 1 < s.size() && (static_cast<unsigned char>(s[i + 1]) & 0xC0) == 0x80) { ++i; out.push_back('?'); }   // utf-8 de 2 bytes (acentos latinos)
        else if (c >= 0x80) out.push_back('?');
    }
    return out;
}

}  // namespace f3v
