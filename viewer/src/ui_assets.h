// Recursos da interface (v19), parte PORTAVEL: acha e guarda em cache as imagens (TGA/BMP/.f3tex), skins (.ini), fontes TrueType (atlas por tamanho)
// e os textos (.stf). Monta o GuiResources que o GuiScreen usa. O backend (OpenGL ou CPU) so precisa transformar Image/FontAtlas em textura.
//
// Onde procura (todas as pastas sao opcionais; a primeira que tiver o arquivo vence; nomes sem diferenciar maiusculas):
//   imagens:  RETAIL\Interface\TGA_Image  (o original, decodificado aqui mesmo)  e  CONVERTED\ui (*.f3tex do ui-info)
//   skins:    RETAIL\Interface\INI_Config
//   fontes:   <demo>\Fonts\*.ttf  (casadas pelo nome interno da fonte, nao pelo nome do arquivo)
//   textos:   English.stf (CONVERTED\scripts, a pasta da demo ou F3VB_STF)
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "f3tex.h"
#include "gui.h"
#include "skin.h"
#include "stf.h"
#include "ttf.h"

namespace f3v {

class Vfs;

class UiAssets {
public:
    void set_vfs(const Vfs* v) { vfs_ = v; }               // v20: imagens (TGA_Image) e skins (INI_Config) direto da demo original
    void add_image_dir(const std::string& dir);          // indexa os arquivos da pasta (nao decodifica ainda)
    void add_skin_dir(const std::string& dir);
    std::size_t add_font_dir(const std::string& dir);    // carrega todos os .ttf; devolve quantos
    bool load_stf(const std::string& path, std::string& err);
    bool add_font_memory(const std::vector<std::uint8_t>& data, const std::string& label, std::string& err, bool system = false);
    // v20: fontes do SISTEMA (C:\Windows\Fonts): Arial, Arial Black, Times New Roman, Lucida Console... Com faithful = true (padrao do jogo),
    // as fontes que o .int pede e o Windows nao tem instaladas (Fallouty, Gothic 821, Hellbound) caem na Arial, como no jogo original
    // (prints: textos em Arial, cortados no retangulo). faithful = false: usa as fontes da pasta Fonts da demo.
    std::size_t add_system_fonts(const std::string& fonts_dir);
    void set_faithful_fonts(bool f) { faithful_fonts_ = f; atlases_.clear(); }
    bool faithful_fonts() const { return faithful_fonts_; }

    const Image* image(const std::string& name);         // nullptr = nao existe ou nao decodificou (fica registrado em problems())
    bool has_image(const std::string& name) const;
    const Skin* skin(const std::string& file);
    // Atlas da fonte (nome como no .int: "Fallouty", "Gothic 821 Condensed BT", "Arial"...) com o "em" em pixels (arredondado).
    const FontAtlas* atlas(const std::string& font, float em_px);
    std::string resolve_font(const std::string& font) const;   // nome da fonte TrueType realmente usada (para o log)
    const Stf& stf() const { return stf_; }

    GuiResources resources();
    std::size_t image_files() const { return files_.size(); }
    std::size_t font_count() const { return fonts_.size(); }
    std::vector<std::string> font_names() const;
    const std::vector<std::string>& problems() const { return problems_; }

private:
    struct FontEntry { std::string family, full, path; std::unique_ptr<TrueTypeFont> font; bool system = false; };
    const FontEntry* pick_font(const std::string& name) const;

    std::map<std::string, std::string> files_;           // nome minusculo -> caminho
    std::map<std::string, std::string> skin_files_;
    std::map<std::string, std::unique_ptr<Image>> images_;   // nullptr = falhou
    std::map<std::string, std::unique_ptr<Skin>> skins_;
    std::vector<FontEntry> fonts_;
    std::map<std::string, std::unique_ptr<FontAtlas>> atlases_;   // "fonte|px"
    Stf stf_;
    std::vector<std::string> problems_;
    const Vfs* vfs_ = nullptr;
    std::set<std::string> mismatched_;
    bool faithful_fonts_ = true;                    // v20: pedidos cujo formato nao bate com o conteudo (o original nao desenha)
public:
    std::size_t mismatched_count() const { return mismatched_.size(); }
};

// Lista os arquivos de uma pasta (so nomes; portavel: std::filesystem). Vazio se a pasta nao existe.
std::vector<std::string> list_dir(const std::string& dir);
std::string join_path(const std::string& a, const std::string& b);

}  // namespace f3v
