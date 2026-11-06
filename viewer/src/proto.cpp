// v20: prototipos de entidade (ver proto.h).
#include "proto.h"

#include <cstring>

namespace f3v {

namespace {

bool printable(std::uint8_t c) { return c >= 32 && c < 127; }
std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; }
bool ends_with_ci(const std::string& s, const char* suf) { const std::size_t n = std::strlen(suf); return s.size() >= n && lower(s.substr(s.size() - n)) == lower(suf); }
bool starts_with_ci(const std::string& s, const char* pre) { const std::size_t n = std::strlen(pre); return s.size() >= n && lower(s.substr(0, n)) == lower(pre); }

std::size_t find_tag(const std::uint8_t* d, std::size_t n, const char* tag, std::size_t from = 0) {
    for (std::size_t i = from; i + 4 <= n; ++i) if (std::memcmp(d + i, tag, 4) == 0) return i;
    return std::string::npos;
}

}  // namespace

std::string part_slot(const std::string& p) {
    static const char* kSlots[] = {"Hai", "Pon", "Mus", "Bea", "Bod", "Fee", "Bac", "Van", "Hea", "Han", "Sho", "Eye"};
    if (p.size() < 4 || p.find('.') != std::string::npos) return "";
    for (const char* s : kSlots) if (p.compare(0, 3, s) == 0 && p[3] >= 'A' && p[3] <= 'Z') return s;
    return "";
}

bool parse_prototype(const std::uint8_t* d, std::size_t n, const std::string& kind, Prototype& out) {
    out = Prototype();
    out.kind = kind;
    if (!d || n < 8) return false;
    // textos: u16 tamanho (3..96) + bytes imprimiveis
    for (std::size_t i = 0; i + 2 < n;) {
        const std::size_t len = static_cast<std::size_t>(d[i] | (d[i + 1] << 8));
        if (len >= 3 && len <= 96 && i + 2 + len <= n) {
            bool ok = true;
            for (std::size_t k = 0; k < len && ok; ++k) ok = printable(d[i + 2 + k]);
            if (ok) { out.texts.push_back({i, std::string(reinterpret_cast<const char*>(d + i + 2), len)}); i += 2 + len; continue; }
        }
        ++i;
    }
    const std::size_t een2 = find_tag(d, n, "EEN2"), gcre = find_tag(d, n, "GCRE"), gitm = find_tag(d, n, "GITM"), gchr = find_tag(d, n, "GCHR");
    for (const ProtoText& t : out.texts) if (een2 == std::string::npos || t.at > een2) { out.model = t.text; break; }
    std::string last_part;
    for (const ProtoText& t : out.texts) {
        const std::string& s = t.text;
        if (s == out.model) continue;
        if (starts_with_ci(s, "skin_") && ends_with_ci(s, ".dds")) { if (out.skin.empty()) out.skin = s; continue; }
        if (starts_with_ci(s, "port_")) { out.portrait = s; continue; }
        if (ends_with_ci(s, ".amx")) { out.script = s; continue; }
        if (gchr != std::string::npos && t.at > gchr) { out.character_name = s; continue; }
        const bool after_gcre = gcre != std::string::npos && t.at > gcre;
        if (ends_with_ci(s, ".arm") || ends_with_ci(s, ".wea") || ends_with_ci(s, ".amo") || ends_with_ci(s, ".itm") || ends_with_ci(s, ".use")) {
            if (after_gcre && ends_with_ci(s, ".arm") && out.armor.empty()) out.armor = s;
            else if (after_gcre && ends_with_ci(s, ".wea") && out.weapon.empty()) out.weapon = s;
            else out.inventory.push_back(s);
            continue;
        }
        const std::string slot = part_slot(s);
        if (!slot.empty() && (kind != "ARM" || gitm == std::string::npos || t.at > gitm)) { out.parts.push_back(s); last_part = s; continue; }
        if (!last_part.empty() && ends_with_ci(s, "_lg.dds") && starts_with_ci(s, last_part.c_str())) { out.part_texture[last_part] = s.substr(0, s.size() - 4); continue; }
        if (kind == "WEA" && starts_with_ci(s, "WP_") && s.find('.') == std::string::npos && out.held_model.empty()) { out.held_model = s; continue; }
        if (kind == "CRT" && out.display_name.empty() && s.find('.') == std::string::npos && s != "<No Script>" && t.at > een2 && (gcre == std::string::npos || t.at < gcre)) out.display_name = s;
    }
    return !out.model.empty();
}

}  // namespace f3v
