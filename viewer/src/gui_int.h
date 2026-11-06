// Leitor do formato .int (layout de uma tela da interface, recurso INT_Interface) - v19. Spec: docs/specs/int.md.
// Portavel (so C++ padrao), com checagem de limites em tudo: arquivo corrompido devolve false com a mensagem em `err`.
// Mesma gramatica do leitor Python (tools/py/f3vb/intfile.py), que le os 25 arquivos reais ate o ultimo byte.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace f3v {

enum class WidgetKind : std::uint32_t { Window = 0, Button = 1, Picture = 2, Label = 3, Edit = 4 };
const char* widget_kind_name(WidgetKind k);

struct IRect { std::int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0; std::int32_t w() const { return x1 - x0; } std::int32_t h() const { return y1 - y0; } };

// Um dos 9 fragmentos (nove fatias) de um objeto. flags: 0 esticar, 1 repetir em x, 2 repetir em y, 3 repetir nos dois.
struct GuiFragment {
    std::uint32_t flags = 0;
    std::int32_t width = 0, height = 0;          // recorte da textura em pixels
    std::string texture;                          // "" = sem textura
    std::uint8_t rgba[4] = {255, 255, 255, 255};  // alfa 0 = invisivel neste estado
    IRect rect;                                   // absoluto na tela de 1024x768
};

struct GuiWidget {
    WidgetKind kind = WidgetKind::Window;
    std::string name, skin;
    IRect rect, rel;                              // rect = absoluto (1024x768); rel = relativo (nao usado para desenhar)
    std::uint8_t color[4] = {128, 128, 128, 255};
    std::uint32_t interactive = 0;                // 1 em janelas/botoes/campos (hipotese)
    std::int32_t tip_a = -1, tip_text = -1;       // V1.2+: ids da dica (tooltip)
    std::string tip_texture;
    std::array<GuiFragment, 9> frag;              // upperleft, uppermiddle, upperright, left, middle, right, lowerleft, lowermiddle, lowerright
    std::vector<std::unique_ptr<GuiWidget>> children;
    // botao
    std::string sounds[3];                        // apertar, soltar, soltar (V1.3)
    // rotulo / edicao
    std::uint32_t text_id = 0;                    // 1-based no .stf (0 = sem texto)
    std::string font;
    std::uint32_t font_size = 0;
    std::uint8_t text_rgba[4] = {255, 255, 255, 255};
    std::uint32_t align = 0;                      // u32 final do sufixo: 1 ou 2 nos arquivos; v20: NAO e o alinhamento (significado desconhecido)
    std::uint8_t text_tail[22] = {0};             // v20: bytes do sufixo; text_tail[2] = alinhamento horizontal (0 esquerda, 1 direita, 2 centro), medido nos prints
    int halign() const { return text_tail[2] == 1 ? 2 : (text_tail[2] == 2 ? 1 : 0); }   // 0 esquerda, 1 centro, 2 direita (convencao do GuiDraw)
    GuiWidget* parent = nullptr;

    bool is(WidgetKind k) const { return kind == k; }
};

struct GuiFile {
    int version = 0;                              // 11, 12 ou 13
    std::string name;
    std::vector<std::unique_ptr<GuiWidget>> roots;

    // Busca por nome (sem diferenciar maiusculas), em profundidade. nullptr se nao achar.
    GuiWidget* find(const std::string& name) const;
    std::size_t count() const;
    template <class F> void walk(F&& f) const { for (const auto& r : roots) walk_one(r.get(), 0, f); }

private:
    template <class F> static void walk_one(GuiWidget* w, int depth, F& f) { f(*w, depth); for (const auto& c : w->children) walk_one(c.get(), depth + 1, f); }
};

bool load_gui(const std::uint8_t* data, std::size_t size, GuiFile& out, std::string& err);
bool load_gui_file(const std::string& path, GuiFile& out, std::string& err);

std::string lower_ascii(const std::string& s);

}  // namespace f3v
