// Ferramenta de linha de comando: carrega um .amx da demo, lista publics/natives e executa publics
// com nativas "de log" (registram nome e argumentos e devolvem 0). Serve para validar a VM com scripts reais.
//   amx_run ARQ.amx               -> resumo + executa todas as publics sem argumentos
//   amx_run ARQ.amx --call NOME [arg ...]
//   amx_run ARQ.amx --list
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "amx_vm.hpp"

using namespace f3vb::amx;

static std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: amx_run ARQ.amx [--list | --call NOME [args...]]"); return 2; }
    auto bytes = read_file(argv[1]);
    std::string error;
    auto prog = Program::load(bytes, &error);
    if (!prog) { std::printf("ERRO ao carregar: %s\n", error.c_str()); return 1; }
    const auto& h = prog->header();
    std::printf("%s: file_v=%d amx_v=%d magic=%#x flags=%#x defsize=%d code=%zu cel, data=%zu B, mem=%d B\n", argv[1],
                h.file_version, h.amx_version, h.magic, h.flags, h.defsize, prog->code().size(),
                prog->initial_data().size(), prog->memory_size());
    std::printf("publics=%zu natives=%zu\n", prog->publics().size(), prog->natives().size());
    if (argc > 2 && !std::strcmp(argv[2], "--list")) {
        for (auto& f : prog->publics()) std::printf("  public  %-28s @%u\n", f.name.c_str(), f.address);
        for (auto& f : prog->natives()) std::printf("  native  %s\n", f.name.c_str());
        return 0;
    }

    Machine m(*prog);
    bool verbose = true;
    for (const auto& n : prog->natives()) {
        const std::string name = n.name;
        m.register_native(name, [name, &verbose](Machine& vm, const std::int32_t* p) -> std::int32_t {
            if (verbose) {
                std::printf("      > %s(", name.c_str());
                const int n = p[0] / 4;
                for (int i = 1; i <= n; ++i) {
                    // tenta mostrar strings quando o argumento parece um endereco de texto
                    std::printf("%s%d", i > 1 ? ", " : "", p[i]);
                }
                std::printf(")\n");
            }
            (void)vm;
            return 0;
        });
    }

    auto run_one = [&](int idx, std::vector<std::int32_t> args) {
        std::int32_t result = 0;
        const auto before = m.instructions_executed();
        const Err e = m.call(idx, args, &result);
        std::printf("  %-26s -> %s (retorno %d, %llu instrucoes)\n", prog->publics()[static_cast<std::size_t>(idx)].name.c_str(),
                    err_name(e), result, static_cast<unsigned long long>(m.instructions_executed() - before));
        return e;
    };

    int failures = 0;
    if (argc > 3 && !std::strcmp(argv[2], "--call")) {
        std::vector<std::int32_t> args;
        for (int i = 4; i < argc; ++i) args.push_back(std::atoi(argv[i]));
        const int idx = prog->find_public(argv[3]);
        if (idx < 0) { std::puts("public nao encontrada"); return 1; }
        failures += run_one(idx, args) != Err::None;
    } else {
        for (std::size_t i = 0; i < prog->publics().size(); ++i)
            failures += run_one(static_cast<int>(i), {}) != Err::None;
    }
    std::printf("falhas: %d\n", failures);
    return failures ? 1 : 0;
}
