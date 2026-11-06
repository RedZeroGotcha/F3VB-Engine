#include "gui.h"

#include <algorithm>
#include <cmath>

namespace f3v {

namespace {

const char* state_name(ButtonState s) {
    switch (s) {
        case ButtonState::Up: return "up";
        case ButtonState::Selected: return "selected";
        case ButtonState::Down: return "down";
        case ButtonState::Disabled: return "disabled";
    }
    return "up";
}

std::uint8_t x2(std::uint8_t v) { return static_cast<std::uint8_t>(std::min(255, v * 2)); }
bool contains(const std::string& low, const char* s) { return low.find(s) != std::string::npos; }

}  // namespace

bool GuiScreen::load(const std::uint8_t* data, std::size_t size, std::string& err) {
    hover_ = pressed_ = nullptr;
    return load_gui(data, size, file_, err);
}

bool GuiScreen::load_file(const std::string& path, std::string& err) {
    hover_ = pressed_ = nullptr;
    return load_gui_file(path, file_, err);
}

void GuiScreen::set_disabled(const std::string& n, bool v) { if (v) disabled_.insert(lower_ascii(n)); else disabled_.erase(lower_ascii(n)); }
void GuiScreen::set_checked(const std::string& n, bool v) { if (v) checked_.insert(lower_ascii(n)); else checked_.erase(lower_ascii(n)); }
void GuiScreen::set_hidden(const std::string& n, bool v) { if (v) hidden_.insert(lower_ascii(n)); else hidden_.erase(lower_ascii(n)); }
void GuiScreen::set_text(const std::string& n, const std::string& t) { text_[lower_ascii(n)] = t; }
void GuiScreen::clear_text(const std::string& n) { text_.erase(lower_ascii(n)); }
void GuiScreen::set_image(const std::string& n, const std::string& t) { image_[lower_ascii(n)] = lower_ascii(t); }
void GuiScreen::clear_image(const std::string& n) { image_.erase(lower_ascii(n)); }
void GuiScreen::set_offset(const std::string& n, float dx, float dy) { woff_[lower_ascii(n)] = {dx, dy}; }
void GuiScreen::get_offset(const std::string& n, float& dx, float& dy) const {
    const auto it = woff_.find(lower_ascii(n));
    dx = it == woff_.end() ? 0.f : it->second.first;
    dy = it == woff_.end() ? 0.f : it->second.second;
}
bool GuiScreen::is_disabled(const GuiWidget* w) const { return w && disabled_.count(lower_ascii(w->name)) > 0; }
bool GuiScreen::is_checked(const GuiWidget* w) const { return w && checked_.count(lower_ascii(w->name)) > 0; }
bool GuiScreen::is_hidden(const GuiWidget* w) const { return w && hidden_.count(lower_ascii(w->name)) > 0; }

ButtonState GuiScreen::state_of(const GuiWidget* w) const {
    if (is_disabled(w)) return ButtonState::Disabled;
    if (w && w == pressed_ && w == hover_) return ButtonState::Down;
    if (w && w == hover_) return ButtonState::Selected;
    return ButtonState::Up;
}

GuiWidget* GuiScreen::hit(float x, float y) const {
    GuiWidget* best = nullptr;
    std::function<void(GuiWidget*, float, float)> rec = [&](GuiWidget* w, float ox, float oy) {
        if (is_hidden(w)) return;
        float dx, dy;
        get_offset(w->name, dx, dy);
        ox += dx; oy += dy;
        const float x0 = w->rect.x0 + ox, y0 = w->rect.y0 + oy, x1 = w->rect.x1 + ox, y1 = w->rect.y1 + oy;
        if ((w->is(WidgetKind::Button) || w->is(WidgetKind::Edit)) && !is_disabled(w) && x >= x0 && x < x1 && y >= y0 && y < y1) best = w;
        for (const auto& c : w->children) rec(c.get(), ox, oy);
    };
    for (const auto& r : file_.roots) rec(r.get(), offset_x, offset_y);
    return best;
}

void GuiScreen::build(const GuiResources& res, std::vector<GuiDraw>& out) const {
    stats_ = Stats();
    for (const auto& r : file_.roots) build_widget(*r, res, out, false, false, offset_x, offset_y);
}

void GuiScreen::build_widget(const GuiWidget& w, const GuiResources& res, std::vector<GuiDraw>& out, bool parent_down, bool parent_checked, float ox, float oy) const {
    if (is_hidden(&w)) return;
    {
        float dx, dy;
        get_offset(w.name, dx, dy);
        ox += dx; oy += dy;
    }
    // filho-imagem de um botao: escolhe pelo nome qual aparece em cada estado
    if (w.is(WidgetKind::Picture) && w.parent && w.parent->is(WidgetKind::Button)) {
        const std::string n = lower_ascii(w.name);
        bool sib_checked = false;
        for (const auto& s : w.parent->children) if (contains(lower_ascii(s->name), "checked")) sib_checked = true;
        const bool is_checked_var = contains(n, "checked"), is_down = contains(n, "down"), is_up = !is_down && contains(n, "up");
        bool show = true;
        if (is_checked_var) show = parent_checked && (is_down ? parent_down : !parent_down);
        else if (parent_checked && sib_checked && (is_down || is_up)) show = false;
        else if (is_down) show = parent_down || (parent_checked && !sib_checked);
        else if (is_up) show = !parent_down && !(parent_checked && !sib_checked);
        if (!show) return;
    }
    const SkinFragment* st[9] = {nullptr};
    build_frags(w, res, out, nullptr, false, ox, oy);
    const auto img = image_.find(lower_ascii(w.name));
    if (img != image_.end() && !img->second.empty()) {                 // imagem trocada pelo jogo (digito, loading...)
        int tw = 0, th = 0;
        if (res.texture_size && res.texture_size(img->second, tw, th)) {
            GuiDraw d;
            d.texture = img->second;
            d.x0 = w.rect.x0 + ox; d.y0 = w.rect.y0 + oy; d.x1 = w.rect.x1 + ox; d.y1 = w.rect.y1 + oy;
            d.widget = &w;
            out.push_back(d);
            ++stats_.quads;
        } else { ++stats_.missing; stats_.missing_names.insert(img->second); }
    }
    bool down = parent_down, checked = parent_checked;
    if (w.is(WidgetKind::Button)) {
        const ButtonState s = state_of(&w);
        down = s == ButtonState::Down;
        checked = is_checked(&w);
        if (s != ButtonState::Up) {
            const Skin* sk = res.skin ? res.skin(w.skin) : nullptr;
            if (sk && sk->has_state("button", state_name(s))) {
                bool differs = false;
                for (int i = 0; i < 9; ++i) {
                    st[i] = sk->get("button", state_name(s), i);
                    if (st[i] && lower_ascii(st[i]->texture) != lower_ascii(w.frag[static_cast<std::size_t>(i)].texture)) differs = true;
                }
                if (differs) build_frags(w, res, out, st, true, ox, oy);
            }
        }
    }
    if (w.is(WidgetKind::Label) || w.is(WidgetKind::Edit)) build_text(w, res, out, ox, oy);
    if (w.is(WidgetKind::Button)) {
        // v20: num botao, as imagens de estado (BtnUp/BtnDown...) sao a "cara" do botao e vao por baixo; rotulos e o resto por cima.
        // Medido: no CharacterGeneration.int o lblName vem ANTES de BtnUp/BtnDown no arquivo, e no print do original "None" e
        // "AGE 2 MALE" aparecem sobre o botao (na ordem do arquivo o texto sumia).
        for (const auto& c : w.children) if (c->is(WidgetKind::Picture)) build_widget(*c, res, out, down, checked, ox, oy);
        for (const auto& c : w.children) if (!c->is(WidgetKind::Picture)) build_widget(*c, res, out, down, checked, ox, oy);
        return;
    }
    for (const auto& c : w.children) build_widget(*c, res, out, down, checked, ox, oy);
}

void GuiScreen::build_frags(const GuiWidget& w, const GuiResources& res, std::vector<GuiDraw>& out, const SkinFragment* const* skin_state, bool additive, float ox, float oy) const {
    const float X0 = w.rect.x0 + ox, Y0 = w.rect.y0 + oy, X1 = w.rect.x1 + ox, Y1 = w.rect.y1 + oy;
    const float W = X1 - X0, H = Y1 - Y0;
    if (W <= 0 || H <= 0) return;
    auto fw = [&](int i) -> float {
        const SkinFragment* s = skin_state ? skin_state[i] : nullptr;
        return static_cast<float>(s && s->width > 0 ? s->width : w.frag[static_cast<std::size_t>(i)].width);
    };
    auto fh = [&](int i) -> float {
        const SkinFragment* s = skin_state ? skin_state[i] : nullptr;
        return static_cast<float>(s && s->height > 0 ? s->height : w.frag[static_cast<std::size_t>(i)].height);
    };
    float lw = std::max({fw(0), fw(3), fw(6), 0.f}), rw = std::max({fw(2), fw(5), fw(8), 0.f});
    float th = std::max({fh(0), fh(1), fh(2), 0.f}), bh = std::max({fh(6), fh(7), fh(8), 0.f});
    if (lw + rw > W && lw + rw > 0) { const float k = W / (lw + rw); lw *= k; rw *= k; }
    if (th + bh > H && th + bh > 0) { const float k = H / (th + bh); th *= k; bh *= k; }
    // retangulos das 9 fatias (meio = objeto inteiro, como gravado no .int)
    const float R[9][4] = {
        {X0, Y0, X0 + lw, Y0 + th}, {X0 + lw, Y0, X1 - rw, Y0 + th}, {X1 - rw, Y0, X1, Y0 + th},
        {X0, Y0 + th, X0 + lw, Y1 - bh}, {X0, Y0, X1, Y1}, {X1 - rw, Y0 + th, X1, Y1 - bh},
        {X0, Y1 - bh, X0 + lw, Y1}, {X0 + lw, Y1 - bh, X1 - rw, Y1}, {X1 - rw, Y1 - bh, X1, Y1}};
    static const int order[9] = {4, 1, 3, 5, 7, 0, 2, 6, 8};
    for (int oi = 0; oi < 9; ++oi) {
        const int i = order[oi];
        // v20 (medido nos prints do original): objeto "imagem" desenha SO o miolo (a imagem); as bordas zzmainpicture-* nao aparecem
        // (ex.: as lampadas vermelhas f3_button_up/down de DONE/CANCEL ficariam cobertas). Rotulo nao tem moldura (so o texto).
        if (w.is(WidgetKind::Picture) && i != 4) continue;
        if (w.is(WidgetKind::Label)) continue;
        const GuiFragment& f = w.frag[static_cast<std::size_t>(i)];
        const SkinFragment* s = skin_state ? skin_state[i] : nullptr;
        if (skin_state && !s) continue;
        const std::string tex = lower_ascii(s ? s->texture : f.texture);
        const std::uint32_t flags = s ? s->flags : f.flags;
        std::uint8_t rgba[4];
        if (s) { rgba[0] = s->rgb[0]; rgba[1] = s->rgb[1]; rgba[2] = s->rgb[2]; rgba[3] = 255; }
        else { rgba[0] = x2(f.rgba[2]); rgba[1] = x2(f.rgba[1]); rgba[2] = x2(f.rgba[0]); rgba[3] = f.rgba[3]; }   // v20: D3DCOLOR (bytes B, G, R, A), RGB x2
        if (rgba[3] == 0 || tex.empty()) continue;
        const float* r = R[i];
        const float rw2 = r[2] - r[0], rh2 = r[3] - r[1];
        if (rw2 <= 0.5f || rh2 <= 0.5f) continue;
        int tw = 0, thh = 0;
        if (!res.texture_size || !res.texture_size(tex, tw, thh)) { ++stats_.missing; stats_.missing_names.insert(tex); continue; }
        GuiDraw d;
        d.kind = GuiDraw::Quad;
        d.texture = tex;
        d.x0 = r[0]; d.y0 = r[1]; d.x1 = r[2]; d.y1 = r[3];
        const float cw = fw(i) > 0 ? fw(i) : static_cast<float>(tw), ch = fh(i) > 0 ? fh(i) : static_cast<float>(thh);
        d.u1 = (flags & 1) && cw > 0 ? rw2 / cw : 1.f;
        d.v1 = (flags & 2) && ch > 0 ? rh2 / ch : 1.f;
        for (int k = 0; k < 4; ++k) d.rgba[k] = rgba[k];
        d.additive = additive;
        d.widget = &w;
        out.push_back(d);
        ++stats_.quads;
    }
}

std::vector<std::string> wrap_text(const std::string& text, float width, const std::function<float(const std::string&)>& measure) {
    std::vector<std::string> lines;
    std::string cur, word;
    auto flush_word = [&]() {
        if (word.empty()) return;
        const std::string cand = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && measure(cand) > width) { lines.push_back(cur); cur = word; }
        else cur = cand;
        word.clear();
    };
    for (char c : text) {
        if (c == '\n') { flush_word(); lines.push_back(cur); cur.clear(); }
        else if (c == ' ' || c == '\t') flush_word();
        else word.push_back(c);
    }
    flush_word();
    if (!cur.empty() || lines.empty()) lines.push_back(cur);
    return lines;
}

void GuiScreen::build_text(const GuiWidget& w, const GuiResources& res, std::vector<GuiDraw>& out, float ox, float oy) const {
    std::string txt;
    const auto it = text_.find(lower_ascii(w.name));
    if (it != text_.end()) txt = it->second;
    else if (w.text_id && res.text) txt = res.text(w.text_id);
    if (txt.empty() || w.text_rgba[3] == 0) return;
    const float px = font_px(w.font_size);
    const float X0 = w.rect.x0 + ox, Y0 = w.rect.y0 + oy, X1 = w.rect.x1 + ox, Y1 = w.rect.y1 + oy;
    const float lh = res.line_height ? res.line_height(w.font, px) : px * 1.2f;
    auto measure = [&](const std::string& s) { return res.text_width ? res.text_width(w.font, px, s) : static_cast<float>(s.size()) * px * 0.5f; };
    // v20: quebra de linha so quando cabem 2 linhas ou mais (caixas de descricao); rotulos de 1 linha sao CORTADOS no retangulo, como no
    // original ("DEFAUL", "CANCE" no print das opcoes de video)
    const bool multi = (Y1 - Y0) >= 2.f * lh - 1.f;
    const std::vector<std::string> lines = multi ? wrap_text(txt, std::max(4.f, X1 - X0 - 2.f), measure) : wrap_text(txt, 1e9f, measure);
    float top = Y0;
    const float total = lh * static_cast<float>(lines.size());
    if (total <= (Y1 - Y0) + 1.f) top = Y0 + std::floor(((Y1 - Y0) - total) * 0.5f);
    const std::size_t maxl = std::max<std::size_t>(1, static_cast<std::size_t>((Y1 - Y0) / std::max(1.f, lh)) + 1);
    for (std::size_t i = 0; i < lines.size() && i < maxl; ++i) {
        GuiDraw d;
        d.kind = GuiDraw::Text;
        d.font = w.font; d.text = lines[i]; d.size_px = px; d.align = w.halign();
        d.cx0 = X0; d.cy0 = Y0; d.cx1 = X1; d.cy1 = Y1 + 2.f;
        d.x0 = X0; d.x1 = X1; d.y0 = top + lh * static_cast<float>(i); d.y1 = d.y0 + lh;
        // v20: D3DCOLOR (bytes B, G, R, A) com RGB x2 (0x80 = neutro), alfa como esta. Medido no print das opcoes de video:
        // "Primary Monitor" 144e60b4 -> (192,156,40) a 70% sobre o fundo = (143,118,35); o print mede (150,123,39).
        d.rgba[0] = x2(w.text_rgba[2]); d.rgba[1] = x2(w.text_rgba[1]); d.rgba[2] = x2(w.text_rgba[0]); d.rgba[3] = w.text_rgba[3];
        d.widget = &w;
        out.push_back(d);
        ++stats_.texts;
    }
}

}  // namespace f3v
