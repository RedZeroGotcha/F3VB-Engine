#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../src/formats/archive.hpp"

static void put32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 0xff);
}

int main() {
    // GRP: 2 lumps (4 e 3 bytes)
    std::vector<std::uint8_t> g;
    put32(g, 1); put32(g, 0); put32(g, 2);
    put32(g, 28); put32(g, 4);
    put32(g, 32); put32(g, 3);
    for (int i = 0; i < 7; ++i) g.push_back(0xAB);
    auto a = f3vb::parse_grp(g.data(), g.size());
    assert(a && a->n_files == 2 && a->lumps[1].offset == 32 && a->issues.empty());

    // lump fora do arquivo gera issue
    put32(g, 0);  // não altera o parse acima; novo buffer abaixo
    std::vector<std::uint8_t> bad;
    put32(bad, 1); put32(bad, 0); put32(bad, 1);
    put32(bad, 20); put32(bad, 9999);
    auto b = f3vb::parse_grp(bad.data(), bad.size());
    assert(b && b->issues.size() == 1);

    // tabela que não cabe
    std::vector<std::uint8_t> huge;
    put32(huge, 1); put32(huge, 0); put32(huge, 1000000);
    assert(!f3vb::parse_grp(huge.data(), huge.size()));

    // RHT: 1 entrada
    std::vector<std::uint8_t> r;
    put32(r, 1); put32(r, 0); put32(r, 1); put32(r, 40); put32(r, 40);
    put32(r, 1); put32(r, 77); put32(r, 700); put32(r, 3); put32(r, 92);
    auto f = f3vb::parse_rht(r.data(), r.size());
    assert(f && f->entries.size() == 1 && f->entries[0].type == 700 && f->entries[0].pack == 92);
    assert(!f3vb::parse_rht(r.data(), 10));

    std::puts("C++ archive tests OK");
    return 0;
}
