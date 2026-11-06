#include "ui_assets.h"

#include <cmath>
#include <filesystem>
#include <system_error>

#include "image.h"
#include "vfs.h"

namespace f3v {

std::vector<std::string> list_dir(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (dir.empty() || !std::filesystem::is_directory(std::filesystem::path(dir), ec)) return out;
    for (const auto& e : std::filesystem::directory_iterator(std::filesystem::path(dir), ec)) {
        if (ec) break;
        std::error_code ec2;
        if (e.is_regular_file(ec2)) out.push_back(e.path().filename().string());
    }
    return out;
}

std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    const char last = a.back();
#ifdef _WIN32
    return (last == '\\' || last == '/') ? a + b : a + "\\" + b;
#else
    return (last == '\\' || last == '/') ? a + b : a + "/" + b;
#endif
}

void UiAssets::add_image_dir(const std::string& dir) {
    for (const std::string& n : list_dir(dir)) {
        std::string key = lower_ascii(n);
        // CONVERTED\ui guarda "<nome original>.f3tex": registra pelo nome original tambem (sem o .f3tex)
        if (key.size() > 6 && key.compare(key.size() - 6, 6, ".f3tex") == 0) key = key.substr(0, key.size() - 6);
        if (!files_.count(key)) files_[key] = join_path(dir, n);
    }
}

void UiAssets::add_skin_dir(const std::string& dir) {
    for (const std::string& n : list_dir(dir)) {
        const std::string key = lower_ascii(n);
        if (key.size() > 4 && key.compare(key.size() - 4, 4, ".ini") == 0 && !skin_files_.count(key)) skin_files_[key] = join_path(dir, n);
    }
}

std::size_t UiAssets::add_font_dir(const std::string& dir) {
    std::size_t n = 0;
    for (const std::string& f : list_dir(dir)) {
        const std::string low = lower_ascii(f);
        if (low.size() < 4 || (low.compare(low.size() - 4, 4, ".ttf") != 0)) continue;
        std::vector<std::uint8_t> data;
        std::string err;
        if (!read_whole_file(join_path(dir, f), data)) { problems_.push_back("fonte " + f + ": nao abriu"); continue; }
        if (!add_font_memory(data, join_path(dir, f), err)) { problems_.push_back("fonte " + f + ": " + err); continue; }
        ++n;
    }
    return n;
}

bool UiAssets::load_stf(const std::string& path, std::string& err) {
    std::vector<std::uint8_t> data;
    if (!read_whole_file(path, data)) { err = "nao consegui abrir " + path; return false; }
    return stf_.load(data.data(), data.size(), err);
}

bool UiAssets::add_font_memory(const std::vector<std::uint8_t>& data, const std::string& label, std::string& err, bool system) {
    FontEntry e;
    e.path = label;
    e.system = system;
    e.font = std::make_unique<TrueTypeFont>();
    if (!e.font->load(data.data(), data.size(), err)) return false;
    e.family = lower_ascii(e.font->family());
    e.full = lower_ascii(e.font->full_name());
    for (const auto& o : fonts_) if (o.full == e.full && o.system == e.system) return true;      // a mesma fonte em duas pastas: fica a primeira
    fonts_.push_back(std::move(e));
    return true;
}

bool UiAssets::has_image(const std::string& name) const {
    const std::string key = lower_ascii(name);
    if (files_.count(key)) return true;
    if (mismatched_.count(key)) return false;
    return vfs_ && vfs_->find(name, RT_TGA) != nullptr;
}

const Image* UiAssets::image(const std::string& name) {
    const std::string key = lower_ascii(name);
    const auto it = images_.find(key);
    if (it != images_.end()) return it->second.get();
    std::unique_ptr<Image> img;
    const auto f = files_.find(key);
    if (f != files_.end()) {
        auto tmp = std::make_unique<Image>();
        std::string err;
        std::vector<std::uint8_t> data;
        if (read_whole_file(f->second, data) && decode_image(data.data(), data.size(), *tmp, err)) img = std::move(tmp);
        else if (load_image_file(f->second, *tmp, err)) img = std::move(tmp);          // .f3tex (CONVERTED/ui) e outros
        else problems_.push_back("imagem " + name + ": " + err);
    } else if (vfs_) {
        // v20: direto da demo. So TGA_Image (os .int citam .bmp/.tga que nao existem: esses NAO sao desenhados, como no original)
        if (const ResEntry* e = vfs_->find(name, RT_TGA)) {
            std::vector<std::uint8_t> data;
            std::string err;
            auto tmp = std::make_unique<Image>();
            // Regra do motor original (medida: os prints nao mostram as molduras zzmainpicture/zzmainlabel): o nome PEDIDO escolhe o
            // decodificador. ".tga" pedido com conteudo BMP (zzmainlabel-*) ou ".bmp" pedido com conteudo TGA (zzmainpicture-*) = nao desenha.
            const std::size_t dot = key.find_last_of('.');
            const std::string ext = dot == std::string::npos ? std::string() : key.substr(dot + 1);
            if (!vfs_->read(*e, data, err)) problems_.push_back("imagem " + name + " (" + vfs_->where(*e) + "): " + err);
            else {
                const bool is_bmp = data.size() >= 2 && data[0] == 'B' && data[1] == 'M';
                if ((ext == "tga" && is_bmp) || (ext == "bmp" && !is_bmp)) mismatched_.insert(key);
                else if (decode_image(data.data(), data.size(), *tmp, err)) img = std::move(tmp);
                else problems_.push_back("imagem " + name + " (" + vfs_->where(*e) + "): " + err);
            }
        }
    }
    const Image* p = img.get();
    images_[key] = std::move(img);
    return p;
}

const Skin* UiAssets::skin(const std::string& file) {
    const std::string key = lower_ascii(file);
    const auto it = skins_.find(key);
    if (it != skins_.end()) return it->second.get();
    std::unique_ptr<Skin> s;
    const auto f = skin_files_.find(key);
    if (f != skin_files_.end()) {
        auto tmp = std::make_unique<Skin>();
        std::string err;
        if (tmp->load_file(f->second, err)) s = std::move(tmp);
        else problems_.push_back("skin " + file + ": " + err);
    } else if (vfs_) {
        std::vector<std::uint8_t> data;
        if (vfs_->read(file, RT_INI, data)) {
            auto tmp = std::make_unique<Skin>();
            if (tmp->parse(std::string(data.begin(), data.end()))) s = std::move(tmp);
            else problems_.push_back("skin " + file + ": sem secoes validas");
        }
    }
    const Skin* p = s.get();
    skins_[key] = std::move(s);
    return p;
}

std::size_t UiAssets::add_system_fonts(const std::string& dir) {
    // so as fontes que a demo pode pedir (fontlist.ini) e a Arial, que o Windows usa no lugar das que nao estao instaladas
    static const char* files[] = {"arial.ttf", "ariblk.ttf", "times.ttf", "lucon.ttf", "LiberationSans-Regular.ttf"};
    std::size_t n = 0;
    for (const char* f : files) {
        std::vector<std::uint8_t> data;
        std::string err;
        if (read_whole_file(join_path(dir, f), data) && add_font_memory(data, join_path(dir, f), err, true)) ++n;
    }
    atlases_.clear();
    return n;
}

// Casa o nome do .int com uma fonte carregada: nome completo ou familia iguais; senao uma substituta.
// Arial/Times/Arial Black (fontes do Windows, que a demo nao traz) -> Gothic 821; Lucida Console -> Fallouty; Hellbound -> JH_Fallout.
const UiAssets::FontEntry* UiAssets::pick_font(const std::string& name) const {
    if (fonts_.empty()) return nullptr;
    const std::string low = lower_ascii(name);
    if (faithful_fonts_) {
        // como o jogo original num Windows sem as fontes da demo instaladas: fonte do sistema com o mesmo nome, senao Arial
        const FontEntry* arial = nullptr;
        for (const auto& e : fonts_) if (e.system && (e.full == low || e.family == low)) return &e;
        for (const auto& e : fonts_) if (e.system && (e.family == "arial" || e.full == "arial" || e.family == "liberation sans")) { arial = &e; break; }
        if (arial) return arial;
    }
    auto by = [&](const std::string& n) -> const FontEntry* {
        for (const auto& e : fonts_) if (e.full == n || e.family == n) return &e;
        for (const auto& e : fonts_) if (!n.empty() && (e.full.find(n) != std::string::npos || e.family.find(n) != std::string::npos)) return &e;
        return nullptr;
    };
    auto by_demo = [&](const std::string& n) -> const FontEntry* {
        for (const auto& e : fonts_) if (!e.system && (e.full == n || e.family == n)) return &e;
        for (const auto& e : fonts_) if (!e.system && !n.empty() && (e.full.find(n) != std::string::npos || e.family.find(n) != std::string::npos)) return &e;
        return nullptr;
    };
    if (const FontEntry* e = by_demo(low)) return e;
    if (const FontEntry* e = by(low)) return e;
    const char* sub = "gothic";
    if (low.find("lucida") != std::string::npos || low.find("console") != std::string::npos) sub = "fallouty";
    else if (low.find("hellbound") != std::string::npos) sub = "jh_fallout";
    if (const FontEntry* e = by(sub)) return e;
    return &fonts_.front();
}

std::string UiAssets::resolve_font(const std::string& font) const {
    const FontEntry* e = pick_font(font);
    return e ? e->font->full_name() : std::string("(nenhuma)");
}

std::vector<std::string> UiAssets::font_names() const {
    std::vector<std::string> v;
    for (const auto& e : fonts_) v.push_back(e.font->full_name());
    return v;
}

const FontAtlas* UiAssets::atlas(const std::string& font, float em_px) {
    const int px = std::max(4, static_cast<int>(std::lround(em_px)));
    const FontEntry* e = pick_font(font);
    if (!e) return nullptr;
    const std::string key = e->path + "|" + std::to_string(px);
    const auto it = atlases_.find(key);
    if (it != atlases_.end()) return it->second.get();
    auto a = std::make_unique<FontAtlas>();
    if (!bake_atlas(*e->font, static_cast<float>(px), *a)) { problems_.push_back("atlas da fonte " + font + " " + std::to_string(px) + "px falhou"); a.reset(); }
    const FontAtlas* p = a.get();
    atlases_[key] = std::move(a);
    return p;
}

GuiResources UiAssets::resources() {
    GuiResources r;
    r.texture_size = [this](const std::string& t, int& w, int& h) {
        const Image* im = image(t);
        if (!im) return false;
        w = static_cast<int>(im->w); h = static_cast<int>(im->h);
        return true;
    };
    r.text_width = [this](const std::string& f, float px, const std::string& s) {
        const FontAtlas* a = atlas(f, px);
        return a ? a->measure(s) : static_cast<float>(s.size()) * px * 0.5f;
    };
    r.line_height = [this](const std::string& f, float px) {
        const FontAtlas* a = atlas(f, px);
        return a ? std::ceil(a->ascent - a->descent) : px * 1.2f;
    };
    r.text = [this](std::uint32_t id) { return stf_.text_cp1252(id); };
    r.skin = [this](const std::string& s) { return skin(s); };
    return r;
}

}  // namespace f3v
