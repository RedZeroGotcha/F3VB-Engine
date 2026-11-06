#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../src/formats/chunk.hpp"

static std::vector<std::uint8_t> mk(const char tag[4], std::uint32_t ver, std::size_t payload) {
    std::vector<std::uint8_t> v(12 + payload, 0);
    for (int i = 0; i < 4; ++i) v[i] = static_cast<std::uint8_t>(tag[i]);
    std::uint32_t size = static_cast<std::uint32_t>(12 + payload);
    for (int i = 0; i < 4; ++i) {
        v[4 + i] = (ver >> (8 * i)) & 0xff;
        v[8 + i] = (size >> (8 * i)) & 0xff;
    }
    return v;
}

int main() {
    auto good = mk("EMAP", 3, 4);
    auto h = f3vb::read_chunk_header(good.data(), good.size());
    assert(h && h->tag_view() == "EMAP" && h->version == 3 && h->payload_size() == 4);

    auto bad = mk("EMAP", 1, 0);
    bad[8] = 4;  // size < 12
    assert(!f3vb::read_chunk_header(bad.data(), bad.size()));

    auto big = mk("EMAP", 1, 0);
    big[8] = 200;  // size > avail
    assert(!f3vb::read_chunk_header(big.data(), big.size()));

    auto nonprint = mk("EMAP", 1, 0);
    nonprint[0] = 1;
    assert(!f3vb::read_chunk_header(nonprint.data(), nonprint.size()));

    assert(!f3vb::read_chunk_header(good.data(), 5));
    std::puts("C++ chunk tests OK");
    return 0;
}
