// Cabeçalho de chunk: tag[4] + version(u32) + size(u32, inclui os 12 bytes).
// Little-endian lido byte a byte (independe da plataforma). Espelha tools/py/f3vb/chunk.py.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

namespace f3vb {

inline constexpr std::size_t kChunkHeaderSize = 12;

struct ChunkHeader {
    char tag[4];
    std::uint32_t version;
    std::uint32_t size;  // total, inclui o cabeçalho

    std::string_view tag_view() const { return {tag, 4}; }
    std::size_t payload_size() const { return size - kChunkHeaderSize; }
};

inline std::uint32_t read_le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// Lê e valida um cabeçalho. `avail` = bytes disponíveis a partir de `data`.
inline std::optional<ChunkHeader> read_chunk_header(const std::uint8_t* data, std::size_t avail) {
    if (avail < kChunkHeaderSize) return std::nullopt;
    ChunkHeader h{};
    std::memcpy(h.tag, data, 4);
    for (char c : h.tag) {
        if (c < 32 || c > 126) return std::nullopt;
    }
    h.version = read_le32(data + 4);
    h.size = read_le32(data + 8);
    if (h.size < kChunkHeaderSize || h.size > avail) return std::nullopt;
    return h;
}

}  // namespace f3vb
