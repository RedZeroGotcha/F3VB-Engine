// Leitores de cabeçalho/tabela de .grp e resource.rht (layouts em docs/specs, a confirmar
// em arquivos reais). Somente validação e acesso; sem alocar os dados dos lumps.
// Espelha tools/py/f3vb/grp.py e rht.py.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "chunk.hpp"  // read_le32

namespace f3vb {

struct GrpLump {
    std::uint32_t offset;
    std::uint32_t length;
    std::uint64_t end() const { return std::uint64_t{offset} + length; }
};

struct GrpArchive {
    std::uint32_t v_major = 0, v_minor = 0, n_files = 0;
    std::size_t file_size = 0;
    std::vector<GrpLump> lumps;
    std::vector<std::string> issues;
};

// Retorna nullopt se o buffer não puder ser um .grp (cabeçalho ou tabela não cabem).
inline std::optional<GrpArchive> parse_grp(const std::uint8_t* data, std::size_t size) {
    constexpr std::size_t kHeader = 12, kLump = 8;
    if (size < kHeader) return std::nullopt;
    GrpArchive a;
    a.v_major = read_le32(data);
    a.v_minor = read_le32(data + 4);
    a.n_files = read_le32(data + 8);
    a.file_size = size;
    const std::uint64_t table_end = kHeader + std::uint64_t{kLump} * a.n_files;
    if (table_end > size) return std::nullopt;
    a.lumps.reserve(a.n_files);
    for (std::uint32_t i = 0; i < a.n_files; ++i) {
        const std::uint8_t* p = data + kHeader + std::size_t{kLump} * i;
        GrpLump l{read_le32(p), read_le32(p + 4)};
        if (l.end() > size) {
            a.issues.push_back("lump " + std::to_string(i) + ": fora do arquivo");
        } else if (l.length != 0 && l.offset < table_end) {
            a.issues.push_back("lump " + std::to_string(i) + ": começa dentro da tabela");
        }
        a.lumps.push_back(l);
    }
    return a;
}

struct RhtEntry {
    std::uint32_t one, number, type, offset, pack;
};

struct RhtFile {
    std::uint32_t v_major = 0, v_minor = 0, n_entries = 0, offset_packs = 0, offset_resources = 0;
    std::vector<RhtEntry> entries;
};

inline std::optional<RhtFile> parse_rht(const std::uint8_t* data, std::size_t size) {
    constexpr std::size_t kHeader = 20, kEntry = 20;
    if (size < kHeader) return std::nullopt;
    RhtFile f;
    f.v_major = read_le32(data);
    f.v_minor = read_le32(data + 4);
    f.n_entries = read_le32(data + 8);
    f.offset_packs = read_le32(data + 12);
    f.offset_resources = read_le32(data + 16);
    if (kHeader + std::uint64_t{kEntry} * f.n_entries > size) return std::nullopt;
    f.entries.reserve(f.n_entries);
    for (std::uint32_t i = 0; i < f.n_entries; ++i) {
        const std::uint8_t* p = data + kHeader + std::size_t{kEntry} * i;
        f.entries.push_back({read_le32(p), read_le32(p + 4), read_le32(p + 8), read_le32(p + 12),
                             read_le32(p + 16)});
    }
    return f;
}

}  // namespace f3vb
