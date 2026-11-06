#include "skin.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

#include "gui_int.h"

namespace f3v {

namespace {

const char* kFrag[9] = {"upperleft", "uppermiddle", "upperright", "left", "middle", "right", "lowerleft", "lowermiddle", "lowerright"};

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

int clamp255(long v) { return v < 0 ? 0 : (v > 255 ? 255 : static_cast<int>(v)); }

}  // namespace

const char* Skin::fragment_name(int i) { return i >= 0 && i < 9 ? kFrag[i] : "?"; }

bool Skin::parse(const std::string& text) {
    sections_.clear();
    std::istringstream in(text);
    std::string line, cur;
    SkinFragment* frag = nullptr;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            const std::size_t e = line.find(']');
            cur = lower_ascii(trim(line.substr(1, e == std::string::npos ? std::string::npos : e - 1)));
            frag = cur.empty() ? nullptr : &sections_[cur];
            continue;
        }
        if (!frag) continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = lower_ascii(trim(line.substr(0, eq)));
        std::string val = trim(line.substr(eq + 1));
        const std::size_t sc = val.find(';');
        if (sc != std::string::npos) val = trim(val.substr(0, sc));
        if (key == "flags") frag->flags = static_cast<std::uint32_t>(std::strtoul(val.c_str(), nullptr, 10)) & 3u;
        else if (key == "texture") frag->texture = val;
        else if (key == "width") frag->width = std::atoi(val.c_str());
        else if (key == "height") frag->height = std::atoi(val.c_str());
        else if (key == "rate") frag->rate = static_cast<float>(std::atof(val.c_str()));
        else if (key == "color") {
            long c[3] = {255, 255, 255};
            const char* p = val.c_str();
            for (int i = 0; i < 3 && *p; ++i) {
                char* end = nullptr;
                c[i] = std::strtol(p, &end, 10);
                if (end == p) break;
                p = end;
                while (*p == ',' || *p == ' ' || *p == '\t') ++p;
            }
            for (int i = 0; i < 3; ++i) frag->rgb[i] = static_cast<std::uint8_t>(clamp255(c[i]));
            frag->has_color = true;
        }
    }
    return !sections_.empty();
}

bool Skin::load_file(const std::string& path, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "nao consegui abrir " + path; return false; }
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (!parse(text)) { err = "nenhuma secao [objeto.estado.fragmento] em " + path; return false; }
    return true;
}

const SkinFragment* Skin::get(const std::string& object, const std::string& state, int fragment) const {
    if (fragment < 0 || fragment > 8) return nullptr;
    const std::string key = lower_ascii(object) + (state.empty() ? "" : "." + lower_ascii(state)) + "." + kFrag[fragment];
    const auto it = sections_.find(key);
    return it == sections_.end() ? nullptr : &it->second;
}

bool Skin::has_state(const std::string& object, const std::string& state) const { return get(object, state, 4) != nullptr; }

}  // namespace f3v
