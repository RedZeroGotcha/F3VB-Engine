#include "gui_int.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace f3v {

namespace {

constexpr std::uint32_t kMaxStr = 4096, kMaxChildren = 4096;
constexpr int kMaxDepth = 32;

struct Reader {
    const std::uint8_t* d; std::size_t n, o = 0; std::string* err; bool bad = false;
    bool fail(const std::string& m) { if (!bad) { *err = m + " (em " + std::to_string(o) + ")"; bad = true; } return false; }
    bool need(std::size_t k) { return o + k <= n ? true : fail("fim do arquivo: faltam " + std::to_string(o + k - n) + " bytes"); }
    std::uint8_t u8() { if (!need(1)) return 0; return d[o++]; }
    std::uint32_t u32() { if (!need(4)) return 0; const std::uint32_t v = d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (static_cast<std::uint32_t>(d[o + 3]) << 24); o += 4; return v; }
    std::int32_t i32() { return static_cast<std::int32_t>(u32()); }
    void raw(std::uint8_t* dst, std::size_t k) { if (!need(k)) return; std::memcpy(dst, d + o, k); o += k; }
    IRect rect() { IRect r; r.x0 = i32(); r.y0 = i32(); r.x1 = i32(); r.y1 = i32(); return r; }
    std::string str() {
        const std::uint32_t k = u32();
        if (bad) return {};
        if (k > kMaxStr) { fail("string de " + std::to_string(k) + " bytes (absurda)"); return {}; }
        if (!need(k)) return {};
        std::string s(reinterpret_cast<const char*>(d + o), k);
        o += k;
        return s;
    }
};

bool read_widget(Reader& r, std::uint32_t kind, int ver, int depth, GuiWidget* parent, std::unique_ptr<GuiWidget>& out) {
    if (depth > kMaxDepth) return r.fail("arvore funda demais");
    if (kind > 4) return r.fail("tipo de objeto desconhecido " + std::to_string(kind));
    auto w = std::make_unique<GuiWidget>();
    w->kind = static_cast<WidgetKind>(kind);
    w->parent = parent;
    if (w->kind == WidgetKind::Button) {
        r.u32();
        if (ver >= 13) { r.u8(); for (auto& s : w->sounds) s = r.str(); }
    }
    w->rect = r.rect();
    w->name = r.str();
    r.u8(); r.u8(); r.u8();
    w->skin = r.str();
    r.raw(w->color, 4);
    w->rel = r.rect();
    r.u8();
    w->interactive = r.u32();
    if (ver >= 12) { r.u32(); w->tip_a = r.i32(); r.u32(); w->tip_text = r.i32(); w->tip_texture = r.str(); }
    for (GuiFragment& f : w->frag) {
        f.flags = r.u32(); f.width = r.i32(); f.height = r.i32(); r.i32(); r.i32();
        r.u32();
        f.texture = r.str();
        r.u32();                                                   // f32 sem uso conhecido
        r.raw(f.rgba, 4);
        f.rect = r.rect();
    }
    r.i32();                                                       // sempre -1 nos arquivos reais
    const std::uint32_t nch = r.u32();
    if (r.bad) return false;
    if (nch > kMaxChildren) return r.fail(std::to_string(nch) + " filhos (absurdo)");
    for (std::uint32_t i = 0; i < nch; ++i) {
        const std::uint32_t k = r.u32();
        std::unique_ptr<GuiWidget> c;
        if (r.bad || !read_widget(r, k, ver, depth + 1, w.get(), c)) return false;
        w->children.push_back(std::move(c));
    }
    if (w->kind == WidgetKind::Label || w->kind == WidgetKind::Edit) {   // sufixo DEPOIS dos filhos
        w->text_id = r.u32();
        w->font = r.str();
        r.u32();
        w->font_size = r.u32();
        r.u32();
        r.raw(w->text_rgba, 4);
        r.raw(w->text_tail, sizeof w->text_tail);
        w->align = r.u32();
    }
    if (r.bad) return false;
    out = std::move(w);
    return true;
}

}  // namespace

const char* widget_kind_name(WidgetKind k) {
    switch (k) {
        case WidgetKind::Window: return "janela";
        case WidgetKind::Button: return "botao";
        case WidgetKind::Picture: return "imagem";
        case WidgetKind::Label: return "rotulo";
        case WidgetKind::Edit: return "edicao";
    }
    return "?";
}

std::string lower_ascii(const std::string& s) {
    std::string o(s);
    for (char& c : o) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return o;
}

GuiWidget* GuiFile::find(const std::string& name) const {
    const std::string low = lower_ascii(name);
    GuiWidget* hit = nullptr;
    walk([&](GuiWidget& w, int) { if (!hit && lower_ascii(w.name) == low) hit = &w; });
    return hit;
}

std::size_t GuiFile::count() const {
    std::size_t n = 0;
    walk([&](GuiWidget&, int) { ++n; });
    return n;
}

bool load_gui(const std::uint8_t* data, std::size_t size, GuiFile& out, std::string& err) {
    out = GuiFile();
    if (!data || size < 12 || std::memcmp(data, "GUI V", 5) != 0 || data[6] != '.' || data[5] < '0' || data[5] > '9' || data[7] < '0' || data[7] > '9') {
        err = "nao e um .int (assinatura 'GUI V1.x' ausente)";
        return false;
    }
    const int ver = (data[5] - '0') * 10 + (data[7] - '0');
    if (ver < 11 || ver > 13) { err = "versao GUI V" + std::string(1, static_cast<char>(data[5])) + "." + std::string(1, static_cast<char>(data[7])) + " nao suportada"; return false; }
    Reader r{data, size, 8, &err};
    out.version = ver;
    out.name = r.str();
    r.u8();
    const std::uint32_t nroots = r.u32();
    if (r.bad) return false;
    if (nroots > kMaxChildren) { err = std::to_string(nroots) + " raizes (absurdo)"; return false; }
    for (std::uint32_t i = 0; i < nroots; ++i) {
        const std::uint32_t k = r.u32();
        std::unique_ptr<GuiWidget> w;
        if (r.bad || !read_widget(r, k, ver, 0, nullptr, w)) { out.roots.clear(); return false; }
        out.roots.push_back(std::move(w));
    }
    if (r.o != size) { err = "sobraram " + std::to_string(size - r.o) + " bytes depois da arvore"; out.roots.clear(); return false; }
    return true;
}

bool load_gui_file(const std::string& path, GuiFile& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "nao consegui abrir " + path; return false; }
    std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return load_gui(b.data(), b.size(), out, err);
}

}  // namespace f3v
