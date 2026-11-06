// Tabela de strings (.stf) para os dialogos (v17). Portavel: sem Windows nem OpenGL.
//
// Layout (derivado do stf.pat do OpenJE/patterns; VERIFICADO em 2026-10-28 com o English.stf real: 3281 entradas, 254.846 bytes, texto 'Single Player' na entrada 0):
//   u8 magic[8], u32 count, count x { u32 offset, u32 length, u8 constante[8] }
// Hipoteses: id do texto = indice da entrada; o texto esta em `offset` (absoluto) com `length` bytes (ou unidades de 2 bytes se nao couber);
// codificacao UTF-16LE se os bytes alternam com zeros, senao cp1252 (convertida para ASCII simples: o HUD so desenha ASCII).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace f3v {

class Stf {
public:
    bool load(const std::uint8_t* data, std::size_t size, std::string& err);
    bool load_file(const std::string& path, std::string& err);
    bool loaded() const { return loaded_; }
    std::size_t count() const { return entries_.size(); }
    // "" se o id nao existe (ou o arquivo nao foi carregado). ATENCAO: `id` aqui e o INDICE da entrada (0-based).
    std::string text(std::uint32_t id) const;
    // v19: ids 1-based, como os dos .int (rotulos): id N = entrada N-1; 0 = sem texto. Medido: rotulo de QuitBtn = 16 -> entrada 15 'Quit'.
    // Devolve o texto em cp1252 (acentos preservados para a fonte TrueType), sem trocar por '?'.
    std::string text_cp1252(std::uint32_t id_1based) const;
    bool has(std::uint32_t id) const { return id < entries_.size(); }

private:
    std::string raw_entry(std::uint32_t index) const;     // texto cru (cp1252 / latin-1) da entrada
    struct Entry { std::uint32_t off, len; };
    std::vector<std::uint8_t> data_;
    std::vector<Entry> entries_;
    bool loaded_ = false;
};

// Troca tudo que nao e ASCII imprimivel (acentos etc.) por equivalentes simples ou '?', para o HUD de bitmap.
std::string ascii_only(const std::string& utf8_or_cp1252);

}  // namespace f3v
