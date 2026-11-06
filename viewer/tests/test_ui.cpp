// v19: testes do sistema de interface (imagens TGA/BMP, formato .int, skins, fonte TrueType,
// montagem da tela, clique/deslocamento e regras da criacao de personagem).
// Tudo e gerado aqui mesmo, em memoria: NENHUM arquivo do jogo e usado (regra CLEANROOM).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "chargen.h"
#include "gui.h"
#include "gui_int.h"
#include "image.h"
#include "skin.h"
#include "ttf.h"
#include "ui_raster.h"

using namespace f3v;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

using Bytes = std::vector<std::uint8_t>;

// ------------------------------------------------------------------ escritores little-endian / big-endian
static void le(Bytes& b, std::uint32_t v, int n) { for (int i = 0; i < n; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
static void be(Bytes& b, std::uint32_t v, int n) { for (int i = n - 1; i >= 0; --i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
static void lestr(Bytes& b, const std::string& s) { le(b, static_cast<std::uint32_t>(s.size()), 4); b.insert(b.end(), s.begin(), s.end()); }
static void lef(Bytes& b, float f) { std::uint32_t u; std::memcpy(&u, &f, 4); le(b, u, 4); }

// =================================================================== 1. imagens
static void test_images() {
    // TGA 2x2 sem compressao, 32 bits, origem embaixo (padrao): a primeira linha gravada e a de BAIXO
    {
        Bytes t(18, 0);
        t[2] = 2; t[12] = 2; t[14] = 2; t[16] = 32; t[17] = 8;           // tipo 2, 2x2, 32 bpp, 8 bits de alfa
        const std::uint8_t px[4][4] = {{0, 0, 255, 255}, {0, 255, 0, 255}, {255, 0, 0, 128}, {10, 20, 30, 40}};   // BGRA
        for (auto& p : px) t.insert(t.end(), p, p + 4);
        Image im; std::string err;
        CHECK(decode_tga(t.data(), t.size(), im, err));
        CHECK(im.w == 2 && im.h == 2);
        // linha de cima da imagem = segunda linha gravada (azul, alfa 128) e (30,20,10,40)
        CHECK(im.rgba[0] == 0 && im.rgba[1] == 0 && im.rgba[2] == 255 && im.rgba[3] == 128);
        CHECK(im.rgba[4] == 30 && im.rgba[5] == 20 && im.rgba[6] == 10 && im.rgba[7] == 40);
        CHECK(im.rgba[8] == 255 && im.rgba[9] == 0 && im.rgba[10] == 0);   // vermelho embaixo a esquerda
    }
    // TGA RLE (tipo 10), 24 bits, origem em cima: 1 pacote repetido de 3 + 1 pixel cru
    {
        Bytes t(18, 0);
        t[2] = 10; t[12] = 4; t[14] = 1; t[16] = 24; t[17] = 0x20;
        t.push_back(0x80 | 2); t.push_back(1); t.push_back(2); t.push_back(3);     // 3x (B1 G2 R3)
        t.push_back(0); t.push_back(9); t.push_back(8); t.push_back(7);            // 1 pixel cru
        Image im; std::string err;
        CHECK(decode_tga(t.data(), t.size(), im, err));
        CHECK(im.w == 4 && im.h == 1);
        CHECK(im.rgba[0] == 3 && im.rgba[1] == 2 && im.rgba[2] == 1 && im.rgba[3] == 255);
        CHECK(im.rgba[8] == 3 && im.rgba[12] == 7 && im.rgba[13] == 8 && im.rgba[14] == 9);
        // truncado: precisa falhar sem estourar
        Bytes cut(t.begin(), t.end() - 3);
        CHECK(!decode_tga(cut.data(), cut.size(), im, err));
    }
    // BMP 24 bits 2x2 (de baixo para cima, linhas alinhadas a 4 bytes)
    {
        Bytes b;
        const std::uint32_t stride = 8, off = 54;
        b.push_back('B'); b.push_back('M'); le(b, off + stride * 2, 4); le(b, 0, 4); le(b, off, 4);
        le(b, 40, 4); le(b, 2, 4); le(b, 2, 4); le(b, 1, 2); le(b, 24, 2); le(b, 0, 4); le(b, stride * 2, 4); le(b, 0, 4); le(b, 0, 4); le(b, 0, 4); le(b, 0, 4);
        const std::uint8_t bottom[8] = {255, 0, 0, 0, 255, 0, 0, 0};          // azul, verde (BGR) + 2 de alinhamento
        const std::uint8_t top[8] = {0, 0, 255, 255, 255, 255, 0, 0};          // vermelho, branco
        b.insert(b.end(), bottom, bottom + 8);
        b.insert(b.end(), top, top + 8);
        Image im; std::string err;
        CHECK(decode_bmp(b.data(), b.size(), im, err));
        CHECK(im.w == 2 && im.h == 2);
        CHECK(im.rgba[0] == 255 && im.rgba[1] == 0 && im.rgba[2] == 0);       // canto superior esquerdo = vermelho
        CHECK(im.rgba[4] == 255 && im.rgba[5] == 255 && im.rgba[6] == 255);
        CHECK(im.rgba[8] == 0 && im.rgba[9] == 0 && im.rgba[10] == 255);      // embaixo a esquerda = azul
        Image im2;
        CHECK(decode_image(b.data(), b.size(), im2, err) && im2.w == 2);       // detecta o formato sozinho
    }
    // lixo nao pode passar
    {
        Bytes junk(100, 0xAB);
        Image im; std::string err;
        CHECK(!decode_bmp(junk.data(), junk.size(), im, err));
        CHECK(!decode_tga(junk.data(), 5, im, err));
    }
    // PNG gerado: assinatura e cabecalho IHDR
    {
        const std::vector<std::uint8_t> rgba(3 * 2 * 4, 200);
        const Bytes png = encode_png(3, 2, rgba);
        CHECK(png.size() > 33);
        CHECK(png.size() > 8 && png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
        CHECK(png.size() > 24 && png[19] == 3 && png[23] == 2);               // largura 3, altura 2
    }
}

// =================================================================== 2. formato .int (montador igual ao tools/py/f3vb/intfile.py)
struct WSpec {
    std::uint32_t kind = 0;
    std::string name, skin;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    std::string tex[9];
    std::uint8_t frag_rgba[4] = {128, 128, 128, 255};
    int frag_w = 8, frag_h = 8;
    std::uint32_t frag_flags = 0;
    std::vector<WSpec> children;
    std::uint32_t text_id = 0, font_size = 12, align = 0;
    std::string font = "Arial";
    std::string sounds[3];
    std::uint8_t tail2 = 2;                    // v20: alinhamento horizontal no 3o byte do sufixo (0 esq, 1 dir, 2 centro)
};

static void build_widget(Bytes& b, const WSpec& w, int ver) {
    le(b, w.kind, 4);
    if (w.kind == 1) {
        le(b, 0, 4);
        if (ver >= 13) { b.push_back(0); for (const auto& s : w.sounds) lestr(b, s); }
    }
    le(b, static_cast<std::uint32_t>(w.x0), 4); le(b, static_cast<std::uint32_t>(w.y0), 4);
    le(b, static_cast<std::uint32_t>(w.x1), 4); le(b, static_cast<std::uint32_t>(w.y1), 4);
    lestr(b, w.name);
    b.push_back(0); b.push_back(0); b.push_back(0);
    lestr(b, w.skin);
    b.push_back(128); b.push_back(128); b.push_back(128); b.push_back(255);
    for (int i = 0; i < 4; ++i) le(b, 0, 4);                                   // rel
    b.push_back(0); le(b, w.kind == 2 || w.kind == 3 ? 0 : 1, 4);               // body_u8, interactive
    if (ver >= 12) { le(b, 0, 4); le(b, 0xFFFFFFFFu, 4); le(b, 0, 4); le(b, 0xFFFFFFFFu, 4); lestr(b, ""); }
    for (int i = 0; i < 9; ++i) {
        le(b, w.frag_flags, 4); le(b, static_cast<std::uint32_t>(w.frag_w), 4); le(b, static_cast<std::uint32_t>(w.frag_h), 4);
        le(b, 0, 4); le(b, 0, 4);
        le(b, 0xDEADBEEFu, 4);                                                  // u32 "lixo" antes do nome (visto nos arquivos reais)
        lestr(b, w.tex[i]);
        lef(b, 0.f);
        b.insert(b.end(), w.frag_rgba, w.frag_rgba + 4);
        le(b, static_cast<std::uint32_t>(w.x0), 4); le(b, static_cast<std::uint32_t>(w.y0), 4);
        le(b, static_cast<std::uint32_t>(w.x1), 4); le(b, static_cast<std::uint32_t>(w.y1), 4);
    }
    le(b, 0xFFFFFFFFu, 4);
    le(b, static_cast<std::uint32_t>(w.children.size()), 4);
    for (const auto& c : w.children) build_widget(b, c, ver);
    if (w.kind == 3 || w.kind == 4) {
        le(b, w.text_id, 4); lestr(b, w.font); le(b, 0, 4); le(b, w.font_size, 4); le(b, 1, 4);
        b.push_back(255); b.push_back(200); b.push_back(100); b.push_back(255);
        for (int i = 0; i < 22; ++i) b.push_back(i == 2 ? w.tail2 : 0);
        le(b, w.align, 4);
    }
}

static Bytes build_int(const std::string& name, const std::vector<WSpec>& roots, int ver) {
    Bytes b;
    const std::string sig = "GUI V1." + std::to_string(ver % 10);
    b.insert(b.end(), sig.begin(), sig.end());
    lestr(b, name);
    b.push_back(0);
    le(b, static_cast<std::uint32_t>(roots.size()), 4);
    for (const auto& r : roots) build_widget(b, r, ver);
    return b;
}

static WSpec sample_screen() {
    WSpec win; win.kind = 0; win.name = "MainWin"; win.skin = "Default.ini"; win.x0 = 0; win.y0 = 0; win.x1 = 1024; win.y1 = 768;
    win.tex[4] = "fundo.tga";
    WSpec btn; btn.kind = 1; btn.name = "btnNew"; btn.skin = "Default.ini"; btn.x0 = 100; btn.y0 = 200; btn.x1 = 300; btn.y1 = 240;
    btn.tex[4] = "botao.tga"; btn.sounds[0] = "f3_button_down_01"; btn.sounds[1] = "f3_button_up_01";
    WSpec up; up.kind = 2; up.name = "picUp"; up.x0 = 100; up.y0 = 200; up.x1 = 300; up.y1 = 240; up.tex[4] = "seta_up.tga";
    WSpec down = up; down.name = "picDown"; down.tex[4] = "seta_down.tga";
    btn.children = {up, down};
    WSpec lbl; lbl.kind = 3; lbl.name = "lblTitulo"; lbl.x0 = 100; lbl.y0 = 50; lbl.x1 = 500; lbl.y1 = 90; lbl.text_id = 7; lbl.font_size = 18; lbl.align = 1;
    lbl.tex[4] = "botao.tga";                                                   // rotulo: a moldura NUNCA e desenhada (v20, como o original)
    WSpec hidden; hidden.kind = 2; hidden.name = "picInvisivel"; hidden.x0 = 0; hidden.y0 = 0; hidden.x1 = 10; hidden.y1 = 10;
    hidden.tex[4] = "fundo.tga"; hidden.frag_rgba[3] = 0;                       // alfa 0 no .int = escondido de proposito
    WSpec edit; edit.kind = 4; edit.name = "edtNome"; edit.x0 = 600; edit.y0 = 600; edit.x1 = 800; edit.y1 = 630; edit.text_id = 0;
    edit.tex[4] = "zz_inexistente.tga";                                         // textura que nao existe: nao desenha nada
    win.children = {btn, lbl, hidden, edit};
    return win;
}

static void test_int_format() {
    for (int ver : {11, 12, 13}) {
        const Bytes data = build_int("Teste", {sample_screen()}, ver);
        GuiFile f; std::string err;
        CHECK(load_gui(data.data(), data.size(), f, err));
        if (!err.empty()) std::printf("  (v%d) erro: %s\n", ver, err.c_str());
        CHECK(f.version == ver);
        CHECK(f.name == "Teste");
        CHECK(f.count() == 7);
        GuiWidget* b = f.find("BTNNEW");                                        // busca sem diferenciar maiusculas
        CHECK(b && b->is(WidgetKind::Button));
        if (b) {
            CHECK(b->rect.x0 == 100 && b->rect.y1 == 240 && b->rect.w() == 200 && b->rect.h() == 40);
            CHECK(b->children.size() == 2);
            CHECK(b->frag[4].texture == "botao.tga");
            CHECK(b->parent && b->parent->name == "MainWin");
            if (ver >= 13) CHECK(b->sounds[0] == "f3_button_down_01" && b->sounds[1] == "f3_button_up_01");
            else CHECK(b->sounds[0].empty());
        }
        GuiWidget* l = f.find("lblTitulo");
        CHECK(l && l->text_id == 7 && l->font == "Arial" && l->font_size == 18 && l->align == 1);
        CHECK(l && l->text_rgba[0] == 255 && l->text_rgba[1] == 200 && l->text_rgba[2] == 100);
        CHECK(l && l->text_tail[2] == 2 && l->halign() == 1);
        int depth_max = 0;
        f.walk([&](const GuiWidget&, int d) { if (d > depth_max) depth_max = d; });
        CHECK(depth_max == 2);
        // truncar em qualquer ponto precisa falhar com erro, nunca travar
        int ok_truncated = 0;
        for (std::size_t cut = 0; cut < data.size(); cut += 7) {
            GuiFile g; std::string e2;
            if (load_gui(data.data(), cut, g, e2)) ++ok_truncated;
        }
        CHECK(ok_truncated == 0);
    }
    {
        Bytes bad = {'G', 'U', 'I', ' ', 'V', '2', '.', '0', 0, 0, 0, 0, 0, 0, 0, 0};
        GuiFile f; std::string err;
        CHECK(!load_gui(bad.data(), bad.size(), f, err) && !err.empty());
    }
    CHECK(lower_ascii("BtnNEW.TGA") == "btnnew.tga");
}

// =================================================================== 3. skin
static void test_skin() {
    Skin s;
    CHECK(!s.parse("nada aqui\n"));
    const char* ini =
        "; comentario\r\n"
        "[Button.Selected.Middle]\r\n"
        "flags = 3\r\n"
        "texture = botao_luz.tga ; brilho\r\n"
        "color = 200, 100, 50\r\n"
        "width=16\r\n"
        "height=12\r\n"
        "[button.up.middle]\r\n"
        "texture=botao.tga\r\n";
    CHECK(s.parse(ini));
    CHECK(s.size() == 2);
    const SkinFragment* f = s.get("button", "selected", 4);
    CHECK(f && f->texture == "botao_luz.tga" && f->flags == 3 && f->width == 16 && f->height == 12);
    CHECK(f && f->has_color && f->rgb[0] == 200 && f->rgb[1] == 100 && f->rgb[2] == 50);
    CHECK(s.has_state("button", "selected"));
    CHECK(!s.has_state("button", "down"));
    CHECK(s.get("button", "selected", 0) == nullptr);
    CHECK(std::string(Skin::fragment_name(0)) == "upperleft" && std::string(Skin::fragment_name(8)) == "lowerright");
}

// =================================================================== 4. TrueType minima gerada aqui
// glifo 0 vazio, glifo 1 = quadrado (letra 'A'), glifo 2 = triangulo (letra 'B'); kern A,B = -100
struct Table { std::string tag; Bytes data; };

static Bytes glyph_square() {
    Bytes g;
    be(g, 1, 2); be(g, 0, 2); be(g, 0, 2); be(g, 500, 2); be(g, 700, 2);    // 1 contorno, bbox
    be(g, 3, 2);                                                            // ultimo ponto = 3
    be(g, 0, 2);                                                            // sem instrucoes
    for (int i = 0; i < 4; ++i) g.push_back(1);                             // on-curve, coords de 16 bits
    const int xs[4] = {0, 500, 500, 0}, ys[4] = {0, 0, 700, 700};
    int px = 0; for (int x : xs) { be(g, static_cast<std::uint32_t>(static_cast<std::uint16_t>(x - px)), 2); px = x; }
    int py = 0; for (int y : ys) { be(g, static_cast<std::uint32_t>(static_cast<std::uint16_t>(y - py)), 2); py = y; }
    while (g.size() % 4) g.push_back(0);
    return g;
}
static Bytes glyph_triangle() {
    Bytes g;
    be(g, 1, 2); be(g, 0, 2); be(g, 0, 2); be(g, 600, 2); be(g, 700, 2);
    be(g, 2, 2); be(g, 0, 2);
    for (int i = 0; i < 3; ++i) g.push_back(1);
    const int xs[3] = {0, 600, 300}, ys[3] = {0, 0, 700};
    int px = 0; for (int x : xs) { be(g, static_cast<std::uint32_t>(static_cast<std::uint16_t>(x - px)), 2); px = x; }
    int py = 0; for (int y : ys) { be(g, static_cast<std::uint32_t>(static_cast<std::uint16_t>(y - py)), 2); py = y; }
    while (g.size() % 4) g.push_back(0);
    return g;
}

static Bytes build_ttf() {
    std::vector<Table> t;
    const Bytes g1 = glyph_square(), g2 = glyph_triangle();
    {   // head (54 bytes): unitsPerEm=1000 no offset 18, indexToLocFormat=1 no offset 50
        Bytes h(54, 0);
        h[0] = 0; h[1] = 1; h[18] = 0x03; h[19] = 0xE8; h[51] = 1;
        t.push_back({"head", h});
    }
    {   // hhea (36): ascent 800, descent -200, gap 0, numberOfHMetrics 3
        Bytes h(36, 0);
        h[1] = 1; h[4] = 0x03; h[5] = 0x20; h[6] = 0xFF; h[7] = 0x38; h[34] = 0; h[35] = 3;
        t.push_back({"hhea", h});
    }
    {   Bytes m; be(m, 0x00010000, 4); be(m, 3, 2); m.resize(32, 0); t.push_back({"maxp", m}); }
    {   Bytes m; be(m, 0, 2); be(m, 0, 2); be(m, 600, 2); be(m, 0, 2); be(m, 700, 2); be(m, 0, 2); t.push_back({"hmtx", m}); }
    {   Bytes l; be(l, 0, 4); be(l, 0, 4); be(l, static_cast<std::uint32_t>(g1.size()), 4); be(l, static_cast<std::uint32_t>(g1.size() + g2.size()), 4); t.push_back({"loca", l}); }
    {   Bytes g = g1; g.insert(g.end(), g2.begin(), g2.end()); t.push_back({"glyf", g}); }
    {   // cmap: (3,1) formato 4 com segmentos [A..B] (delta 1-65) e [FFFF]
        Bytes c; be(c, 0, 2); be(c, 1, 2); be(c, 3, 2); be(c, 1, 2); be(c, 12, 4);
        const int seg = 2;
        Bytes s; be(s, 4, 2); be(s, 0, 2); be(s, 0, 2); be(s, seg * 2, 2); be(s, 2, 2); be(s, 0, 2); be(s, 2, 2);
        be(s, 'B', 2); be(s, 0xFFFF, 2); be(s, 0, 2);                             // ends, reservado
        be(s, 'A', 2); be(s, 0xFFFF, 2);                                          // starts
        be(s, static_cast<std::uint16_t>(1 - 'A'), 2); be(s, 1, 2);               // deltas
        be(s, 0, 2); be(s, 0, 2);                                                 // rangeOffsets
        s[2] = static_cast<std::uint8_t>(s.size() >> 8); s[3] = static_cast<std::uint8_t>(s.size());
        c.insert(c.end(), s.begin(), s.end());
        t.push_back({"cmap", c});
    }
    {   // kern versao 0, 1 subtabela formato 0, horizontal: par (1,2) = -100
        Bytes k; be(k, 0, 2); be(k, 1, 2);
        be(k, 0, 2); be(k, 6 + 8 + 6, 2); be(k, 1, 2);
        be(k, 1, 2); be(k, 6, 2); be(k, 0, 2); be(k, 0, 2);
        be(k, 1, 2); be(k, 2, 2); be(k, static_cast<std::uint16_t>(-100), 2);
        t.push_back({"kern", k});
    }
    {   // name: id 1 (familia) e id 4 (nome completo), plataforma 3 UTF-16BE
        const std::string fam = "TesteFonte", full = "Teste Fonte Regular";
        Bytes strs;
        auto u16s = [&](const std::string& s) { for (char ch : s) { strs.push_back(0); strs.push_back(static_cast<std::uint8_t>(ch)); } };
        Bytes n; be(n, 0, 2); be(n, 2, 2); be(n, 6 + 24, 2);
        be(n, 3, 2); be(n, 1, 2); be(n, 0x409, 2); be(n, 1, 2); be(n, static_cast<std::uint32_t>(fam.size() * 2), 2); be(n, 0, 2);
        be(n, 3, 2); be(n, 1, 2); be(n, 0x409, 2); be(n, 4, 2); be(n, static_cast<std::uint32_t>(full.size() * 2), 2); be(n, static_cast<std::uint32_t>(fam.size() * 2), 2);
        u16s(fam); u16s(full);
        n.insert(n.end(), strs.begin(), strs.end());
        t.push_back({"name", n});
    }
    Bytes out;
    be(out, 0x00010000, 4); be(out, static_cast<std::uint32_t>(t.size()), 2); be(out, 0, 2); be(out, 0, 2); be(out, 0, 2);
    std::uint32_t off = 12 + 16 * static_cast<std::uint32_t>(t.size());
    for (auto& tb : t) {
        out.insert(out.end(), tb.tag.begin(), tb.tag.end());
        be(out, 0, 4); be(out, off, 4); be(out, static_cast<std::uint32_t>(tb.data.size()), 4);
        off += static_cast<std::uint32_t>((tb.data.size() + 3) & ~3u);
    }
    for (auto& tb : t) { out.insert(out.end(), tb.data.begin(), tb.data.end()); while (out.size() % 4) out.push_back(0); }
    return out;
}

static void test_truetype() {
    const Bytes data = build_ttf();
    TrueTypeFont f; std::string err;
    CHECK(f.load(data.data(), data.size(), err));
    if (!err.empty()) std::printf("  ttf: %s\n", err.c_str());
    CHECK(f.ok());
    CHECK(f.family() == "TesteFonte");
    CHECK(f.full_name() == "Teste Fonte Regular");
    CHECK(f.units_per_em() == 1000 && f.ascent() == 800 && f.descent() == -200);
    CHECK(f.glyph_count() == 3);
    CHECK(f.glyph_index('A') == 1 && f.glyph_index('B') == 2 && f.glyph_index('C') == 0);
    CHECK(f.advance(1) == 600 && f.advance(2) == 700);
    CHECK(f.kerning(1, 2) == -100 && f.kerning(2, 1) == 0);
    std::vector<std::vector<TrueTypeFont::Point>> cont;
    CHECK(f.outline(1, cont, 0.5f) && cont.size() == 1);
    // rasterizar o quadrado a 0.02 px/unidade = 10x14 px: o miolo precisa estar cheio e a borda de fora vazia
    TrueTypeFont::Bitmap bm;
    CHECK(f.rasterize(1, 0.02f, bm));
    CHECK(bm.w >= 10 && bm.w <= 12 && bm.h >= 14 && bm.h <= 16);
    if (bm.w > 0 && bm.h > 0) {
        int full = 0, total = 0;
        for (int y = 1; y < bm.h - 1; ++y) for (int x = 1; x < bm.w - 1; ++x) { ++total; if (bm.alpha[static_cast<std::size_t>(y * bm.w + x)] >= 250) ++full; }
        CHECK(full * 10 >= total * 7);                       // pelo menos 70% cheio por dentro (bordas antialiasadas)
        CHECK(bm.y0 < 0);                                     // fica acima da linha de base (y para baixo)
    }
    // atlas: so 'A' e 'B' existem; largura de "AB" = (600 + 700 - 100) * escala
    FontAtlas a;
    CHECK(bake_atlas(f, 20.f, a));
    CHECK(a.glyph[static_cast<unsigned char>('A')].present && a.glyph[static_cast<unsigned char>('B')].present);
    const float sc = 20.f / 1000.f;
    CHECK(std::fabs(a.measure("AB") - (600 + 700 - 100) * sc) < 0.6f);
    CHECK(std::fabs(a.ascent - 16.f) < 0.01f && std::fabs(a.line - 20.f) < 0.01f);
    CHECK(cp1252_to_unicode(0x80) == 0x20AC && cp1252_to_unicode('A') == 'A' && cp1252_to_unicode(0xE9) == 0xE9);
    // fontes quebradas: erro, sem travar
    Bytes bad = data; bad[0] = 'O'; bad[1] = 'T'; bad[2] = 'T'; bad[3] = 'O';
    TrueTypeFont g;
    CHECK(!g.load(bad.data(), bad.size(), err));
    int ok_cut = 0;
    for (std::size_t cut = 0; cut < data.size(); cut += 13) { TrueTypeFont h; std::string e; if (h.load(data.data(), cut, e)) { ++ok_cut; TrueTypeFont::Bitmap b2; (void)h.rasterize(1, 0.02f, b2); } }
    (void)ok_cut;                                             // pode carregar parcialmente; o importante e nao travar
}

// =================================================================== 5. montagem da tela, clique e deslocamento
static GuiResources fake_resources(int& text_calls) {
    GuiResources r;
    r.texture_size = [](const std::string& tex, int& w, int& h) {
        if (tex.rfind("zz", 0) == 0) return false;            // "zz_inexistente.tga" nao existe
        w = 64; h = 32; return true;
    };
    r.text_width = [](const std::string&, float px, const std::string& s) { return static_cast<float>(s.size()) * px * 0.5f; };
    r.line_height = [](const std::string&, float px) { return px * 1.25f; };
    r.text = [&text_calls](std::uint32_t id) { ++text_calls; return id == 7 ? std::string("Titulo de teste") : std::string(); };
    r.skin = [](const std::string&) -> const Skin* {
        static Skin s;
        static bool init = false;
        if (!init) { s.parse("[button.selected.middle]\ntexture=botao_luz.tga\ncolor=255,255,255\n[button.down.middle]\ntexture=botao_aperto.tga\n"); init = true; }
        return &s;
    };
    return r;
}

static std::size_t count_tex(const std::vector<GuiDraw>& d, const std::string& t) { std::size_t n = 0; for (const auto& x : d) if (x.kind == GuiDraw::Quad && x.texture == t) ++n; return n; }

static void test_screen() {
    const Bytes data = build_int("Teste", {sample_screen()}, 13);
    GuiScreen s; std::string err;
    CHECK(s.load(data.data(), data.size(), err));
    int text_calls = 0;
    const GuiResources res = fake_resources(text_calls);
    std::vector<GuiDraw> d;
    s.build(res, d);
    CHECK(count_tex(d, "fundo.tga") == 1);                   // a imagem com alfa 0 NAO aparece
    CHECK(count_tex(d, "botao.tga") == 1);                   // so a do botao: a do rotulo nao aparece
    CHECK(count_tex(d, "seta_up.tga") == 1 && count_tex(d, "seta_down.tga") == 0);   // botao solto: so a imagem "up"
    for (const auto& x : d) if (x.texture == "seta_up.tga") CHECK(std::fabs(x.x0 - 100) < 0.01f && std::fabs(x.x1 - 300) < 0.01f);   // imagem: so o miolo
    CHECK(s.stats().missing == 1 && s.stats().missing_names.count("zz_inexistente.tga") == 1);
    std::size_t texts = 0;
    for (const auto& x : d) if (x.kind == GuiDraw::Text) {
        ++texts;
        CHECK(x.text == "Titulo de teste" && x.align == 1);
        CHECK(std::fabs(x.size_px - 24.f) < 0.01f);           // 18 pt * 4/3
        CHECK(x.cx0 == 100.f && x.cx1 == 500.f);                // recorte = retangulo do rotulo
        CHECK(x.rgba[0] == 200 && x.rgba[1] == 255 && x.rgba[2] == 255 && x.rgba[3] == 255);   // D3DCOLOR: bytes B,G,R,A = 255,200,100 -> RGB x2
    }
    CHECK(texts == 1);
    // cor do fragmento: 128 = neutro (x2 = 255)
    for (const auto& x : d) if (x.kind == GuiDraw::Quad && x.texture == "botao.tga") CHECK(x.rgba[0] == 255 && x.rgba[3] == 255 && !x.additive);

    // clique: so botoes e campos respondem
    GuiWidget* b = s.find("btnNew");
    CHECK(s.hit(150, 220) == b);
    CHECK(s.hit(150, 70) == nullptr);                         // rotulo nao e clicavel
    CHECK(s.hit(700, 610) == s.find("edtNome"));
    CHECK(s.hit(5, 5) == nullptr);

    // passar o mouse: estado "selected" da skin desenhado por cima, somando luz
    s.set_hover(b);
    CHECK(s.state_of(b) == ButtonState::Selected);
    d.clear(); s.build(res, d);
    CHECK(count_tex(d, "botao_luz.tga") == 1);
    for (const auto& x : d) if (x.texture == "botao_luz.tga") CHECK(x.additive);
    // apertar: estado "down" e a imagem "down" troca com a "up"
    s.set_pressed(b);
    CHECK(s.state_of(b) == ButtonState::Down);
    d.clear(); s.build(res, d);
    CHECK(count_tex(d, "botao_aperto.tga") == 1);
    CHECK(count_tex(d, "seta_down.tga") == 1 && count_tex(d, "seta_up.tga") == 0);
    // desabilitado: nao responde ao clique
    s.set_pressed(nullptr); s.set_hover(nullptr);
    s.set_disabled("btnNew", true);
    CHECK(s.state_of(b) == ButtonState::Disabled);
    CHECK(s.hit(150, 220) == nullptr);
    s.set_disabled("btnNew", false);

    // deslocamento do widget (cursor de slider) e da tela inteira (barra de acoes embaixo)
    s.set_offset("btnNew", 50, 0);
    CHECK(s.hit(120, 220) == nullptr && s.hit(320, 220) == b);
    s.set_offset("btnNew", 0, 0);
    s.offset_y = 604;
    CHECK(s.hit(150, 220) == nullptr && s.hit(150, 220 + 604) == b);
    d.clear(); s.build(res, d);
    for (const auto& x : d) if (x.texture == "botao.tga") CHECK(std::fabs(x.y0 - (200 + 604)) < 0.01f);
    s.offset_y = 0;

    // texto trocado pelo jogo, imagem trocada e escondido
    s.set_text("edtNome", "Prisioneiro");
    s.set_image("btnNew", "f3_digit_5.tga");
    s.set_hidden("lblTitulo", true);
    d.clear(); s.build(res, d);
    bool name_ok = false; for (const auto& x : d) if (x.kind == GuiDraw::Text && x.text == "Prisioneiro") name_ok = true;
    CHECK(name_ok);
    CHECK(count_tex(d, "f3_digit_5.tga") == 1);
    for (const auto& x : d) if (x.kind == GuiDraw::Text) CHECK(x.text != "Titulo de teste");

    // quebra de linha
    auto m = [](const std::string& t) { return static_cast<float>(t.size()); };
    const auto lines = wrap_text("um dois tres quatro", 9.f, m);
    CHECK(lines.size() == 3 && lines[0] == "um dois" && lines[1] == "tres" && lines[2] == "quatro");
    CHECK(wrap_text("a\nb", 100.f, m).size() == 2);
    CHECK(wrap_text("", 100.f, m).size() == 1);
    CHECK(std::fabs(font_px(12) - 16.f) < 0.01f && std::fabs(font_px(0) - 12.f) < 0.01f);
}

// =================================================================== 6. criacao de personagem (casos reais: os 2 prints da tela original)
static void test_chargen() {
    Character c;
    CHECK(c.points == 5 && !c.ready() && c.name == "None" && c.age == 21);
    CHECK(std::string(special_code(S_ST)) == "ST" && std::string(special_code(S_LK)) == "LK");
    // print 1 (personagem "None", todos 5, nada marcado): Base / Total
    const int base1[13] = {80, 80, 80, 0, 75, 0, 0, 75, 0, 0, 0, 0, 0};
    const int total1[13] = {130, 130, 95, 15, 90, 15, 15, 90, 15, 15, 15, 15, 15};
    for (int k = 1; k <= 13; ++k) { CHECK(c.skill_base(k) == base1[k - 1]); CHECK(c.skill_total(k) == total1[k - 1]); }
    const char* der1[14] = {"25", "150", "10", "5%", "0", "5", "+5", "55", "1/3", "15/1", "15", "0%", "0%", "0%"};
    for (int i = 1; i <= 14; ++i) CHECK(c.derived(i) == der1[i - 1]);
    CHECK(c.hit_points() == 100);
    // print 2 ("Test"): ST..IN 6, AG/LK 5, Firearms, Barter e Mechanics marcadas
    for (int s = S_ST; s <= S_IN; ++s) CHECK(c.inc(s));
    CHECK(c.points == 0 && !c.inc(S_AG));
    CHECK(c.toggle_tag(1) && c.toggle_tag(4) && c.toggle_tag(7));
    CHECK(!c.toggle_tag(2));                                  // so 3 pericias marcadas
    CHECK(c.ready());
    const int base2[13] = {100, 80, 80, 20, 75, 0, 20, 75, 0, 0, 0, 0, 0};
    const int total2[13] = {150, 130, 98, 38, 93, 18, 38, 93, 18, 18, 17, 17, 16};
    for (int k = 1; k <= 13; ++k) { CHECK(c.skill_base(k) == base2[k - 1]); CHECK(c.skill_total(k) == total2[k - 1]); }
    const char* der2[14] = {"25", "175", "12", "5%", "0", "6", "+6", "58", "1/3", "17/1", "18", "0%", "0%", "0%"};
    for (int i = 1; i <= 14; ++i) CHECK(c.derived(i) == der2[i - 1]);
    // regras de limite
    CHECK(c.dec(S_IN) && c.points == 1 && c.inc(S_IN));
    Character d;
    for (int i = 0; i < 4; ++i) CHECK(d.dec(S_ST));
    CHECK(d.special[S_ST] == 1 && !d.dec(S_ST));               // minimo 1
    for (int i = 0; i < 5; ++i) d.inc(S_IN);
    CHECK(d.special[S_IN] == 10 && !d.inc(S_IN));              // maximo 10
    CHECK(d.toggle_trait(11) && d.toggle_trait(12) && !d.toggle_trait(13));
    CHECK(d.toggle_trait(11) && d.traits.size() == 1);
    CHECK(Character::descriptor_text_id(1) == 41 && Character::descriptor_text_id(5) == 45 && Character::descriptor_text_id(99) == 50);
    // .CRT sintetico no formato do Default_PC (GCRE v4: so as pericias diferentes de 0)
    Bytes crt;
    const char* t1 = "EEN2"; crt.insert(crt.end(), t1, t1 + 4); le(crt, 0, 4); le(crt, 12, 4);
    Bytes g;
    for (int i = 0; i < 7; ++i) le(g, 6, 4);
    le(g, 1, 4); le(g, 0, 4); le(g, 0, 4); le(g, 0, 4); le(g, 33, 4); le(g, 0, 4); le(g, 6, 4); le(g, 1, 4);
    le(g, 2, 4); le(g, 3, 4); le(g, 55, 4); le(g, 12, 4); le(g, 44, 4);   // 2 pericias: Barter (3) = 55, Steal (12) = 44
    const char* t2 = "GCRE"; crt.insert(crt.end(), t2, t2 + 4); le(crt, 4, 4); le(crt, static_cast<std::uint32_t>(12 + g.size()), 4);
    crt.insert(crt.end(), g.begin(), g.end());
    Character e;
    CHECK(e.load_crt(crt.data(), crt.size(), false));
    CHECK(e.age == 33 && e.special[S_ST] == 5 && e.skill_points[3] == 55 && e.skill_points[12] == 44 && e.skill_points[0] == 0);
    CHECK(e.load_crt(crt.data(), crt.size(), true) && e.special[S_ST] == 6);
    CHECK(!e.load_crt(crt.data(), 20, false));
}

int main() {
    test_images();
    test_int_format();
    test_skin();
    test_truetype();
    test_screen();
    test_chargen();
    if (fails == 0) std::puts("ui tests OK");
    return fails ? 1 : 0;
}
