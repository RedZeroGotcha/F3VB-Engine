// Skins da interface (*.ini do pack Interface: zzmainmenu.ini, quickslot.ini, f3_green.ini, areamap.ini...) - v19.
// Formato documentado no proprio AreaMap.ini da demo: secoes [objeto.estado.fragmento] (ou [objeto.fragmento] sem estado) com os campos
//   flags = 0 esticar | 1 repetir em x | 2 repetir em y | 3 repetir nos dois;  texture = arquivo;  color = r, g, b (0-255, padrao branco);
//   width/height = tamanho do recorte (padrao: o da textura; 8 sem textura);  rate = quadros por segundo (0 = parado).
// Os botoes tem 4 estados: up, selected (mouse em cima), down (apertado), disabled. O .int guarda so o estado de repouso; os outros vem daqui.
// Portavel (so C++ padrao).
#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace f3v {

struct SkinFragment {
    std::uint32_t flags = 0;
    std::string texture;
    std::uint8_t rgb[3] = {255, 255, 255};
    bool has_color = false;
    int width = -1, height = -1;     // -1 = padrao
    float rate = 0.f;
};

class Skin {
public:
    bool parse(const std::string& text);                 // false se nenhuma secao valida
    bool load_file(const std::string& path, std::string& err);
    // objeto: "window", "button"...; estado: "" (sem estado), "up", "selected", "down", "disabled"; fragmento: 0..8 (ordem do .int).
    const SkinFragment* get(const std::string& object, const std::string& state, int fragment) const;
    bool has_state(const std::string& object, const std::string& state) const;
    std::size_t size() const { return sections_.size(); }

    static const char* fragment_name(int i);            // "upperleft" ... "lowerright"

private:
    std::map<std::string, SkinFragment> sections_;       // chave minuscula "button.up.middle"
};

}  // namespace f3v
