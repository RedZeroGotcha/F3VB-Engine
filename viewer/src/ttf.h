// Leitor e rasterizador de fontes TrueType PROPRIO (v19), sem bibliotecas externas. Portavel (so C++ padrao).
// Le: cabecalho de tabelas, head, hhea, maxp, hmtx, loca, glyf (glifos simples e compostos), cmap (formatos 4, 6 e 0), kern (formato 0) e name.
// Rasteriza: curvas quadraticas -> segmentos; regra de preenchimento "nao zero"; cobertura analitica na horizontal e 5 sub-linhas na vertical.
// Uso no jogo: as fontes da demo (Fonts/r_fallouty.ttf "Fallouty", Fonts/TT0807M_.TTF "Gothic 821 Condensed BT", Fonts/JH_FALLOUT.TTF "JH_Fallout").
// Hinting (instrucoes TrueType) e ignorado: em tamanhos de interface o resultado e o de um rasterizador sem hinting (levemente mais suave).
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace f3v {

class TrueTypeFont {
public:
    bool load(const std::uint8_t* data, std::size_t size, std::string& err);   // copia os bytes
    bool load_file(const std::string& path, std::string& err);
    bool ok() const { return ok_; }

    std::string family() const { return family_; }       // nome id 1 (ex.: "Fallouty")
    std::string full_name() const { return full_; }      // nome id 4 (ex.: "Gothic 821 Condensed BT")
    int units_per_em() const { return upem_; }
    int ascent() const { return ascent_; }                 // em unidades da fonte (hhea)
    int descent() const { return descent_; }               // negativo
    int line_gap() const { return line_gap_; }
    int glyph_count() const { return nglyphs_; }

    int glyph_index(std::uint32_t codepoint) const;        // 0 = glifo "faltando"
    int advance(int glyph) const;                          // em unidades da fonte
    int left_bearing(int glyph) const;
    int kerning(int left_glyph, int right_glyph) const;    // em unidades da fonte (0 se nao ha tabela kern)

    struct Point { float x, y; };
    // Contornos do glifo ja achatados em poligonos (unidades da fonte, y para CIMA). tolerance = erro maximo do achatamento, em unidades.
    bool outline(int glyph, std::vector<std::vector<Point>>& contours, float tolerance) const;

    struct Bitmap { int w = 0, h = 0, x0 = 0, y0 = 0; std::vector<std::uint8_t> alpha; };   // x0/y0: canto superior esquerdo em relacao a origem (y para BAIXO)
    // Rasteriza com escala = pixels por unidade da fonte. Glifos vazios (espaco) devolvem w = h = 0.
    bool rasterize(int glyph, float scale, Bitmap& out) const;
    float scale_for_em(float em_pixels) const { return upem_ > 0 ? em_pixels / static_cast<float>(upem_) : 0.f; }

private:
    bool parse_glyph(int glyph, std::vector<std::vector<Point>>& out, float tol, int depth, const float m[6]) const;
    bool glyph_range(int glyph, std::size_t& off, std::size_t& len) const;
    std::uint16_t u16(std::size_t o) const;
    std::int16_t s16(std::size_t o) const { return static_cast<std::int16_t>(u16(o)); }
    std::uint32_t u32(std::size_t o) const;

    std::vector<std::uint8_t> d_;
    bool ok_ = false;
    std::size_t glyf_ = 0, glyf_len_ = 0, loca_ = 0, hmtx_ = 0, cmap_sub_ = 0, kern_ = 0, kern_len_ = 0;
    int cmap_fmt_ = -1, loca_long_ = 0, nhmetrics_ = 0, nglyphs_ = 0, upem_ = 0, ascent_ = 0, descent_ = 0, line_gap_ = 0;
    bool symbol_ = false;
    std::string family_, full_;
};

// Atlas de glifos (cp1252: 32..255) de uma fonte num tamanho em pixels, pronto para subir como textura de alfa.
struct GlyphInfo { int x = 0, y = 0, w = 0, h = 0, ox = 0, oy = 0; float advance = 0; bool present = false; };
struct FontAtlas {
    int w = 0, h = 0;                         // tamanho da textura (potencia de 2)
    std::vector<std::uint8_t> alpha;          // w*h, linha de cima primeiro
    float em = 0, ascent = 0, descent = 0, line = 0;   // em pixels
    GlyphInfo glyph[256];                     // indice = byte cp1252
    std::map<std::uint32_t, float> kern;      // (a << 8 | b) -> ajuste em pixels
    float measure(const std::string& cp1252) const;   // largura em pixels
};
bool bake_atlas(const TrueTypeFont& font, float em_pixels, FontAtlas& out);
std::uint32_t cp1252_to_unicode(std::uint8_t c);

}  // namespace f3v
