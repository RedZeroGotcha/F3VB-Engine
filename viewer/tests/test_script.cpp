// v17: biblioteca de scripts, STF, sessao de dialogo e levantamento, com scripts SINTETICOS (nenhum dado do jogo).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <random>
#include <string>
#include <vector>

#include "script_world.h"
#include "stf.h"

using namespace f3v;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

enum : std::int32_t { LOAD_S_PRI = 3, CONST_PRI = 11, STOR_PRI = 15, PUSH_C = 39, STACK = 44, PROC = 46, RETN = 48, ZERO_PRI = 89, HALT = 120, SYSREQ_C = 123, FILE_OP = 124, LINE_OP = 125, SYMBOL_OP = 126 };

// ---- montador minimo de AMX classico (file_version 6), igual ao de tests_cpp/test_host.cpp
static std::vector<std::uint8_t> build(const std::vector<std::int32_t>& code, const std::vector<std::pair<std::string, std::int32_t>>& pubs,
                                      const std::vector<std::string>& natives, const std::vector<std::int32_t>& data_cells) {
    const int defsize = 40;
    std::vector<std::uint8_t> out;
    auto put = [&](std::uint32_t v, int n) { for (int i = 0; i < n; ++i) out.push_back((v >> (8 * i)) & 0xff); };
    const std::int32_t pub_off = 52, nat_off = pub_off + defsize * static_cast<int>(pubs.size());
    const std::int32_t lib_off = nat_off + defsize * static_cast<int>(natives.size());
    const std::int32_t cod = lib_off, dat = cod + 4 * static_cast<int>(code.size());
    const std::int32_t hea = dat + 4 * static_cast<int>(data_cells.size()), stp = hea + 4096;
    put(static_cast<std::uint32_t>(hea), 4); put(0xF1E0, 2); put(6, 1); put(6, 1); put(0, 2); put(defsize, 2);
    for (std::int32_t v : {cod, dat, hea, stp, -1, pub_off, nat_off, lib_off, lib_off, lib_off}) put(static_cast<std::uint32_t>(v), 4);
    auto stub = [&](std::uint32_t addr, const std::string& n) {
        put(addr, 4);
        for (int i = 0; i < defsize - 4; ++i) out.push_back(i < static_cast<int>(n.size()) ? static_cast<std::uint8_t>(n[static_cast<std::size_t>(i)]) : 0);
    };
    for (auto& p : pubs) stub(static_cast<std::uint32_t>(p.second) * 4, p.first);
    for (auto& n : natives) stub(0, n);
    for (std::int32_t v : code) put(static_cast<std::uint32_t>(v), 4);
    for (std::int32_t v : data_cells) put(static_cast<std::uint32_t>(v), 4);
    return out;
}

// Script de dialogo sintetico (ids de texto pequenos: 10, 20.. para a tabela de teste ser pequena).
//   OnStartDialogue: no "Node 1", funcao de texto NU1_N1, NPC fala o texto 10
//   NU1_N1: respostas 1, 2 (acao RC2_N1) e 3 (acao RC3_N1)  [v19: 3o argumento = acao, 4o = destino; todas valem]
//   RL1_N1: NPC fala 30 e troca a funcao de texto para NU2_N1;  NU2_N1: resposta 1 (texto 40);  RL4_N1: PCSpeaker(41) + EndDialogue
//   RL3_N1: NPC fala 31 + EndDialogue;  RL2_N1: nada
static std::vector<std::uint8_t> make_dialog_script(bool with_start = true) {
    std::vector<std::int32_t> data(1, 0);
    auto str = [&](const std::string& s) {
        const std::int32_t addr = static_cast<std::int32_t>(data.size()) * 4;
        for (char c : s) data.push_back(static_cast<unsigned char>(c));
        data.push_back(0);
        return addr;
    };
    const std::int32_t a_node = str("Node 1"), a_nu1 = str("NU1_N1"), a_nu2 = str("NU2_N1"), a_rc2 = str("RC2_N1"), a_rc3 = str("RC3_N1");
    const std::int32_t a_rl1 = str("RL1_N1"), a_rl2 = str("RL2_N1"), a_rl3 = str("RL3_N1"), a_rl4 = str("RL4_N1"), a_end = str("EndDialogue"), a_empty = str("");
    enum { N_NODE = 0, N_TEXTFN = 1, N_REPLY = 2, N_SPEECH = 3, N_PC = 4 };
    std::vector<std::int32_t> code = {HALT, 0};
    auto emit = [&code](std::initializer_list<std::int32_t> l) { for (std::int32_t v : l) code.push_back(v); };
    auto pos = [&code]() { return static_cast<std::int32_t>(code.size()); };
    auto node = [&](std::int32_t a) { emit({PUSH_C, a, PUSH_C, 4, SYSREQ_C, N_NODE, STACK, 8}); };
    auto textfn = [&](std::int32_t a) { emit({PUSH_C, a, PUSH_C, 4, SYSREQ_C, N_TEXTFN, STACK, 8}); };
    auto reply = [&](std::int32_t id, std::int32_t text, std::int32_t cond, std::int32_t res) { emit({PUSH_C, res, PUSH_C, cond, PUSH_C, text, PUSH_C, id, PUSH_C, 16, SYSREQ_C, N_REPLY, STACK, 20}); };
    auto speech = [&](std::int32_t text) { emit({PUSH_C, 128, PUSH_C, 128, PUSH_C, 128, PUSH_C, 0, PUSH_C, text, PUSH_C, 1000, PUSH_C, 24, SYSREQ_C, N_SPEECH, STACK, 28}); };
    auto pc = [&](std::int32_t text) { emit({PUSH_C, text, PUSH_C, 4, SYSREQ_C, N_PC, STACK, 8}); };

    std::vector<std::pair<std::string, std::int32_t>> pubs;
    pubs.push_back({"OnStartDialogue", pos()});
    if (with_start) { emit({PROC}); node(a_node); textfn(a_nu1); speech(10); emit({CONST_PRI, 0, RETN}); }
    else emit({PROC, RETN});
    pubs.push_back({"NU1_N1", pos()});
    emit({PROC}); reply(1, 20, a_empty, a_rl1); reply(2, 21, a_rc2, a_rl2); reply(3, 22, a_rc3, a_rl3); emit({CONST_PRI, 0, RETN});
    pubs.push_back({"RC2_N1", pos()}); emit({PROC, CONST_PRI, 0, RETN});
    pubs.push_back({"RC3_N1", pos()}); emit({PROC, CONST_PRI, 1, RETN});
    pubs.push_back({"RL1_N1", pos()}); emit({PROC}); speech(30); textfn(a_nu2); emit({CONST_PRI, 0, RETN});
    pubs.push_back({"NU2_N1", pos()}); emit({PROC}); reply(1, 40, a_empty, a_rl4); emit({CONST_PRI, 0, RETN});
    pubs.push_back({"RL4_N1", pos()}); emit({PROC}); pc(41); textfn(a_end); emit({CONST_PRI, 0, RETN});
    pubs.push_back({"RL3_N1", pos()}); emit({PROC}); speech(31); textfn(a_end); emit({CONST_PRI, 0, RETN});
    pubs.push_back({"RL2_N1", pos()}); emit({PROC, CONST_PRI, 0, RETN});
    return build(code, pubs, {"SetCurrentNodeName", "SetNodeTextFunction", "RegisterReply", "FloatingSpeechText", "PCSpeaker"}, data);
}

// STF sintetico no layout hipotetico: 8 bytes de magic + u32 count + count x {off, len, 8 bytes} + textos
static std::vector<std::uint8_t> make_stf(std::uint32_t count) {
    std::vector<std::uint8_t> b;
    auto put32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xff); };
    for (char c : std::string("STFMAGIC")) b.push_back(static_cast<std::uint8_t>(c));
    put32(count);
    std::vector<std::string> texts;
    for (std::uint32_t i = 0; i < count; ++i) texts.push_back(i == 7 ? std::string("\xC1gua") : "texto " + std::to_string(i));   // 7 = cp1252 "Agua" com acento
    std::uint32_t off = 12 + 16 * count;
    for (const auto& t : texts) { put32(off); put32(static_cast<std::uint32_t>(t.size())); for (int i = 0; i < 8; ++i) b.push_back(0); off += static_cast<std::uint32_t>(t.size()); }
    for (const auto& t : texts) for (char c : t) b.push_back(static_cast<std::uint8_t>(c));
    return b;
}


// Script compilado COM simbolos de depuracao (flags=DEBUG): FILE / LINE / SYMBOL no meio do codigo, publics com parametros e a mesma native importada 2 vezes.
// Regressao do v17: a VM executava o payload de FILE/SYMBOL como se fosse codigo e TODA public falhava com "instrucao invalida".
static std::vector<std::uint8_t> make_debug_script() {
    std::vector<std::int32_t> data = {0, 0, 0, 0};                 // cel 1 (end. 4) recebe o parametro; end. 16: "EndDialogue"
    const std::string end_dialogue = "EndDialogue";
    for (char c : end_dialogue) data.push_back(static_cast<unsigned char>(c));
    data.push_back(0);
    std::vector<std::int32_t> code = {HALT, 0};
    auto emit = [&code](std::initializer_list<std::int32_t> l) { for (std::int32_t v : l) code.push_back(v); };
    auto pos = [&code]() { return static_cast<std::int32_t>(code.size()); };
    auto file = [&]() { emit({FILE_OP, 16, 0, 0x2E, 0x2E, 0x2E}); };                           // size 16: 4 cels de payload
    auto line = [&](std::int32_t a, std::int32_t b) { emit({LINE_OP, a, b}); };
    auto symbol = [&]() { emit({SYMBOL_OP, 24, 12, 0x0101, 0x72727543, 0x47746e65, 0x54656d61, 0x00656d69}); };   // size 24: offset, ident, nome
    std::vector<std::pair<std::string, std::int32_t>> pubs;
    pubs.push_back({"OnCreate", pos()});
    emit({PROC}); line(74, 26); symbol(); line(76, 26); emit({LOAD_S_PRI, 12, STOR_PRI, 4, ZERO_PRI, RETN});
    pubs.push_back({"Call2", pos()});
    emit({PROC}); file(); symbol(); emit({PUSH_C, 16, PUSH_C, 4, SYSREQ_C, 1, STACK, 8, ZERO_PRI, RETN});
    // dados: o nome "EndDialogue" comeca na cel 4 (end. 16) com 1 caractere por celula
    return build(code, pubs, {"SetNodeTextFunction", "SetNodeTextFunction"}, data);
}

static std::vector<std::string> line_texts(const DialogSession& d) { std::vector<std::string> v; for (const auto& l : d.lines()) v.push_back(l.who + ": " + l.text); return v; }

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc >= 3 && !std::strcmp(argv[1], "--write-sample")) {            // grava o script/STF sinteticos para experimentar com o F3VBScript
        const auto b = make_dialog_script();
        std::ofstream(argv[2], std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
        if (argc >= 4) { const auto t = make_stf(50); std::ofstream(argv[3], std::ios::binary).write(reinterpret_cast<const char*>(t.data()), static_cast<std::streamsize>(t.size())); }
        return 0;
    }
    std::string err;

    // ---------------------------------------------------------------- STF
    {
        const auto bytes = make_stf(50);
        Stf stf;
        CHECK(stf.load(bytes.data(), bytes.size(), err));
        CHECK(stf.count() == 50 && stf.has(49) && !stf.has(50));
        CHECK(stf.text(10) == "texto 10" && stf.text(0) == "texto 0");
        CHECK(stf.text(7) == "?gua");                                            // acento vira '?' (o HUD so desenha ASCII)
        CHECK(stf.text(999).empty());
        Stf bad;
        CHECK(!bad.load(bytes.data(), 8, err));
        std::vector<std::uint8_t> huge(12, 0); huge[8] = 0xff; huge[9] = 0xff; huge[10] = 0xff; huge[11] = 0x7f;
        CHECK(!bad.load(huge.data(), huge.size(), err) && !bad.loaded() && bad.text(0).empty());
        // UTF-16LE
        std::vector<std::uint8_t> u(12, 0); u[8] = 1;
        auto p32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) u.push_back((v >> (8 * i)) & 0xff); };
        p32(28); p32(4); for (int i = 0; i < 8; ++i) u.push_back(0);
        for (char c : std::string("Ok")) { u.push_back(static_cast<std::uint8_t>(c)); u.push_back(0); }
        Stf s16;
        CHECK(s16.load(u.data(), u.size(), err) && s16.text(0) == "Ok");
        CHECK(ascii_only("Caf\xC3\xA9 \xE9") == "Caf? ?");
    }

    // ---------------------------------------------------------------- biblioteca
    const fs::path tmp = fs::temp_directory_path() / "f3vb_test_scripts_v17";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    const auto script = make_dialog_script();
    auto write = [&](const std::string& n, const std::vector<std::uint8_t>& b) { std::ofstream f(tmp / n, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size())); };
    write("00 Teste.amx", script);
    write("MainMenu.amx", script);
    write("leia.txt", {1, 2, 3});
    ScriptLibrary lib;
    CHECK(lib.scan(tmp.string()) == 2 && lib.size() == 2);
    CHECK(!lib.find("00 Teste.amx").empty() && !lib.find("Scripts\\00 TESTE.AMX").empty() && !lib.find("mainmenu").empty() && lib.find("nope").empty());
    CHECK(ScriptLibrary::normalize("C:\\x\\Foo:Bar.AMX ") == "foo_bar");
    CHECK(lib.scan((tmp / "nao_existe").string()) == 0);
    lib.scan(tmp.string());

    // ---------------------------------------------------------------- scripts do mapa
    {
        MapData m;
        MapEntity a; a.script = "00 Teste.amx";
        MapEntity b; b.script = "00 teste";
        MapEntity c; c.script = "MainMenu";
        MapEntity d; d.script = "Falta.amx";
        MapEntity e;                                                           // sem script
        m.entities = {a, b, c, d, e};
        const auto refs = map_scripts(m, lib);
        CHECK(refs.size() == 3);
        std::size_t total = 0, missing = 0;
        for (const auto& r : refs) { total += r.entities; missing += r.path.empty(); }
        CHECK(total == 4 && missing == 1);
        CHECK(refs[0].name == "00 Teste.amx" && refs[0].entities == 2 && !refs[0].path.empty());
    }


    // ---------------------------------------------------------------- scripts de dialogo e casamento por nome (v18)
    {
        const fs::path d2 = fs::temp_directory_path() / "f3vb_test_scripts_v18";
        fs::remove_all(d2);
        fs::create_directories(d2);
        auto w2 = [&](const std::string& n, const std::vector<std::uint8_t>& b) { std::ofstream f(d2 / n, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size())); };
        w2("00 Corporal Armstrong.amx", make_dialog_script());
        w2("10 Mormon.amx", make_dialog_script());
        w2("CaptainArmstrong.amx", make_debug_script());                       // comportamento: NAO tem OnStartDialogue
        w2("lixo.amx", {1, 2, 3});                                             // invalido: ignorado
        ScriptLibrary l2;
        CHECK(l2.scan(d2.string()) == 4);
        const auto& dl = l2.dialogue_scripts();
        CHECK(dl.size() == 2 && dl[0].name == "00 Corporal Armstrong" && dl[1].name == "10 Mormon");
        auto m = l2.match_dialogue("Critters00_TMCpl_Armstrong.CRT Cpl_Armstrong00_TM_000");
        CHECK(m.size() == 1 && m[0].name == "00 Corporal Armstrong");
        CHECK(l2.match_dialogue("Critters00_TMCommunist_Insurgent.CRT").empty());
        CHECK(l2.match_dialogue("MORMON_guy").size() == 1);
        fs::remove_all(d2);
    }

    // ---------------------------------------------------------------- dialogo
    {
        auto si = ScriptInstance::from_file(lib.find("00 Teste.amx"), "00 Teste", 1000, 1, err);
        CHECK(si != nullptr);
        CHECK(si->vm->unresolved_natives().empty());
        const auto bytes = make_stf(50);
        Stf stf;
        CHECK(stf.load(bytes.data(), bytes.size(), err));
        DialogSession d(*si, &stf);
        CHECK(d.start());
        CHECK(!d.finished() && d.error().empty());
        CHECK(d.node_name() == "Node 1");
        // v19: ids de texto 1-based (id N = entrada N-1: o texto 10 e "texto 9") e TODAS as respostas registradas valem (a funcao de texto ja filtrou);
        // o 3o argumento do RegisterReply e a ACAO (RC2/RC3 aqui) e o 4o o DESTINO (RLn)
        CHECK(d.lines().size() == 1 && d.lines()[0].who == "NPC" && d.lines()[0].text_id == 10 && d.lines()[0].text == "texto 9");
        CHECK(d.choices().size() == 3 && d.hidden_choices() == 0);
        CHECK(d.choices()[0].reply_id == 1 && d.choices()[0].text == "texto 19" && d.choices()[0].link_fn == "RL1_N1");
        CHECK(d.choices()[2].reply_id == 3 && d.choices()[2].action_fn == "RC3_N1" && d.choices()[2].link_fn == "RL3_N1");
        CHECK(!d.choose(5));
        // resposta 1: o NPC fala 30 e o no passa a oferecer a resposta de NU2_N1
        CHECK(d.choose(0));
        CHECK(!d.finished() && d.choices().size() == 1 && d.choices()[0].text_id == 40 && d.hidden_choices() == 0);
        auto lt = line_texts(d);
        CHECK(lt.size() == 3 && lt[1] == "Jogador: texto 19" && lt[2] == "NPC: texto 29");
        // ultima resposta: PCSpeaker(41) e EndDialogue
        CHECK(d.choose(0));
        CHECK(d.finished() && d.choices().empty());
        lt = line_texts(d);
        CHECK(lt.size() == 5 && lt[3] == "Jogador: texto 39" && lt[4] == "Jogador: texto 40");
        CHECK(!d.choose(0));                                                    // depois do fim nada mais acontece

        // reiniciar e escolher a resposta 3: acao RC3_N1 (devolve 1, sem efeito) e destino RL3_N1 (NPC fala 31 + EndDialogue)
        CHECK(d.start());
        CHECK(d.choices().size() == 3 && d.lines().size() == 1);
        CHECK(d.choose(2));
        CHECK(d.finished());
        lt = line_texts(d);
        CHECK(lt.size() == 3 && lt[1] == "Jogador: texto 21" && lt[2] == "NPC: texto 30");

        // resposta 2: o destino RL2_N1 nao muda de no: o dialogo acaba (antes ficava preso na mesma pergunta)
        CHECK(d.start());
        CHECK(d.choose(1));
        CHECK(d.finished());

        // codigos de cor do .stf: "{c255}Ola" -> "Ola" com cor 255 (COLORREF)
        std::int32_t col = 0;
        CHECK(strip_color_codes("{c255}Ola", col) == "Ola" && col == 255);
        CHECK(strip_color_codes("{c16711680}Who?", col) == "Who?" && col == 16711680);
        CHECK(strip_color_codes("sem cor {x}", col) == "sem cor {x}" && col == -1);

        // sem a tabela de strings: as falas continuam, so sem texto
        DialogSession d2(*si, nullptr);
        CHECK(d2.start() && d2.lines().size() == 1 && d2.lines()[0].text.empty() && d2.lines()[0].text_id == 10);
        CHECK(!d2.trace.empty());
    }
    {   // script sem OnStartDialogue
        std::string e2;
        auto si = ScriptInstance::from_bytes(make_dialog_script(false), "sem dialogo", 1000, 1, e2);
        CHECK(si != nullptr);
        DialogSession d(*si, nullptr);
        CHECK(d.start());                                                       // existe a public, mas nao cria no nenhum: termina limpo
        CHECK(d.finished() && d.choices().empty());
        const auto sv = survey_script(*si);
        CHECK(sv.loaded && sv.has_dialogue);
    }


    // ---------------------------------------------------------------- script com simbolos de depuracao (regressao do v17)
    {
        std::string e5;
        auto si = ScriptInstance::from_bytes(make_debug_script(), "debug", 1000, 1, e5);
        CHECK(si != nullptr);
        CHECK(si->vm->unresolved_natives().empty());                           // a native importada 2 vezes foi registrada nas 2 entradas
        const std::int32_t arg[1] = {777};
        CHECK(si->vm->call("OnCreate", std::span<const std::int32_t>(arg, 1)) == f3vb::amx::Err::None);
        const auto cell = si->vm->read_cell(4);
        CHECK(cell && *cell == 777);                                           // LOAD.S.pri 12 leu o 1o parametro depois de passar por LINE e SYMBOL
        CHECK(si->vm->call("OnCreate", {}) != f3vb::amx::Err::InvInstr);       // sem parametro: acesso a memoria invalido, mas NAO "instrucao invalida"
        CHECK(si->vm->call("Call2", {}) == f3vb::amx::Err::None);
        CHECK(!si->host->nodes().empty() && si->host->nodes().back().text_fn == "EndDialogue");
        const ScriptSurvey sv = survey_script(*si);
        CHECK(sv.publics == 2 && sv.ran_ok == 2 && sv.ran_fail == 0 && sv.unresolved.empty());   // o levantamento passa 8 zeros: nenhum falso erro
    }

    // ---------------------------------------------------------------- levantamento
    {
        auto si = ScriptInstance::from_file(lib.find("MainMenu"), "MainMenu", 1001, 3, err);
        CHECK(si != nullptr);
        const ScriptSurvey sv = survey_script(*si);
        CHECK(sv.loaded && sv.has_dialogue && sv.publics == 9 && sv.natives == 5);
        CHECK(sv.ran_ok == 9 && sv.ran_fail == 0 && sv.failures.empty() && sv.unresolved.empty());
        CHECK(sv.unimplemented.count("PCSpeaker") == 1 && sv.unimplemented.at("PCSpeaker") == 1);
    }
    {   // arquivo invalido
        std::string e3;
        CHECK(ScriptInstance::from_file((tmp / "leia.txt").string(), "x", 1, 1, e3) == nullptr && !e3.empty());
        CHECK(ScriptInstance::from_file((tmp / "nao.amx").string(), "x", 1, 1, e3) == nullptr);
    }

    // ---------------------------------------------------------------- fuzz: nada pode derrubar o programa
    {
        std::mt19937 rng(17);
        int loaded = 0, started = 0;
        for (int it = 0; it < 3000; ++it) {
            std::vector<std::uint8_t> b = script;
            const int k = 1 + static_cast<int>(rng() % 5);
            for (int q = 0; q < k; ++q) b[rng() % b.size()] = static_cast<std::uint8_t>(rng());
            if (it % 9 == 0) b.resize(rng() % b.size());
            std::string e4;
            auto si = ScriptInstance::from_bytes(b, "fuzz", 1000, 1, e4);
            if (!si) continue;
            ++loaded;
            si->vm->set_instruction_limit(20000);
            DialogSession d(*si, nullptr);
            if (d.start()) ++started;
            for (int step = 0; step < 4 && !d.finished(); ++step) d.choose(rng() % 3);
            (void)survey_script(*si);
        }
        std::printf("fuzz de scripts: %d de 3000 carregaram, %d iniciaram dialogo\n", loaded, started);
    }

    fs::remove_all(tmp);
    if (fails == 0) std::puts("script tests OK");
    return fails ? 1 : 0;
}
