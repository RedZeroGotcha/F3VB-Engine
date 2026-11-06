// F3VBUiPreview (v19): desenha telas da interface original (.int) em PNG, sem janela e sem placa de video (backend de CPU).
//   F3VBUiPreview Mainmenu                      -> Mainmenu.png (tela de 1024x768)
//   F3VBUiPreview --all [--out PASTA]           -> um PNG por .int
//   F3VBUiPreview Mainmenu --hover NewGameBtn   -> com o mouse em cima de um botao (estado "selected" da skin)
//   opcoes: --assets <original_assets>  --demo <pasta da demo (Fonts, English.stf)>  --w 1024 --h 768  --down NOME  --bg imagem.tga  --out ARQ|PASTA
// Pastas padrao: F3VB_ASSETS_DIR e F3VB_DEMO_DIR (variaveis de ambiente). Imprime o que faltou (texturas, fontes) para conferir.
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "gui.h"
#include "image.h"
#include "ui_assets.h"
#include "ui_raster.h"
#include "vfs.h"

using namespace f3v;

namespace {

std::string env(const char* n) { const char* v = std::getenv(n); return v ? v : ""; }
Vfs g_vfs;      // v20: com --demo, tudo vem da demo original (resource.rht + .grp)

void setup(UiAssets& a, const std::string& assets, const std::string& demo) {
    if (g_vfs.is_open()) {
        a.set_vfs(&g_vfs);
        const std::size_t nf = a.add_font_dir(join_path(demo, "Fonts"));
        std::string err;
        if (a.load_stf(join_path(demo, "English.stf"), err)) std::printf("textos: %s (%zu)\n", join_path(demo, "English.stf").c_str(), a.stf().count());
        std::printf("demo original: %zu recursos | fontes TrueType: %zu\n", g_vfs.size(), nf);
        return;
    }
    a.add_image_dir(join_path(assets, join_path("RETAIL", join_path("Interface", "TGA_Image"))));
    a.add_image_dir(join_path(assets, join_path("CONVERTED", "ui")));
    a.add_skin_dir(join_path(assets, join_path("RETAIL", join_path("Interface", "INI_Config"))));
    std::size_t nf = 0;
    if (!demo.empty()) nf += a.add_font_dir(join_path(demo, "Fonts"));
    nf += a.add_font_dir(join_path(assets, join_path("RETAIL", "Fonts")));
    std::string err;
    const std::string stfs[] = {env("F3VB_STF"), demo.empty() ? "" : join_path(demo, "English.stf"), join_path(assets, join_path("RETAIL", "English.stf")),
                                join_path(assets, join_path("CONVERTED", join_path("scripts", "English.stf")))};
    bool stf = false;
    for (const std::string& s : stfs) if (!s.empty() && a.load_stf(s, err)) { std::printf("textos: %s (%zu)\n", s.c_str(), a.stf().count()); stf = true; break; }
    if (!stf) std::printf("AVISO: English.stf nao encontrado (rotulos sem texto). Use --demo ou F3VB_STF.\n");
    std::printf("imagens indexadas: %zu | fontes TrueType: %zu", a.image_files(), nf);
    for (const std::string& n : a.font_names()) std::printf(" [%s]", n.c_str());
    std::printf("\n");
    if (nf == 0) std::printf("AVISO: nenhuma fonte .ttf (procure a pasta Fonts da demo: --demo ou F3VB_DEMO_DIR; ou rode: py -3 -m f3vb import-loose <demo>)\n");
}

bool render_one(UiAssets& a, const std::string& int_path, const std::string& out, int W, int H, const std::string& hover, const std::string& down, const Image* bg) {
    GuiScreen s;
    std::string err;
    std::vector<std::uint8_t> bytes;
    if (g_vfs.is_open() && !file_exists(int_path)) {
        if (!g_vfs.read(int_path, RT_INT, bytes) || !s.load(bytes.data(), bytes.size(), err)) { std::printf("FALHA %s: %s\n", int_path.c_str(), err.empty() ? "nao existe na demo" : err.c_str()); return false; }
    } else if (!s.load_file(int_path, err)) { std::printf("FALHA %s: %s\n", int_path.c_str(), err.c_str()); return false; }
    if (!hover.empty()) { s.set_hover(s.find(hover)); if (!s.find(hover)) std::printf("  aviso: botao '%s' nao existe nesta tela\n", hover.c_str()); }
    if (!down.empty()) { GuiWidget* w = s.find(down); s.set_hover(w); s.set_pressed(w); }
    std::vector<GuiDraw> draws;
    s.build(a.resources(), draws);
    Canvas c;
    c.w = W; c.h = H;
    c.clear(24, 22, 20);
    if (bg) raster_background(*bg, c);
    raster_draws(draws, a, c);
    const bool ok = write_png(out, W, H, c.rgba);
    const auto& st = s.stats();
    std::printf("%-28s %4zu objetos | %4zu quads, %3zu textos, %3zu fragmentos com textura ausente -> %s%s\n", s.file().name.c_str(), s.file().count(), st.quads, st.texts, st.missing,
                out.c_str(), ok ? "" : " (NAO GRAVOU)");
    if (!st.missing_names.empty()) {
        std::printf("    ausentes:");
        int k = 0;
        for (const auto& n : st.missing_names) { if (k++ == 8) { std::printf(" ..."); break; } std::printf(" %s", n.c_str()); }
        std::printf("\n");
    }
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    std::string assets = env("F3VB_ASSETS_DIR"), demo = env("F3VB_DEMO_DIR"), out, hover, down, bgfile, target;
    int W = 1024, H = 768;
    bool all = false, demo_fonts = false;
    std::string sysfonts;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--assets") assets = next();
        else if (a == "--demo") demo = next();
        else if (a == "--out") out = next();
        else if (a == "--hover") hover = next();
        else if (a == "--down") down = next();
        else if (a == "--bg") bgfile = next();
        else if (a == "--w") W = std::atoi(next().c_str());
        else if (a == "--h") H = std::atoi(next().c_str());
        else if (a == "--all") all = true;
        else if (a == "--system-fonts") sysfonts = next();
        else if (a == "--demo-fonts") demo_fonts = true;
        else if (a == "-h" || a == "--help") { std::printf("uso: F3VBUiPreview <tela|arquivo.int|--all> [--assets DIR] [--demo DIR] [--out ARQ|PASTA] [--hover BOTAO] [--down BOTAO] [--bg IMG] [--w N --h N]\n"); return 0; }
        else target = a;
    }
    if (W < 64 || H < 48 || W > 8192 || H > 8192) { std::printf("tamanho invalido\n"); return 2; }
    if (!demo.empty()) {
        std::string err;
        if (!g_vfs.open_demo(demo, err) && assets.empty()) { std::printf("demo nao abriu: %s\n", err.c_str()); return 2; }
    }
    if (assets.empty() && !g_vfs.is_open()) { std::printf("use --demo <pasta da demo> (ou --assets <original_assets>)\n"); return 2; }
    UiAssets a;
    setup(a, assets, demo);
    if (!sysfonts.empty()) std::printf("fontes do sistema: %zu (%s)\n", a.add_system_fonts(sysfonts), sysfonts.c_str());
    a.set_faithful_fonts(!demo_fonts);
    Image bg;
    const Image* bgp = nullptr;
    if (!bgfile.empty()) { std::string err; if (load_image_file(bgfile, bg, err)) bgp = &bg; else std::printf("fundo: %s\n", err.c_str()); }
    const std::string int_dir = join_path(assets, join_path("RETAIL", join_path("Interface", "INT_Interface")));
    int fails = 0;
    if (all) {
        const std::string dir = out.empty() ? "ui_preview" : out;
        std::error_code mk_ec;
        std::filesystem::create_directories(std::filesystem::path(dir), mk_ec);
        if (g_vfs.is_open()) {
            for (const ResEntry* e : g_vfs.list(RT_INT)) if (!render_one(a, e->name, join_path(dir, e->name + ".png"), W, H, hover, down, bgp)) ++fails;
        } else for (const std::string& f : list_dir(int_dir)) {
            const std::string low = lower_ascii(f);
            if (low.size() < 4 || low.compare(low.size() - 4, 4, ".int") != 0) continue;
            if (!render_one(a, join_path(int_dir, f), join_path(dir, f.substr(0, f.size() - 4) + ".png"), W, H, hover, down, bgp)) ++fails;
        }
    } else {
        if (target.empty()) { std::printf("diga a tela (ex.: Mainmenu) ou --all\n"); return 2; }
        std::string path = target;
        if (lower_ascii(path).find(".int") == std::string::npos && !g_vfs.is_open()) path = join_path(int_dir, target + ".int");
        if (out.empty()) { const std::size_t k = target.find_last_of("\\/"); out = (k == std::string::npos ? target : target.substr(k + 1)); if (lower_ascii(out).find(".int") != std::string::npos) out = out.substr(0, out.size() - 4); out += ".png"; }
        if (!render_one(a, path, out, W, H, hover, down, bgp)) ++fails;
    }
    for (const std::string& p : a.problems()) std::printf("problema: %s\n", p.c_str());
    return fails ? 1 : 0;
}
