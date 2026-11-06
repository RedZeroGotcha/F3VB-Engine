// Interface do jogo, parte PORTAVEL (v19): transforma uma tela .int (gui_int.h) + skins (.ini) + estado (mouse, botoes) numa lista de desenho.
// Quem desenha e o "backend": OpenGL no F3VB.exe (gui_gl) e CPU no F3VBUiPreview (gera PNG). Assim o layout e testavel sem placa de video.
//
// Regras (o que foi medido e o que e suposicao; ver docs/specs/int.md e docs/JOGO.md):
//  - Coordenadas: tela virtual de 1024x768 (a do .int). O backend escala para a janela.
//  - Nove fatias: o "meio" cobre o retangulo inteiro do objeto (como gravado no .int) e e desenhado primeiro; bordas e cantos por cima, por DENTRO
//    do retangulo, com o tamanho de cada fragmento (largura/altura gravadas). flags 1/2/3 repetem a textura em x/y (coordenadas de textura > 1).
//  - Textura que NAO existe nos dados nao e desenhada (medido: o Mainmenu.int pede zzmainpicture-*.bmp e a skin pede zzmainlabel-*.tga, e nenhum existe:
//    no jogo original essas molduras nao apareciam). Fragmento sem textura tambem nao.
//  - Cor do fragmento: RGB x 2 (0x80 = neutro, como o _2X dos materiais), alfa como esta (0 = invisivel). SUPOSICAO.
//  - Botoes: o .int guarda o estado "up"; selected (mouse em cima), down (apertado) e disabled vem da skin, desenhados com mistura ADITIVA
//    (a textura "selected" do zzmainmenu.ini e azul-escuro opaca: somada, vira um brilho azul sobre o desenho do botao). SUPOSICAO.
//  - Filhos-imagem de um botao com "down"/"checked" no nome so aparecem com o botao apertado/marcado; com "up", so solto (CharGen, menu ESC).
//  - Texto dos rotulos: id 1-based no .stf; fonte/tamanho/cor do .int; tamanho em pontos -> pixels x 4/3 (96 dpi); alinhamento 0/1/2 =
//    esquerda/centro/direita (como DT_LEFT/DT_CENTER/DT_RIGHT do Windows: SUPOSICAO); quebra de linha por palavras; uma linha = centro vertical.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gui_int.h"
#include "skin.h"

namespace f3v {

struct GuiDraw {
    enum Kind { Quad, Text } kind = Quad;
    // Quad
    std::string texture;                     // nome do arquivo (minusculo), resolvido pelo backend
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;    // tela virtual 1024x768
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;    // > 1 = repetir
    std::uint8_t rgba[4] = {255, 255, 255, 255};
    bool additive = false;
    // Text (x0..y1 = caixa do rotulo)
    std::string font, text;                  // text em cp1252
    float size_px = 12;
    int align = 0;                           // 0 esquerda, 1 centro, 2 direita
    float cx0 = -1e9f, cy0 = -1e9f, cx1 = 1e9f, cy1 = 1e9f;   // v20: recorte do texto (o retangulo do rotulo): o original corta, nao quebra
    const void* widget = nullptr;
};

enum class ButtonState { Up, Selected, Down, Disabled };

// O que o backend sabe responder: tamanho de uma textura (false = nao existe) e largura de um texto numa fonte/tamanho.
struct GuiResources {
    std::function<bool(const std::string& tex, int& w, int& h)> texture_size;
    std::function<float(const std::string& font, float px, const std::string& text)> text_width;
    std::function<float(const std::string& font, float px)> line_height;
    std::function<std::string(std::uint32_t text_id)> text;           // id 1-based do .stf -> texto cp1252 ("" se nao achar)
    std::function<const Skin*(const std::string& skin_file)> skin;    // nullptr se nao houver
};

class GuiScreen {
public:
    bool load(const std::uint8_t* data, std::size_t size, std::string& err);
    bool load_file(const std::string& path, std::string& err);
    const GuiFile& file() const { return file_; }
    GuiWidget* find(const std::string& name) const { return file_.find(name); }

    // estado
    void set_hover(const GuiWidget* w) { hover_ = w; }
    void set_pressed(const GuiWidget* w) { pressed_ = w; }
    const GuiWidget* hover() const { return hover_; }
    const GuiWidget* pressed() const { return pressed_; }
    void set_disabled(const std::string& name, bool v);
    void set_checked(const std::string& name, bool v);
    void set_hidden(const std::string& name, bool v);
    void set_text(const std::string& name, const std::string& cp1252);   // troca o texto de um rotulo/campo
    void set_image(const std::string& name, const std::string& texture);  // desenha esta textura esticada sobre o widget (ex.: digito, imagem de loading)
    void clear_image(const std::string& name);
    void set_offset(const std::string& name, float dx, float dy);        // desloca o widget e os filhos (ex.: cursor de um slider)
    void get_offset(const std::string& name, float& dx, float& dy) const;
    void clear_text(const std::string& name);
    bool is_disabled(const GuiWidget* w) const;
    bool is_checked(const GuiWidget* w) const;
    bool is_hidden(const GuiWidget* w) const;
    ButtonState state_of(const GuiWidget* w) const;

    // Botao (ou campo de edicao) mais de cima sob o ponto (tela virtual). nullptr = nenhum. Ignora ocultos e desabilitados.
    GuiWidget* hit(float x, float y) const;
    // Monta a lista de desenho (ordem = ordem de pintura).
    void build(const GuiResources& res, std::vector<GuiDraw>& out) const;
    // Estatisticas do ultimo build (para o log): fragmentos desenhados, pulados por textura ausente, rotulos com texto.
    struct Stats { std::size_t quads = 0, missing = 0, texts = 0; std::set<std::string> missing_names; };
    const Stats& stats() const { return stats_; }

    // Deslocamento da tela inteira em pixels virtuais (ex.: centralizar uma MessageBox de 386x162 na tela).
    float offset_x = 0, offset_y = 0;

private:
    void build_widget(const GuiWidget& w, const GuiResources& res, std::vector<GuiDraw>& out, bool parent_down, bool parent_checked, float ox, float oy) const;
    void build_frags(const GuiWidget& w, const GuiResources& res, std::vector<GuiDraw>& out, const SkinFragment* const* skin_state, bool additive, float ox, float oy) const;
    void build_text(const GuiWidget& w, const GuiResources& res, std::vector<GuiDraw>& out, float ox, float oy) const;

    GuiFile file_;
    const GuiWidget* hover_ = nullptr;
    const GuiWidget* pressed_ = nullptr;
    std::set<std::string> disabled_, checked_, hidden_;
    std::map<std::string, std::string> text_, image_;
    std::map<std::string, std::pair<float, float>> woff_;
    mutable Stats stats_;
};

// Quebra um texto em linhas que cabem em `width` (por palavras; palavra maior que a largura fica sozinha na linha).
std::vector<std::string> wrap_text(const std::string& text, float width, const std::function<float(const std::string&)>& measure);
// Tamanho da fonte do .int (pontos) -> altura do "em" em pixels (96 dpi).
inline float font_px(std::uint32_t points) { return points ? static_cast<float>(points) * 4.f / 3.f : 12.f; }

}  // namespace f3v
