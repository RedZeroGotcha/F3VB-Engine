// Executa um script AMX real com a camada de natives e imprime: o log de cada public, a arvore de dialogo
// (nos e respostas) e as natives que ainda faltam implementar.
//   dialog_dump ARQ.amx [--seed N] [--quiet]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "game_host.hpp"

using namespace f3vb;

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: dialog_dump ARQ.amx [--seed N] [--quiet]"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::uint32_t seed = 1;
    bool quiet = false;
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        if (!std::strcmp(argv[i], "--quiet")) quiet = true;
    }
    std::string err;
    auto prog = amx::Program::load(bytes, &err);
    if (!prog) { std::printf("ERRO ao carregar: %s\n", err.c_str()); return 1; }
    amx::Machine m(*prog);
    host::GameHost h(m, seed);
    h.install();
    std::printf("script: %zu publics, %zu natives\n\n", prog->publics().size(), prog->natives().size());
    int failures = 0;
    for (std::size_t i = 0; i < prog->publics().size(); ++i) {
        h.clear_events();
        std::int32_t r = 0;
        const amx::Err e = m.call(static_cast<int>(i), {}, &r);
        if (e != amx::Err::None) ++failures;
        if (!quiet) {
            std::printf("%-22s -> %s (ret %d)\n", prog->publics()[i].name.c_str(), amx::err_name(e), r);
            for (const auto& ev : h.events()) {
                std::printf("    %s%s(", ev.implemented ? "" : "[so-log] ", ev.native.c_str());
                for (std::size_t k = 0; k < ev.args.size(); ++k) {
                    std::printf("%s%d", k ? ", " : "", ev.args[k]);
                    if (k < ev.strings.size() && !ev.strings[k].empty()) std::printf(" \"%s\"", ev.strings[k].c_str());
                }
                std::printf(")\n");
            }
        }
    }
    std::puts("\n=== Arvore de dialogo ===");
    for (const auto& n : h.nodes()) {
        std::printf("no \"%s\"  (funcao de texto: \"%s\")\n", n.name.c_str(), n.text_fn.c_str());
        for (const auto& r : n.replies)
            std::printf("   resposta #%d  texto(id)=%d  condicao=\"%s\"  resultado=\"%s\"\n", r.id, r.text_id,
                        r.action_fn.c_str(), r.link_fn.c_str());
    }
    std::printf("\nfalhas de execucao: %d\n", failures);
    return failures ? 1 : 0;
}
