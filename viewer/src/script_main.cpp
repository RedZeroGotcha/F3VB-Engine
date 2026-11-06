// F3VBScript: roda um script .amx da demo SEM janela e imprime o dialogo (v17). Ferramenta de depuracao: mande a saida (ou o --log) para analise.
//   F3VBScript ARQ.amx [--stf TABELA.stf] [--choices 1,2,1] [--seed N] [--survey] [--log SAIDA.txt]
//     sem opcoes: inicia o dialogo (OnStartDialogue) e mostra falas e respostas; --choices responde na ordem (numeros das respostas visiveis);
//     --survey: roda TODAS as publics (como o dialog_dump) e lista erros e natives que ainda faltam implementar.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "script_world.h"
#include "stf.h"

using namespace f3v;

static FILE* g_log = nullptr;
static void out(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    std::vprintf(fmt, ap);
    if (g_log) std::vfprintf(g_log, fmt, ap2);
    va_end(ap2);
    va_end(ap);
}

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: F3VBScript ARQ.amx [--stf TABELA.stf] [--choices 1,2,1] [--seed N] [--survey] [--log SAIDA.txt]"); return 2; }
    std::string stf_path, log_path;
    std::vector<int> choices;
    std::uint32_t seed = 1;
    bool survey = false;
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--stf") && i + 1 < argc) stf_path = argv[++i];
        else if (!std::strcmp(argv[i], "--log") && i + 1 < argc) log_path = argv[++i];
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--survey")) survey = true;
        else if (!std::strcmp(argv[i], "--choices") && i + 1 < argc) {
            for (char* tok = std::strtok(argv[++i], ","); tok; tok = std::strtok(nullptr, ",")) choices.push_back(std::atoi(tok));
        }
    }
    if (!log_path.empty()) g_log = std::fopen(log_path.c_str(), "w");
    Stf stf;
    std::string err;
    if (!stf_path.empty()) {
        if (stf.load_file(stf_path, err)) out("tabela de strings: %s (%zu textos)\n", stf_path.c_str(), stf.count());
        else out("AVISO: .stf nao carregou: %s\n", err.c_str());
    }
    auto si = ScriptInstance::from_file(argv[1], argv[1], 1000, seed, err);
    if (!si) { out("ERRO ao carregar %s: %s\n", argv[1], err.c_str()); return 1; }
    out("script %s: %zu publics, %zu natives\n", argv[1], si->prog->publics().size(), si->prog->natives().size());

    int rc = 0;
    if (survey) {
        const ScriptSurvey sv = survey_script(*si);
        out("publics: %zu ok, %zu com erro | dialogo (OnStartDialogue): %s\n", sv.ran_ok, sv.ran_fail, sv.has_dialogue ? "sim" : "nao");
        for (const std::string& f : sv.failures) out("  erro: %s\n", f.c_str());
        for (const std::string& f : sv.unresolved) out("  native importada e nao registrada: %s\n", f.c_str());
        for (const auto& kv : sv.unimplemented) {
            out("  native sem implementar: %s x%d", kv.first.c_str(), kv.second);
            const auto it = sv.samples.find(kv.first);
            if (it != sv.samples.end()) for (const std::string& smp : it->second) out("  ex.: %s", smp.c_str());
            out("\n");
        }
        std::string all;
        for (const auto& kv : sv.called) all += (all.empty() ? "" : ", ") + kv.first + "x" + std::to_string(kv.second);
        out("natives chamadas (todas): %s\n", all.empty() ? "(nenhuma)" : all.c_str());
        rc = sv.ran_fail ? 1 : 0;
    } else {
        DialogSession d(*si, stf.loaded() ? &stf : nullptr);
        auto show = [&](std::size_t& shown) {
            for (; shown < d.lines().size(); ++shown) {
                const DialogLine& l = d.lines()[shown];
                out("  %s: %s\n", l.who.c_str(), l.text.empty() ? ("[texto #" + std::to_string(l.text_id) + "]").c_str() : l.text.c_str());
            }
            if (!d.error().empty()) out("  ERRO da VM: %s\n", d.error().c_str());
            if (d.finished()) out("  (fim do dialogo)\n");
            else {
                for (std::size_t i = 0; i < d.choices().size(); ++i) {
                    const DialogChoice& c = d.choices()[i];
                    out("  %zu. %s   (resposta #%d, texto %d, acao '%s', destino '%s')\n", i + 1, c.text.empty() ? "[sem texto]" : c.text.c_str(), c.reply_id, c.text_id, c.action_fn.c_str(), c.link_fn.c_str());
                }
                if (d.hidden_choices()) out("  (%zu resposta(s) escondida(s) pela condicao)\n", d.hidden_choices());
            }
        };
        std::size_t shown = 0;
        out("\n== iniciar dialogo ==\n");
        if (!d.start()) { out("nao deu para iniciar: %s\n", d.error().c_str()); rc = 1; }
        show(shown);
        for (int c : choices) {
            if (d.finished()) break;
            out("\n== escolhendo %d ==\n", c);
            if (c < 1 || !d.choose(static_cast<std::size_t>(c - 1))) { out("  resposta %d nao existe\n", c); rc = 1; break; }
            show(shown);
        }
        out("\n== passos da VM ==\n");
        for (const std::string& t : d.trace) out("  %s\n", t.c_str());
        out("\n== chamadas de natives ==\n");
        for (const auto& e : si->host->events()) {
            out("  %s%s(", e.implemented ? "" : "[so-log] ", e.native.c_str());
            for (std::size_t k = 0; k < e.args.size(); ++k) out("%s%d", k ? ", " : "", e.args[k]);
            out(")\n");
        }
        for (const auto& kv : si->host->unimplemented_calls()) out("native sem implementar: %s x%d\n", kv.first.c_str(), kv.second);
    }
    if (g_log) std::fclose(g_log);
    return rc;
}
