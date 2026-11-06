// Testes da camada de natives (GameHost) com um script sintetico (nenhum dado do jogo).
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <cstring>
#include <string>
#include <vector>

#include "../src/host/game_host.hpp"

using namespace f3vb;

enum : std::int32_t { CONST_PRI = 11, PUSH_C = 39, STACK = 44, PROC = 46, RETN = 48, HALT = 120, SYSREQ_C = 123 };

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

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static void put_string(std::vector<std::int32_t>& d, std::size_t at_cell, const std::string& s) {   // 1 caractere por celula
    if (d.size() < at_cell + s.size() + 1) d.resize(at_cell + s.size() + 1, 0);
    for (std::size_t i = 0; i < s.size(); ++i) d[at_cell + i] = static_cast<unsigned char>(s[i]);
    d[at_cell + s.size()] = 0;
}

int main() {
    // dados: cel 1 (end. 4) "Node 1"; cel 8 (end. 32) "RL1_N1"; cel 15 (end. 60) ""
    std::vector<std::int32_t> data(16, 0);
    put_string(data, 1, "Node 1");
    put_string(data, 8, "RL1_N1");
    data[15] = 0;
    const std::int32_t fa = host::GameHost::from_float(1.5f), fb = host::GameHost::from_float(2.25f);

    std::vector<std::int32_t> code = {HALT, 0};
    auto emit = [&code](std::initializer_list<std::int32_t> l) { for (std::int32_t v : l) code.push_back(v); };
    const std::int32_t t_add = static_cast<std::int32_t>(code.size());
    emit({PROC, PUSH_C, fb, PUSH_C, fa, PUSH_C, 8, SYSREQ_C, 0, STACK, 12, RETN});
    const std::int32_t t_rnd = static_cast<std::int32_t>(code.size());
    emit({PROC, PUSH_C, 5, PUSH_C, 5, PUSH_C, 8, SYSREQ_C, 1, STACK, 12, RETN});
    const std::int32_t t_dlg = static_cast<std::int32_t>(code.size());
    emit({PROC, PUSH_C, 4, PUSH_C, 4, SYSREQ_C, 2, STACK, 8,
                           PUSH_C, 32, PUSH_C, 60, PUSH_C, 3123, PUSH_C, 1, PUSH_C, 16, SYSREQ_C, 3, STACK, 20,
                           CONST_PRI, 0, RETN});
    const std::int32_t t_unk = static_cast<std::int32_t>(code.size());
    emit({PROC, PUSH_C, 42, PUSH_C, 4, SYSREQ_C, 4, STACK, 8, RETN});

    auto bytes = build(code, {{"t_add", t_add}, {"t_rnd", t_rnd}, {"t_dlg", t_dlg}, {"t_unk", t_unk}},
                       {"floatadd", "RandomNumber", "SetCurrentNodeName", "RegisterReply", "Misteriosa"}, data);
    std::string err;
    auto prog = amx::Program::load(bytes, &err);
    if (!prog) { std::printf("load falhou: %s\n", err.c_str()); return 1; }
    amx::Machine m(*prog);
    host::GameHost h(m, 7);
    h.install();
    CHECK(m.unresolved_natives().empty());

    std::int32_t r = 0;
    CHECK(m.call("t_add", {}, &r) == amx::Err::None);
    CHECK(host::GameHost::as_float(r) == 3.75f);
    CHECK(m.call("t_rnd", {}, &r) == amx::Err::None && r == 5);

    h.clear_events();
    CHECK(m.call("t_dlg", {}, &r) == amx::Err::None);
    CHECK(h.nodes().size() == 1);
    CHECK(h.nodes()[0].name == "Node 1");
    CHECK(h.nodes()[0].replies.size() == 1);
    const auto& rep = h.nodes()[0].replies[0];
    CHECK(rep.id == 1 && rep.text_id == 3123 && rep.action_fn.empty() && rep.link_fn == "RL1_N1");

    h.clear_events();
    CHECK(m.call("t_unk", {}, &r) == amx::Err::None && r == 0);
    const auto un = h.unimplemented_calls();
    CHECK(un.count("Misteriosa") == 1 && un.at("Misteriosa") == 1);
    CHECK(h.events().size() == 1 && h.events()[0].args.size() == 1 && h.events()[0].args[0] == 42);

    if (fails == 0) std::puts("C++ host tests OK");
    return fails ? 1 : 0;
}
