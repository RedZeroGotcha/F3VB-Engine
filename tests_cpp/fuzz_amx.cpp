// Fuzz simples da VM: muta bytes de um .amx real e confere que carregar/executar nunca derruba o programa
// (rode com -fsanitize=address,undefined). Uso: fuzz_amx ARQ.amx [iteracoes]
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include "../src/amx/amx_vm.hpp"

using namespace f3vb::amx;

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::ifstream f(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> orig((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const int iters = argc > 2 ? std::atoi(argv[2]) : 20000;
    std::mt19937 rng(12345);
    int loaded = 0, ran = 0, errors = 0;
    for (int it = 0; it < iters; ++it) {
        auto b = orig;
        const int muts = 1 + static_cast<int>(rng() % 6);
        for (int k = 0; k < muts; ++k) b[rng() % b.size()] = static_cast<std::uint8_t>(rng());
        if (rng() % 8 == 0) b.resize(rng() % b.size());
        auto prog = Program::load(b);
        if (!prog) continue;
        ++loaded;
        Machine m(*prog);
        m.set_instruction_limit(20000);
        for (const auto& n : prog->natives()) m.register_native(n.name, [](Machine&, const std::int32_t* p) { return p[0]; });
        for (std::size_t i = 0; i < prog->publics().size(); ++i) {
            std::int32_t r = 0;
            if (m.call(static_cast<int>(i), {}, &r) != Err::None) ++errors;
            ++ran;
        }
    }
    std::printf("fuzz: %d iteracoes, %d carregaram, %d execucoes (%d terminaram com erro controlado)\n", iters, loaded, ran, errors);
    return 0;
}
