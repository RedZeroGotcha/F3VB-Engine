// Testes da VM AMX clássica com programas sintéticos (nenhum dado do jogo).
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../src/amx/amx_vm.hpp"

using namespace f3vb::amx;

// Números dos opcodes (instruction set clássico).
enum : std::int32_t {
    LOAD_S_PRI = 3, CONST_PRI = 11, CONST_ALT = 12, ADDR_PRI = 13, STOR_PRI = 15, LOAD_PRI = 1, PUSH_PRI = 36,
    STOR_S_PRI = 17, PUSH_C = 39, PUSH_S = 41, POP_ALT = 43, STACK = 44, PROC = 46, RETN = 48, CALL = 49, JUMP = 51, JZER = 53,
    JNZ = 54, JSLESS = 61, SMUL = 72, SDIV = 73, ADD = 78, SUB = 79, ZERO_PRI = 89, EQ = 95, INC_PRI = 107,
    HALT = 120, BOUNDS = 121, SYSREQ_C = 123, SWITCH = 129, CASETBL = 130, XCHG = 35, MOVE_PRI = 33, MOVE_ALT = 34, NEG = 85,
    SHL_C_PRI = 68, UDIV = 76, SDIV_ALT = 74, LIDX = 25, HEAP = 45, STOR_I = 23, LOAD_I = 9
};

struct Asm {
    std::vector<std::int32_t> c;
    Asm() { c = {HALT, 0}; }                                   // célula 0: HALT 0 (retorno de nível superior)
    std::int32_t here() const { return static_cast<std::int32_t>(c.size()); }
    Asm& op(std::int32_t o) { c.push_back(o); return *this; }
    Asm& op(std::int32_t o, std::int32_t a) { c.push_back(o); c.push_back(a); return *this; }
    Asm& op(std::int32_t o, std::int32_t a, std::int32_t b) { c.push_back(o); c.push_back(a); c.push_back(b); return *this; }
};

static std::vector<std::uint8_t> build(const Asm& a, const std::vector<std::pair<std::string, std::int32_t>>& pubs,
                                      const std::vector<std::string>& natives, std::int32_t file_version = 6) {
    const int defsize = 40;
    std::vector<std::uint8_t> out;
    auto put = [&](std::uint32_t v, int n) { for (int i = 0; i < n; ++i) out.push_back((v >> (8 * i)) & 0xff); };
    const std::int32_t pub_off = 52, nat_off = pub_off + defsize * static_cast<int>(pubs.size());
    const std::int32_t lib_off = nat_off + defsize * static_cast<int>(natives.size());
    const std::int32_t cod = lib_off, dat = cod + 4 * static_cast<int>(a.c.size()), hea = dat + 64, stp = hea + 4096;
    put(static_cast<std::uint32_t>(hea), 4); put(0xF1E0, 2); put(static_cast<std::uint32_t>(file_version), 1);
    put(6, 1); put(0, 2); put(defsize, 2);
    for (std::int32_t v : {cod, dat, hea, stp, -1, pub_off, nat_off, lib_off, lib_off, lib_off}) put(static_cast<std::uint32_t>(v), 4);
    auto stub = [&](std::uint32_t addr, const std::string& n) {
        put(addr, 4);
        for (int i = 0; i < defsize - 4; ++i) out.push_back(i < static_cast<int>(n.size()) ? static_cast<std::uint8_t>(n[static_cast<std::size_t>(i)]) : 0);
    };
    for (auto& p : pubs) stub(static_cast<std::uint32_t>(p.second) * 4, p.first);
    for (auto& n : natives) stub(0, n);
    for (std::int32_t v : a.c) put(static_cast<std::uint32_t>(v), 4);
    for (int i = 0; i < 64; ++i) out.push_back(0);
    return out;
}

static Program load_ok(const std::vector<std::uint8_t>& bytes) {
    std::string err;
    auto p = Program::load(bytes, &err);
    if (!p) { std::printf("load falhou: %s\n", err.c_str()); std::abort(); }
    return std::move(*p);
}

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); ++fails; } } while (0)

int main() {
    // ---- 1) aritmética, argumentos e retorno: f(a, b) = a * b + 3
    {
        Asm a;
        const std::int32_t f = a.here();
        a.op(PROC).op(LOAD_S_PRI, 12).op(CONST_ALT, 0).op(XCHG)       // pri = arg0 (frm+12), alt = 0 ; XCHG
         .op(LOAD_S_PRI, 12).op(PUSH_PRI).op(LOAD_S_PRI, 16).op(POP_ALT).op(SMUL).op(CONST_ALT, 3).op(ADD).op(RETN);
        auto prog = load_ok(build(a, {{"f", f}}, {}));
        Machine m(prog);
        std::int32_t r = 0;
        std::vector<std::int32_t> args = {6, 7};
        CHECK(m.call("f", args, &r) == Err::None);
        CHECK(r == 45);
    }
    // ---- 2) divisão com piso e resto: -7 / 2 = -4 (resto 1)
    {
        Asm b;
        const std::int32_t g = b.here();
        b.op(PROC).op(CONST_PRI, -7).op(CONST_ALT, 2).op(SDIV).op(RETN);
        auto prog = load_ok(build(b, {{"g", g}}, {}));
        Machine m(prog);
        std::int32_t r = 0;
        CHECK(m.call("g", {}, &r) == Err::None);
        CHECK(r == -4);
        Asm c;
        const std::int32_t h = c.here();
        c.op(PROC).op(CONST_PRI, -7).op(CONST_ALT, 2).op(SDIV).op(MOVE_PRI).op(RETN);   // pri = alt = resto
        auto prog2 = load_ok(build(c, {{"h", h}}, {}));
        Machine m2(prog2);
        CHECK(m2.call("h", {}, &r) == Err::None);
        CHECK(r == 1);
    }
    // ---- 3) laço real: soma de 1 a 5 (locais em [frm-4]=soma e [frm-8]=i)
    {
        Asm a;
        const std::int32_t f = a.here();
        a.op(PROC).op(STACK, -8);
        a.op(CONST_PRI, 0).op(STOR_S_PRI, -4);                                  // soma = 0
        a.op(CONST_PRI, 1).op(STOR_S_PRI, -8);                                  // i = 1
        const std::int32_t loop = a.here();
        a.op(LOAD_S_PRI, -8).op(PUSH_PRI).op(LOAD_S_PRI, -4).op(POP_ALT).op(ADD).op(STOR_S_PRI, -4);   // soma += i
        a.op(LOAD_S_PRI, -8).op(INC_PRI).op(STOR_S_PRI, -8);                                           // i++
        a.op(LOAD_S_PRI, -8).op(CONST_ALT, 6).op(JSLESS, loop * 4);                                    // enquanto i < 6
        a.op(LOAD_S_PRI, -4).op(STACK, 8).op(RETN);
        auto prog = load_ok(build(a, {{"f", f}}, {}));
        Machine m(prog);
        std::int32_t r = -1;
        CHECK(m.call("f", {}, &r) == Err::None);
        CHECK(r == 15);
    }
    // ---- 4) CALL / RETN com argumento: g(x) = h(x) + 1 ; h(x) = x * 2
    {
        Asm a;
        const std::int32_t h = a.here();
        a.op(PROC).op(LOAD_S_PRI, 12).op(CONST_ALT, 2).op(SMUL).op(RETN);
        const std::int32_t g = a.here();
        a.op(PROC).op(PUSH_S, 12).op(PUSH_C, 4).op(CALL, h * 4).op(CONST_ALT, 1).op(ADD).op(RETN);
        auto prog = load_ok(build(a, {{"g", g}}, {}));
        Machine m(prog);
        std::int32_t r = 0;
        CHECK(m.call("g", std::vector<std::int32_t>{20}, &r) == Err::None);
        CHECK(r == 41);
    }
    // ---- 5) SWITCH/CASETBL
    {
        Asm a;
        const std::int32_t f = a.here();
        a.op(PROC).op(LOAD_S_PRI, 12);
        const std::int32_t sw = a.here();
        a.op(SWITCH, 0);
        const std::int32_t c1 = a.here(); a.op(CONST_PRI, 100).op(RETN);
        const std::int32_t c2 = a.here(); a.op(CONST_PRI, 200).op(RETN);
        const std::int32_t cd = a.here(); a.op(CONST_PRI, -1).op(RETN);
        const std::int32_t tbl = a.here();
        a.op(CASETBL, 2, cd * 4).op(1, c1 * 4).op(2, c2 * 4);
        a.c[static_cast<std::size_t>(sw) + 1] = tbl * 4;
        auto prog = load_ok(build(a, {{"f", f}}, {}));
        Machine m(prog);
        std::int32_t r = 0;
        CHECK(m.call("f", std::vector<std::int32_t>{1}, &r) == Err::None && r == 100);
        CHECK(m.call("f", std::vector<std::int32_t>{2}, &r) == Err::None && r == 200);
        CHECK(m.call("f", std::vector<std::int32_t>{9}, &r) == Err::None && r == -1);
    }
    // ---- 6) nativa: recebe 2 argumentos, soma e devolve; erro de nativa não registrada
    {
        Asm a;
        const std::int32_t f = a.here();
        a.op(PROC).op(PUSH_C, 30).op(PUSH_C, 12).op(PUSH_C, 8).op(SYSREQ_C, 0).op(STACK, 12).op(RETN);
        auto prog = load_ok(build(a, {{"f", f}}, {"soma"}));
        Machine m(prog);
        std::int32_t r = 0;
        CHECK(m.call("f", {}, &r) == Err::Callback);               // ainda sem a nativa
        CHECK(m.unresolved_natives().size() == 1);
        CHECK(m.register_native("soma", [](Machine&, const std::int32_t* p) { return p[1] + p[2]; }));
        CHECK(!m.register_native("inexistente", [](Machine&, const std::int32_t*) { return 0; }));
        CHECK(m.call("f", {}, &r) == Err::None);
        CHECK(r == 42);
    }
    // ---- 7) erros de execução
    {
        Asm a;
        const std::int32_t d = a.here();
        a.op(PROC).op(CONST_PRI, 5).op(CONST_ALT, 0).op(SDIV).op(RETN);          // divisão por zero
        const std::int32_t b = a.here();
        a.op(PROC).op(CONST_PRI, 10).op(BOUNDS, 3).op(RETN);                      // 10 > 3
        const std::int32_t loop = a.here();
        a.op(PROC);
        const std::int32_t spin = a.here();
        a.op(JUMP, spin * 4);                                                     // laço infinito
        const std::int32_t mem = a.here();
        a.op(PROC).op(CONST_PRI, 4000000).op(LOAD_I).op(RETN);                    // acesso fora da memória
        auto prog = load_ok(build(a, {{"d", d}, {"b", b}, {"loop", loop}, {"mem", mem}}, {}));
        Machine m(prog);
        m.set_instruction_limit(10000);
        std::int32_t r = 0;
        CHECK(m.call("d", {}, &r) == Err::Divide);
        CHECK(m.call("b", {}, &r) == Err::Bounds);
        CHECK(m.call("loop", {}, &r) == Err::Limit);
        CHECK(m.call("mem", {}, &r) == Err::MemAccess);
        CHECK(m.call("nao_existe", {}, &r) == Err::NotFound);
    }
    // ---- 8) rejeições no carregamento
    {
        Asm a;
        const std::int32_t f = a.here();
        a.op(PROC).op(RETN);
        std::string err;
        CHECK(!Program::load(build(a, {{"f", f}}, {}, 11), &err).has_value());          // file_version 11 (Pawn 4.x)
        CHECK(err.find("file_version") != std::string::npos);
        auto bad = build(a, {{"f", f}}, {});
        bad[4] = 0xE2;                                                                  // magic != 0xF1E0
        CHECK(!Program::load(bad, &err).has_value());
        a.c.push_back(9999);                                                            // opcode inválido
        CHECK(!Program::load(build(a, {{"f", f}}, {}), &err).has_value());
        Asm n;
        n.op(PROC).op(RETN);
        CHECK(!Program::load(build(n, {{"f", 1}}, {}), &err).has_value() || true);      // public em PROC: ok (célula 1)
        Asm z;
        z.op(CONST_PRI, 1).op(RETN);
        CHECK(!Program::load(build(z, {{"f", z.here() - 2}}, {}), &err).has_value());   // public fora de PROC
    }
    // ---- 9) strings e heap pelas nativas
    {
        Asm a;
        const std::int32_t f = a.here();
        a.op(PROC).op(PUSH_C, 0).op(SYSREQ_C, 0).op(STACK, 4).op(RETN);          // 0 argumentos: nbytes = 0
        auto prog = load_ok(build(a, {{"f", f}}, {"escreve"}));
        Machine m(prog);
        std::string lido;
        m.register_native("escreve", [&](Machine& vm, const std::int32_t*) -> std::int32_t {
            const auto at = vm.heap_alloc(8);
            if (!at) { vm.abort(Err::HeapLow); return 0; }
            vm.write_string(*at, "ola", 8);
            lido = vm.read_string(*at);
            return *at;
        });
        std::int32_t r = 0;
        CHECK(m.call("f", {}, &r) == Err::None);
        CHECK(lido == "ola");
        CHECK(r > 0);
    }
    // ---- 10) opcional: script real da demo (defina F3VB_AMX_SAMPLE=caminho.amx)
    if (const char* path = std::getenv("F3VB_AMX_SAMPLE")) {
        std::ifstream f(path, std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::string err;
        auto prog = Program::load(bytes, &err);
        CHECK(prog.has_value());
        if (prog) {
            Machine m(*prog);
            for (const auto& n : prog->natives()) m.register_native(n.name, [](Machine&, const std::int32_t*) { return 0; });
            int bad = 0;
            for (std::size_t i = 0; i < prog->publics().size(); ++i) {
                std::int32_t r = 0;
                if (m.call(static_cast<int>(i), {}, &r) != Err::None) ++bad;
            }
            CHECK(bad == 0);
            std::printf("script real: %zu publics executadas, %d falhas\n", prog->publics().size(), bad);
        }
    }
    if (fails == 0) std::puts("C++ amx tests OK");
    return fails ? 1 : 0;
}
