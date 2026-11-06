#include "amx_vm.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace f3vb::amx {

namespace {
// Opcodes do conjunto clássico (índice = número do opcode).
enum Op : std::int32_t {
    NONE, LOAD_PRI, LOAD_ALT, LOAD_S_PRI, LOAD_S_ALT, LREF_PRI, LREF_ALT, LREF_S_PRI, LREF_S_ALT, LOAD_I, LODB_I,
    CONST_PRI, CONST_ALT, ADDR_PRI, ADDR_ALT, STOR_PRI, STOR_ALT, STOR_S_PRI, STOR_S_ALT, SREF_PRI, SREF_ALT,
    SREF_S_PRI, SREF_S_ALT, STOR_I, STRB_I, LIDX, LIDX_B, IDXADDR, IDXADDR_B, ALIGN_PRI, ALIGN_ALT, LCTRL, SCTRL,
    MOVE_PRI, MOVE_ALT, XCHG, PUSH_PRI, PUSH_ALT, PUSH_R, PUSH_C, PUSH, PUSH_S, POP_PRI, POP_ALT, STACK, HEAP,
    PROC, RET, RETN, CALL, CALL_PRI, JUMP, JREL, JZER, JNZ, JEQ, JNEQ, JLESS, JLEQ, JGRTR, JGEQ, JSLESS, JSLEQ,
    JSGRTR, JSGEQ, SHL, SHR, SSHR, SHL_C_PRI, SHL_C_ALT, SHR_C_PRI, SHR_C_ALT, SMUL, SDIV, SDIV_ALT, UMUL, UDIV,
    UDIV_ALT, ADD, SUB, SUB_ALT, AND, OR, XOR, NOT, NEG, INVERT, ADD_C, SMUL_C, ZERO_PRI, ZERO_ALT, ZERO, ZERO_S,
    SIGN_PRI, SIGN_ALT, EQ, NEQ, LESS, LEQ, GRTR, GEQ, SLESS, SLEQ, SGRTR, SGEQ, EQ_C_PRI, EQ_C_ALT, INC_PRI,
    INC_ALT, INC, INC_S, INC_I, DEC_PRI, DEC_ALT, DEC, DEC_S, DEC_I, MOVS, CMPS, FILL, HALT, BOUNDS, SYSREQ_PRI,
    SYSREQ_C, FILE, LINE, SYMBOL, SRANGE, JUMP_PRI, SWITCH, CASETBL, SWAP_PRI, SWAP_ALT, PUSH_ADR, NOP, SYSREQ_D,
    SYMTAG, BREAK, OP_COUNT
};

const std::vector<OpInfo> kTable = {
    {"NONE", 0}, {"LOAD.pri", 1}, {"LOAD.alt", 1}, {"LOAD.S.pri", 1}, {"LOAD.S.alt", 1}, {"LREF.pri", 1},
    {"LREF.alt", 1}, {"LREF.S.pri", 1}, {"LREF.S.alt", 1}, {"LOAD.I", 0}, {"LODB.I", 1}, {"CONST.pri", 1},
    {"CONST.alt", 1}, {"ADDR.pri", 1}, {"ADDR.alt", 1}, {"STOR.pri", 1}, {"STOR.alt", 1}, {"STOR.S.pri", 1},
    {"STOR.S.alt", 1}, {"SREF.pri", 1}, {"SREF.alt", 1}, {"SREF.S.pri", 1}, {"SREF.S.alt", 1}, {"STOR.I", 0},
    {"STRB.I", 1}, {"LIDX", 0}, {"LIDX.B", 1}, {"IDXADDR", 0}, {"IDXADDR.B", 1}, {"ALIGN.pri", 1},
    {"ALIGN.alt", 1}, {"LCTRL", 1}, {"SCTRL", 1}, {"MOVE.pri", 0}, {"MOVE.alt", 0}, {"XCHG", 0},
    {"PUSH.pri", 0}, {"PUSH.alt", 0}, {"PUSH.R", 1}, {"PUSH.C", 1}, {"PUSH", 1}, {"PUSH.S", 1}, {"POP.pri", 0},
    {"POP.alt", 0}, {"STACK", 1}, {"HEAP", 1}, {"PROC", 0}, {"RET", 0}, {"RETN", 0}, {"CALL", 1},
    {"CALL.pri", 0}, {"JUMP", 1}, {"JREL", 1}, {"JZER", 1}, {"JNZ", 1}, {"JEQ", 1}, {"JNEQ", 1}, {"JLESS", 1},
    {"JLEQ", 1}, {"JGRTR", 1}, {"JGEQ", 1}, {"JSLESS", 1}, {"JSLEQ", 1}, {"JSGRTR", 1}, {"JSGEQ", 1},
    {"SHL", 0}, {"SHR", 0}, {"SSHR", 0}, {"SHL.C.pri", 1}, {"SHL.C.alt", 1}, {"SHR.C.pri", 1},
    {"SHR.C.alt", 1}, {"SMUL", 0}, {"SDIV", 0}, {"SDIV.alt", 0}, {"UMUL", 0}, {"UDIV", 0}, {"UDIV.alt", 0},
    {"ADD", 0}, {"SUB", 0}, {"SUB.alt", 0}, {"AND", 0}, {"OR", 0}, {"XOR", 0}, {"NOT", 0}, {"NEG", 0},
    {"INVERT", 0}, {"ADD.C", 1}, {"SMUL.C", 1}, {"ZERO.pri", 0}, {"ZERO.alt", 0}, {"ZERO", 1}, {"ZERO.S", 1},
    {"SIGN.pri", 0}, {"SIGN.alt", 0}, {"EQ", 0}, {"NEQ", 0}, {"LESS", 0}, {"LEQ", 0}, {"GRTR", 0}, {"GEQ", 0},
    {"SLESS", 0}, {"SLEQ", 0}, {"SGRTR", 0}, {"SGEQ", 0}, {"EQ.C.pri", 1}, {"EQ.C.alt", 1}, {"INC.pri", 0},
    {"INC.alt", 0}, {"INC", 1}, {"INC.S", 1}, {"INC.I", 0}, {"DEC.pri", 0}, {"DEC.alt", 0}, {"DEC", 1},
    {"DEC.S", 1}, {"DEC.I", 0}, {"MOVS", 1}, {"CMPS", 1}, {"FILL", 1}, {"HALT", 1}, {"BOUNDS", 1},
    {"SYSREQ.pri", 0}, {"SYSREQ.C", 1}, {"FILE", -1}, {"LINE", 2}, {"SYMBOL", -1}, {"SRANGE", 2},
    {"JUMP.pri", 0}, {"SWITCH", 1}, {"CASETBL", -2}, {"SWAP.pri", 0}, {"SWAP.alt", 0}, {"PUSH.ADR", 1},
    {"NOP", 0}, {"SYSREQ.D", 1}, {"SYMTAG", 1}, {"BREAK", 0},
};

constexpr std::int32_t kCell = 4;

template <class T>
T rd(const std::uint8_t* p) {
    T v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

bool fail(std::string* e, const std::string& m) {
    if (e) *e = m;
    return false;
}

std::string cstr(std::span<const std::uint8_t> b, std::size_t off, std::size_t max) {
    std::string s;
    for (std::size_t i = off; i < b.size() && i < off + max && b[i]; ++i) s.push_back(static_cast<char>(b[i]));
    return s;
}
}  // namespace

const std::vector<OpInfo>& opcode_table() { return kTable; }

const char* err_name(Err e) {
    switch (e) {
        case Err::None: return "ok";
        case Err::Exit: return "saida forcada";
        case Err::Assert: return "assert falhou";
        case Err::StackErr: return "colisao pilha/heap";
        case Err::Bounds: return "indice fora dos limites";
        case Err::MemAccess: return "acesso invalido a memoria";
        case Err::InvInstr: return "instrucao invalida";
        case Err::StackLow: return "pilha abaixo do inicio";
        case Err::HeapLow: return "heap abaixo do inicio";
        case Err::Callback: return "nativa nao registrada";
        case Err::Native: return "nativa falhou";
        case Err::Divide: return "divisao por zero";
        case Err::Sleep: return "sleep";
        case Err::InvState: return "estado invalido";
        case Err::Memory: return "sem memoria";
        case Err::Format: return "formato invalido";
        case Err::Version: return "versao nao suportada";
        case Err::NotFound: return "funcao nao encontrada";
        case Err::Params: return "parametros invalidos";
        case Err::Limit: return "limite de instrucoes";
    }
    return "?";
}

// ---------------------------------------------------------------- Program
std::optional<Program> Program::load(std::span<const std::uint8_t> b, std::string* error) {
    constexpr std::size_t kHdr = 52;    // file_version 6: sem o campo 'nametable' (surgiu na versao 7)
    if (b.size() < kHdr) return fail(error, "arquivo menor que o cabecalho AMX"), std::nullopt;
    Program p;
    Header& h = p.hdr_;
    h.size = rd<std::int32_t>(&b[0]);
    h.magic = rd<std::uint16_t>(&b[4]);
    h.file_version = static_cast<std::int8_t>(b[6]);
    h.amx_version = static_cast<std::int8_t>(b[7]);
    h.flags = rd<std::int16_t>(&b[8]);
    h.defsize = rd<std::int16_t>(&b[10]);
    h.cod = rd<std::int32_t>(&b[12]); h.dat = rd<std::int32_t>(&b[16]); h.hea = rd<std::int32_t>(&b[20]);
    h.stp = rd<std::int32_t>(&b[24]); h.cip = rd<std::int32_t>(&b[28]); h.publics = rd<std::int32_t>(&b[32]);
    h.natives = rd<std::int32_t>(&b[36]); h.libraries = rd<std::int32_t>(&b[40]);
    h.pubvars = rd<std::int32_t>(&b[44]); h.tags = rd<std::int32_t>(&b[48]); h.nametable = 0;

    if (h.magic != 0xF1E0) return fail(error, "magic != 0xF1E0 (so celulas de 32 bits)"), std::nullopt;
    if (h.file_version != 6) {
        if (error) *error = "file_version " + std::to_string(h.file_version) + " (esta VM so aceita 6)";
        return std::nullopt;
    }
    if (h.defsize < 8) return fail(error, "defsize invalido"), std::nullopt;
    const auto sz = static_cast<std::int64_t>(b.size());
    if (h.size > sz) return fail(error, "tamanho declarado maior que o arquivo"), std::nullopt;
    if (!(static_cast<std::int64_t>(kHdr) <= h.cod && h.cod <= h.dat && h.dat <= h.hea && h.hea <= h.stp &&
          h.hea <= sz && (h.dat - h.cod) % kCell == 0))
        return fail(error, "cod/dat/hea/stp fora de ordem ou fora do arquivo"), std::nullopt;
    if (!(h.publics <= h.natives && h.natives <= h.libraries && h.libraries <= h.pubvars && h.pubvars <= h.tags &&
          h.tags <= h.cod && h.publics >= static_cast<std::int32_t>(kHdr) && h.libraries <= h.cod))
        return fail(error, "tabelas de funcoes fora de ordem"), std::nullopt;

    auto read_table = [&](std::int32_t start, std::int32_t end, std::vector<FuncStub>& out) -> bool {
        if ((end - start) % h.defsize != 0) return false;
        for (std::int32_t o = start; o < end; o += h.defsize) {
            if (o + h.defsize > sz) return false;
            FuncStub f;
            f.address = rd<std::uint32_t>(&b[static_cast<std::size_t>(o)]);
            if (h.defsize == 8) {                                   // formato novo: nome na tabela de nomes
                const auto ofs = rd<std::uint32_t>(&b[static_cast<std::size_t>(o) + 4]);
                if (ofs >= b.size()) return false;
                f.name = cstr(b, ofs, 64);
            } else {                                                // formato antigo: nome embutido
                f.name = cstr(b, static_cast<std::size_t>(o) + 4, static_cast<std::size_t>(h.defsize) - 4);
            }
            out.push_back(std::move(f));
        }
        return true;
    };
    if (!read_table(h.publics, h.natives, p.publics_) || !read_table(h.natives, h.libraries, p.natives_))
        return fail(error, "tabela de publics/natives invalida"), std::nullopt;

    // Código: decodifica tudo, marcando o início de cada instrução.
    const std::size_t cells = static_cast<std::size_t>(h.dat - h.cod) / kCell;
    p.code_.resize(cells);
    if (cells) std::memcpy(p.code_.data(), &b[static_cast<std::size_t>(h.cod)], cells * kCell);
    p.starts_.assign(cells, false);
    for (std::size_t i = 0; i < cells;) {
        p.starts_[i] = true;
        const std::int32_t op = p.code_[i];
        if (op < 0 || op >= static_cast<std::int32_t>(kTable.size()))
            return fail(error, "opcode invalido " + std::to_string(op) + " na celula " + std::to_string(i)), std::nullopt;
        std::int64_t np = kTable[static_cast<std::size_t>(op)].params;
        if (np == -1) {                                              // FILE / SYMBOL: size (bytes de payload)
            if (i + 1 >= cells) return fail(error, "FILE/SYMBOL truncado"), std::nullopt;
            const std::int32_t s = p.code_[i + 1];
            if (s < 0 || s % kCell != 0) return fail(error, "FILE/SYMBOL com size invalido"), std::nullopt;
            np = 1 + s / kCell;
        } else if (np == -2) {                                       // CASETBL: num, default, num pares
            if (i + 1 >= cells) return fail(error, "CASETBL truncado"), std::nullopt;
            const std::int32_t n = p.code_[i + 1];
            if (n < 0 || n > 100000) return fail(error, "CASETBL absurdo"), std::nullopt;
            np = 2 + 2 * static_cast<std::int64_t>(n);
        }
        if (i + 1 + static_cast<std::size_t>(np) > cells)
            return fail(error, std::string("instrucao ") + kTable[static_cast<std::size_t>(op)].name + " ultrapassa o codigo"), std::nullopt;
        i += 1 + static_cast<std::size_t>(np);
    }
    for (const auto& f : p.publics_)
        if (f.address % kCell != 0 || !p.is_instruction_start(f.address / kCell))
            return fail(error, "public '" + f.name + "' nao aponta para uma instrucao"), std::nullopt;

    p.data_.assign(b.begin() + h.dat, b.begin() + h.hea);
    return p;
}

int Program::find_public(std::string_view n) const {
    for (std::size_t i = 0; i < publics_.size(); ++i) if (publics_[i].name == n) return static_cast<int>(i);
    return -1;
}
int Program::find_native(std::string_view n) const {
    for (std::size_t i = 0; i < natives_.size(); ++i) if (natives_[i].name == n) return static_cast<int>(i);
    return -1;
}

// ---------------------------------------------------------------- Machine
Machine::Machine(const Program& p) : prog_(p), mem_(static_cast<std::size_t>(p.memory_size()), 0),
                                      natives_(p.natives().size()), op_counts_(OP_COUNT, 0) {
    std::copy(p.initial_data().begin(), p.initial_data().end(), mem_.begin());
    hea_ = p.heap_start();
    stk_ = p.memory_size();
}

bool Machine::register_native(std::string_view name, NativeFn fn) {
    // Um script pode importar o mesmo nome mais de uma vez (include repetido): registra TODAS as entradas com esse nome (v17).
    bool any = false;
    for (std::size_t i = 0; i < prog_.natives().size(); ++i) {
        if (prog_.natives()[i].name != name) continue;
        natives_[i] = fn;
        any = true;
    }
    return any;
}

std::vector<std::string> Machine::unresolved_natives() const {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < natives_.size(); ++i) if (!natives_[i]) out.push_back(prog_.natives()[i].name);
    return out;
}

bool Machine::valid(std::int64_t addr, std::int64_t len) const {
    if (addr < 0 || len < 0) return false;
    const std::int64_t end = addr + len;
    if (end > static_cast<std::int64_t>(mem_.size())) return false;
    return !(addr < stk_ && end > hea_);          // a faixa entre o heap e a pilha é inválida
}

std::optional<std::int32_t> Machine::read_cell(std::int32_t addr) const {
    if (!valid(addr, kCell)) return std::nullopt;
    return rd<std::int32_t>(&mem_[static_cast<std::size_t>(addr)]);
}
bool Machine::write_cell(std::int32_t addr, std::int32_t v) {
    if (!valid(addr, kCell)) return false;
    std::memcpy(&mem_[static_cast<std::size_t>(addr)], &v, kCell);
    return true;
}

std::string Machine::read_string(std::int32_t addr, std::size_t max_len) const {
    std::string s;
    const auto first = read_cell(addr);
    if (!first) return s;
    const bool packed = static_cast<std::uint32_t>(*first) > 0xFFu;
    for (std::int32_t a = addr; s.size() < max_len; a += kCell) {
        const auto c = read_cell(a);
        if (!c) break;
        const auto u = static_cast<std::uint32_t>(*c);
        if (!packed) {
            if (u == 0) break;
            s.push_back(static_cast<char>(u & 0xFF));
        } else {
            bool end = false;
            for (int sh = 24; sh >= 0 && s.size() < max_len; sh -= 8) {
                const char ch = static_cast<char>((u >> sh) & 0xFF);
                if (ch == 0) { end = true; break; }
                s.push_back(ch);
            }
            if (end) break;
        }
    }
    return s;
}

bool Machine::write_string(std::int32_t addr, std::string_view text, std::size_t max_cells) {
    if (max_cells == 0) return false;
    const std::size_t n = std::min(text.size(), max_cells - 1);
    for (std::size_t i = 0; i < n; ++i)
        if (!write_cell(addr + static_cast<std::int32_t>(i) * kCell, static_cast<unsigned char>(text[i]))) return false;
    return write_cell(addr + static_cast<std::int32_t>(n) * kCell, 0);
}

std::optional<std::int32_t> Machine::heap_alloc(std::int32_t cells) {
    if (cells <= 0) return std::nullopt;
    const std::int32_t bytes = cells * kCell;
    if (hea_ + bytes > stk_) return std::nullopt;
    const std::int32_t at = hea_;
    hea_ += bytes;
    return at;
}

Err Machine::call(std::string_view name, std::span<const std::int32_t> args, std::int32_t* result) {
    const int i = prog_.find_public(name);
    return i < 0 ? Err::NotFound : call(i, args, result);
}

Err Machine::call(int idx, std::span<const std::int32_t> args, std::int32_t* result) {
    if (idx < 0 || idx >= static_cast<int>(prog_.publics().size())) return Err::NotFound;
    // Estado limpo a cada chamada (os dados globais do script são preservados entre chamadas).
    stk_ = prog_.memory_size();
    hea_ = prog_.heap_start();
    frm_ = stk_;
    pending_error_ = Err::None;
    call_start_ = executed_;
    auto push = [&](std::int32_t v) -> bool {
        stk_ -= kCell;
        if (stk_ < hea_) return false;
        std::memcpy(&mem_[static_cast<std::size_t>(stk_)], &v, kCell);
        return true;
    };
    for (std::size_t i = args.size(); i-- > 0;) if (!push(args[i])) return Err::StackErr;
    if (!push(static_cast<std::int32_t>(args.size() * kCell)) || !push(0)) return Err::StackErr;   // nº de bytes + retorno 0 (HALT)
    return run(static_cast<std::int32_t>(prog_.publics()[static_cast<std::size_t>(idx)].address / kCell), result);
}

Err Machine::run(std::int32_t cip, std::int32_t* result) {
    const auto& code = prog_.code();
    const auto ncode = static_cast<std::int64_t>(code.size());
    const std::int32_t stp = prog_.memory_size();
    const std::int32_t hea0 = prog_.heap_start();
    std::int32_t pri = 0, alt = 0, frm = frm_, stk = stk_, hea = hea_;
    auto sync = [&] { stk_ = stk; hea_ = hea; frm_ = frm; pri_ = pri; alt_ = alt; };
    auto stop = [&](Err e) -> Err {
        sync();
        if (result) *result = pri;
        return e;
    };
    auto rd32 = [&](std::int64_t a, std::int32_t& v) -> bool {
        stk_ = stk; hea_ = hea;
        if (!valid(a, kCell)) return false;
        std::memcpy(&v, &mem_[static_cast<std::size_t>(a)], kCell);
        return true;
    };
    auto wr32 = [&](std::int64_t a, std::int32_t v) -> bool {
        stk_ = stk; hea_ = hea;
        if (!valid(a, kCell)) return false;
        std::memcpy(&mem_[static_cast<std::size_t>(a)], &v, kCell);
        return true;
    };
    auto W = [](std::int64_t x) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(x)); };   // soma com wrap
    auto push = [&](std::int32_t v) -> bool {
        const std::int64_t ns = std::int64_t{stk} - kCell;
        if (ns < 0 || ns < hea || ns + kCell > stp) return false;
        stk = static_cast<std::int32_t>(ns);
        std::memcpy(&mem_[static_cast<std::size_t>(stk)], &v, kCell);
        return true;
    };
    auto pop = [&](std::int32_t& v) -> bool {
        if (stk < 0 || std::int64_t{stk} + kCell > stp) return false;
        std::memcpy(&v, &mem_[static_cast<std::size_t>(stk)], kCell);
        stk += kCell;
        return true;
    };
    auto jump_to = [&](std::int64_t byte_addr, std::int32_t& out) -> bool {
        if (byte_addr < 0 || byte_addr % kCell != 0) return false;
        const std::int64_t c = byte_addr / kCell;
        if (c >= ncode || !prog_.is_instruction_start(static_cast<std::size_t>(c))) return false;
        out = static_cast<std::int32_t>(c);
        return true;
    };
    auto floor_div = [](std::int32_t num, std::int32_t den, std::int32_t& q, std::int32_t& r) {
        if (num == std::numeric_limits<std::int32_t>::min() && den == -1) { q = num; r = 0; return; }
        q = num / den; r = num % den;
        if (r != 0 && ((r ^ den) < 0)) { --q; r += den; }
    };

    while (true) {
        if (++executed_ - call_start_ > limit_) return stop(Err::Limit);
        if (cip < 0 || cip >= ncode) return stop(Err::InvInstr);
        const std::int32_t op = code[static_cast<std::size_t>(cip)];
        if (op < 0 || op >= OP_COUNT) return stop(Err::InvInstr);
        ++op_counts_[static_cast<std::size_t>(op)];
        const std::int32_t* P = &code[static_cast<std::size_t>(cip) + 1];   // parâmetros (com checagem no carregamento)
        std::int32_t next = cip + 1 + std::max(0, kTable[static_cast<std::size_t>(op)].params);
        if (kTable[static_cast<std::size_t>(op)].params == -1) {         // FILE / SYMBOL (scripts compilados com simbolos de depuracao): [size][size bytes de payload]
            // O tamanho foi validado em Program::load. SEM isto o payload (nomes de simbolos, numeros de linha) era executado como se fosse codigo
            // e TODA public de um script compilado com -d falhava com "instrucao invalida" (v17).
            next = cip + 2 + P[0] / kCell;
        }
        const std::int32_t p0 = (kTable[static_cast<std::size_t>(op)].params >= 1 || kTable[static_cast<std::size_t>(op)].params < 0) ? P[0] : 0;
        std::int32_t t = 0;
        auto bad = [&](Err e) { return stop(e); };
        auto jcond = [&](bool c) -> bool {                // devolve false se o salto é inválido
            if (!c) return true;
            return jump_to(p0, next);
        };
        switch (op) {
            case NONE: return bad(Err::InvInstr);
            case LOAD_PRI: if (!rd32(p0, pri)) return bad(Err::MemAccess); break;
            case LOAD_ALT: if (!rd32(p0, alt)) return bad(Err::MemAccess); break;
            case LOAD_S_PRI: if (!rd32(std::int64_t{frm} + p0, pri)) return bad(Err::MemAccess); break;
            case LOAD_S_ALT: if (!rd32(std::int64_t{frm} + p0, alt)) return bad(Err::MemAccess); break;
            case LREF_PRI: if (!rd32(p0, t) || !rd32(t, pri)) return bad(Err::MemAccess); break;
            case LREF_ALT: if (!rd32(p0, t) || !rd32(t, alt)) return bad(Err::MemAccess); break;
            case LREF_S_PRI: if (!rd32(std::int64_t{frm} + p0, t) || !rd32(t, pri)) return bad(Err::MemAccess); break;
            case LREF_S_ALT: if (!rd32(std::int64_t{frm} + p0, t) || !rd32(t, alt)) return bad(Err::MemAccess); break;
            case LOAD_I: if (!rd32(pri, pri)) return bad(Err::MemAccess); break;
            case LODB_I: {
                if (p0 != 1 && p0 != 2 && p0 != 4) return bad(Err::InvInstr);
                stk_ = stk; hea_ = hea;
                if (!valid(pri, p0)) return bad(Err::MemAccess);
                std::uint32_t v = 0;
                std::memcpy(&v, &mem_[static_cast<std::size_t>(pri)], static_cast<std::size_t>(p0));
                pri = static_cast<std::int32_t>(v);
                break;
            }
            case CONST_PRI: pri = p0; break;
            case CONST_ALT: alt = p0; break;
            case ADDR_PRI: pri = W(std::int64_t{frm} + p0); break;
            case ADDR_ALT: alt = W(std::int64_t{frm} + p0); break;
            case STOR_PRI: if (!wr32(p0, pri)) return bad(Err::MemAccess); break;
            case STOR_ALT: if (!wr32(p0, alt)) return bad(Err::MemAccess); break;
            case STOR_S_PRI: if (!wr32(std::int64_t{frm} + p0, pri)) return bad(Err::MemAccess); break;
            case STOR_S_ALT: if (!wr32(std::int64_t{frm} + p0, alt)) return bad(Err::MemAccess); break;
            case SREF_PRI: if (!rd32(p0, t) || !wr32(t, pri)) return bad(Err::MemAccess); break;
            case SREF_ALT: if (!rd32(p0, t) || !wr32(t, alt)) return bad(Err::MemAccess); break;
            case SREF_S_PRI: if (!rd32(std::int64_t{frm} + p0, t) || !wr32(t, pri)) return bad(Err::MemAccess); break;
            case SREF_S_ALT: if (!rd32(std::int64_t{frm} + p0, t) || !wr32(t, alt)) return bad(Err::MemAccess); break;
            case STOR_I: if (!wr32(alt, pri)) return bad(Err::MemAccess); break;
            case STRB_I: {
                if (p0 != 1 && p0 != 2 && p0 != 4) return bad(Err::InvInstr);
                stk_ = stk; hea_ = hea;
                if (!valid(alt, p0)) return bad(Err::MemAccess);
                std::memcpy(&mem_[static_cast<std::size_t>(alt)], &pri, static_cast<std::size_t>(p0));
                break;
            }
            case LIDX: if (!rd32(std::int64_t{alt} + std::int64_t{pri} * kCell, pri)) return bad(Err::MemAccess); break;
            case LIDX_B: if (!rd32(std::int64_t{alt} + (std::int64_t{pri} << (p0 & 31)), pri)) return bad(Err::MemAccess); break;
            case IDXADDR: pri = W(std::int64_t{alt} + std::int64_t{pri} * kCell); break;
            case IDXADDR_B: pri = static_cast<std::int32_t>(std::uint32_t(alt) + (std::uint32_t(pri) << (p0 & 31))); break;
            case ALIGN_PRI: if (static_cast<std::uint32_t>(p0) < kCell) pri ^= kCell - p0; break;
            case ALIGN_ALT: if (static_cast<std::uint32_t>(p0) < kCell) alt ^= kCell - p0; break;
            case LCTRL:
                switch (p0) {
                    case 0: pri = prog_.header().cod; break;
                    case 1: pri = prog_.header().dat; break;
                    case 2: pri = hea; break;
                    case 3: pri = stp; break;
                    case 4: pri = stk; break;
                    case 5: pri = frm; break;
                    case 6: pri = next * kCell; break;
                    default: break;
                }
                break;
            case SCTRL:
                switch (p0) {
                    case 2: if (pri < hea0 || pri > stk) return bad(Err::InvInstr); hea = pri; break;
                    case 4: if (pri < hea || pri > stp) return bad(Err::InvInstr); stk = pri; break;
                    case 5: frm = pri; break;
                    case 6: if (!jump_to(pri, next)) return bad(Err::InvInstr); break;
                    default: break;
                }
                break;
            case MOVE_PRI: pri = alt; break;
            case MOVE_ALT: alt = pri; break;
            case XCHG: std::swap(pri, alt); break;
            case PUSH_PRI: if (!push(pri)) return bad(Err::StackErr); break;
            case PUSH_ALT: if (!push(alt)) return bad(Err::StackErr); break;
            case PUSH_R: if (p0 < 0 || p0 > 65536) return bad(Err::InvInstr); for (std::int32_t i = 0; i < p0; ++i) if (!push(pri)) return bad(Err::StackErr); break;
            case PUSH_C: if (!push(p0)) return bad(Err::StackErr); break;
            case PUSH: if (!rd32(p0, t)) return bad(Err::MemAccess); if (!push(t)) return bad(Err::StackErr); break;
            case PUSH_S: if (!rd32(std::int64_t{frm} + p0, t)) return bad(Err::MemAccess); if (!push(t)) return bad(Err::StackErr); break;
            case POP_PRI: if (!pop(pri)) return bad(Err::StackLow); break;
            case POP_ALT: if (!pop(alt)) return bad(Err::StackLow); break;
            case STACK: {
                const std::int64_t ns = std::int64_t{stk} + p0;
                if (ns > stp) return bad(Err::StackLow);
                if (ns < hea) return bad(Err::StackErr);
                alt = stk;
                stk = static_cast<std::int32_t>(ns);
                break;
            }
            case HEAP: {
                const std::int64_t nh = std::int64_t{hea} + p0;
                if (nh < hea0) return bad(Err::HeapLow);
                if (nh > stk) return bad(Err::StackErr);
                alt = hea;
                hea = static_cast<std::int32_t>(nh);
                break;
            }
            case PROC: if (!push(frm)) return bad(Err::StackErr); frm = stk; break;
            case RET: {
                std::int32_t ra = 0;
                if (!pop(frm) || !pop(ra)) return bad(Err::StackLow);
                if (!jump_to(ra, next)) return bad(Err::InvInstr);
                break;
            }
            case RETN: {
                std::int32_t ra = 0, nbytes = 0;
                if (!pop(frm) || !pop(ra)) return bad(Err::StackLow);
                if (!rd32(stk, nbytes)) return bad(Err::StackLow);
                const std::int64_t ns = std::int64_t{stk} + nbytes + kCell;
                if (nbytes < 0 || ns > stp) return bad(Err::StackLow);
                stk = static_cast<std::int32_t>(ns);
                if (!jump_to(ra, next)) return bad(Err::InvInstr);
                break;
            }
            case CALL: if (!push(next * kCell)) return bad(Err::StackErr); if (!jump_to(p0, next)) return bad(Err::InvInstr); break;
            case CALL_PRI: if (!push(next * kCell)) return bad(Err::StackErr); if (!jump_to(pri, next)) return bad(Err::InvInstr); break;
            case JUMP: if (!jump_to(p0, next)) return bad(Err::InvInstr); break;
            case JREL: {                                    // relativo ao endereço do parâmetro (a confirmar em scripts reais)
                const std::int64_t target = (std::int64_t{cip} + 1) * kCell + p0;
                if (!jump_to(target, next)) return bad(Err::InvInstr);
                break;
            }
            case JZER: if (!jcond(pri == 0)) return bad(Err::InvInstr); break;
            case JNZ: if (!jcond(pri != 0)) return bad(Err::InvInstr); break;
            case JEQ: if (!jcond(pri == alt)) return bad(Err::InvInstr); break;
            case JNEQ: if (!jcond(pri != alt)) return bad(Err::InvInstr); break;
            case JLESS: if (!jcond(std::uint32_t(pri) < std::uint32_t(alt))) return bad(Err::InvInstr); break;
            case JLEQ: if (!jcond(std::uint32_t(pri) <= std::uint32_t(alt))) return bad(Err::InvInstr); break;
            case JGRTR: if (!jcond(std::uint32_t(pri) > std::uint32_t(alt))) return bad(Err::InvInstr); break;
            case JGEQ: if (!jcond(std::uint32_t(pri) >= std::uint32_t(alt))) return bad(Err::InvInstr); break;
            case JSLESS: if (!jcond(pri < alt)) return bad(Err::InvInstr); break;
            case JSLEQ: if (!jcond(pri <= alt)) return bad(Err::InvInstr); break;
            case JSGRTR: if (!jcond(pri > alt)) return bad(Err::InvInstr); break;
            case JSGEQ: if (!jcond(pri >= alt)) return bad(Err::InvInstr); break;
            case SHL: pri = static_cast<std::int32_t>(std::uint32_t(pri) << (alt & 31)); break;
            case SHR: pri = static_cast<std::int32_t>(std::uint32_t(pri) >> (alt & 31)); break;
            case SSHR: pri >>= (alt & 31); break;
            case SHL_C_PRI: pri = static_cast<std::int32_t>(std::uint32_t(pri) << (p0 & 31)); break;
            case SHL_C_ALT: alt = static_cast<std::int32_t>(std::uint32_t(alt) << (p0 & 31)); break;
            case SHR_C_PRI: pri = static_cast<std::int32_t>(std::uint32_t(pri) >> (p0 & 31)); break;
            case SHR_C_ALT: alt = static_cast<std::int32_t>(std::uint32_t(alt) >> (p0 & 31)); break;
            case SMUL: pri = static_cast<std::int32_t>(std::uint32_t(pri) * std::uint32_t(alt)); break;
            case SDIV: {                                    // PRI = PRI / ALT (arredondado para baixo), ALT = resto
                if (alt == 0) return bad(Err::Divide);
                std::int32_t q, r;
                floor_div(pri, alt, q, r);
                pri = q; alt = r;
                break;
            }
            case SDIV_ALT: {                                // PRI = ALT / PRI, ALT = resto
                if (pri == 0) return bad(Err::Divide);
                std::int32_t q, r;
                floor_div(alt, pri, q, r);
                pri = q; alt = r;
                break;
            }
            case UMUL: pri = static_cast<std::int32_t>(std::uint32_t(pri) * std::uint32_t(alt)); break;
            case UDIV: {
                if (alt == 0) return bad(Err::Divide);
                const std::uint32_t a = std::uint32_t(pri), b = std::uint32_t(alt);
                pri = static_cast<std::int32_t>(a / b); alt = static_cast<std::int32_t>(a % b);
                break;
            }
            case UDIV_ALT: {
                if (pri == 0) return bad(Err::Divide);
                const std::uint32_t a = std::uint32_t(alt), b = std::uint32_t(pri);
                pri = static_cast<std::int32_t>(a / b); alt = static_cast<std::int32_t>(a % b);
                break;
            }
            case ADD: pri = static_cast<std::int32_t>(std::uint32_t(pri) + std::uint32_t(alt)); break;
            case SUB: pri = static_cast<std::int32_t>(std::uint32_t(pri) - std::uint32_t(alt)); break;
            case SUB_ALT: pri = static_cast<std::int32_t>(std::uint32_t(alt) - std::uint32_t(pri)); break;
            case AND: pri &= alt; break;
            case OR: pri |= alt; break;
            case XOR: pri ^= alt; break;
            case NOT: pri = !pri; break;
            case NEG: pri = static_cast<std::int32_t>(0u - std::uint32_t(pri)); break;
            case INVERT: pri = ~pri; break;
            case ADD_C: pri = static_cast<std::int32_t>(std::uint32_t(pri) + std::uint32_t(p0)); break;
            case SMUL_C: pri = static_cast<std::int32_t>(std::uint32_t(pri) * std::uint32_t(p0)); break;
            case ZERO_PRI: pri = 0; break;
            case ZERO_ALT: alt = 0; break;
            case ZERO: if (!wr32(p0, 0)) return bad(Err::MemAccess); break;
            case ZERO_S: if (!wr32(std::int64_t{frm} + p0, 0)) return bad(Err::MemAccess); break;
            case SIGN_PRI: pri = static_cast<std::int8_t>(pri); break;
            case SIGN_ALT: alt = static_cast<std::int8_t>(alt); break;
            case EQ: pri = pri == alt; break;
            case NEQ: pri = pri != alt; break;
            case LESS: pri = std::uint32_t(pri) < std::uint32_t(alt); break;
            case LEQ: pri = std::uint32_t(pri) <= std::uint32_t(alt); break;
            case GRTR: pri = std::uint32_t(pri) > std::uint32_t(alt); break;
            case GEQ: pri = std::uint32_t(pri) >= std::uint32_t(alt); break;
            case SLESS: pri = pri < alt; break;
            case SLEQ: pri = pri <= alt; break;
            case SGRTR: pri = pri > alt; break;
            case SGEQ: pri = pri >= alt; break;
            case EQ_C_PRI: pri = pri == p0; break;
            case EQ_C_ALT: pri = alt == p0; break;
            case INC_PRI: pri = W(std::int64_t{pri} + 1); break;
            case INC_ALT: alt = W(std::int64_t{alt} + 1); break;
            case INC: if (!rd32(p0, t) || !wr32(p0, W(std::int64_t{t} + 1))) return bad(Err::MemAccess); break;
            case INC_S: if (!rd32(std::int64_t{frm} + p0, t) || !wr32(std::int64_t{frm} + p0, W(std::int64_t{t} + 1))) return bad(Err::MemAccess); break;
            case INC_I: if (!rd32(pri, t) || !wr32(pri, W(std::int64_t{t} + 1))) return bad(Err::MemAccess); break;
            case DEC_PRI: pri = W(std::int64_t{pri} - 1); break;
            case DEC_ALT: alt = W(std::int64_t{alt} - 1); break;
            case DEC: if (!rd32(p0, t) || !wr32(p0, W(std::int64_t{t} - 1))) return bad(Err::MemAccess); break;
            case DEC_S: if (!rd32(std::int64_t{frm} + p0, t) || !wr32(std::int64_t{frm} + p0, W(std::int64_t{t} - 1))) return bad(Err::MemAccess); break;
            case DEC_I: if (!rd32(pri, t) || !wr32(pri, W(std::int64_t{t} - 1))) return bad(Err::MemAccess); break;
            case MOVS: {
                stk_ = stk; hea_ = hea;
                if (p0 < 0 || !valid(pri, p0) || !valid(alt, p0)) return bad(Err::MemAccess);
                std::memmove(&mem_[static_cast<std::size_t>(alt)], &mem_[static_cast<std::size_t>(pri)], static_cast<std::size_t>(p0));
                break;
            }
            case CMPS: {                                    // PRI = comparacao do bloco em ALT com o bloco em PRI
                stk_ = stk; hea_ = hea;
                if (p0 < 0 || !valid(pri, p0) || !valid(alt, p0)) return bad(Err::MemAccess);
                const int c = std::memcmp(&mem_[static_cast<std::size_t>(alt)], &mem_[static_cast<std::size_t>(pri)], static_cast<std::size_t>(p0));
                pri = c;
                break;
            }
            case FILL: {
                stk_ = stk; hea_ = hea;
                if (p0 < 0 || !valid(alt, p0)) return bad(Err::MemAccess);
                for (std::int32_t i = 0; i + kCell <= p0; i += kCell) std::memcpy(&mem_[static_cast<std::size_t>(alt) + static_cast<std::size_t>(i)], &pri, kCell);
                break;
            }
            case HALT: return stop(static_cast<Err>(p0));
            case BOUNDS: if (std::uint32_t(pri) > std::uint32_t(p0)) return bad(Err::Bounds); break;
            case SYSREQ_PRI:
            case SYSREQ_C: {
                const std::int32_t index = op == SYSREQ_C ? p0 : pri;
                if (index < 0 || index >= static_cast<std::int32_t>(natives_.size()) || !natives_[static_cast<std::size_t>(index)])
                    return bad(Err::Callback);
                std::int32_t params[66] = {0};
                std::int32_t nbytes = 0;
                if (!rd32(stk, nbytes)) return bad(Err::MemAccess);
                const std::int32_t ncells = std::min<std::int32_t>(std::max<std::int32_t>(nbytes / kCell, 0), 64);
                for (std::int32_t i = 0; i <= ncells; ++i) if (!rd32(stk + i * kCell, params[i])) return bad(Err::MemAccess);
                sync();
                pending_error_ = Err::None;
                pri = natives_[static_cast<std::size_t>(index)](*this, params);
                // a nativa pode ter mexido no heap (heap_alloc) e na memória; recarrega o heap
                hea = hea_;
                if (pending_error_ != Err::None) { const Err e = pending_error_; pending_error_ = Err::None; return stop(e); }
                break;
            }
            case SYSREQ_D: return bad(Err::InvInstr);
            case FILE: case SYMBOL: case LINE: case SRANGE: case SYMTAG: case BREAK: case NOP: break;
            case JUMP_PRI: if (!jump_to(pri, next)) return bad(Err::InvInstr); break;
            case SWITCH: {
                std::int64_t tc = p0;
                if (tc < 0 || tc % kCell != 0 || tc / kCell >= ncode) return bad(Err::InvInstr);
                const std::size_t ti = static_cast<std::size_t>(tc / kCell);
                if (!prog_.is_instruction_start(ti) || code[ti] != CASETBL) return bad(Err::InvInstr);
                const std::int32_t num = code[ti + 1];
                std::int32_t target = code[ti + 2];
                for (std::int32_t i = 0; i < num; ++i) {
                    if (code[ti + 3 + 2 * static_cast<std::size_t>(i)] == pri) { target = code[ti + 4 + 2 * static_cast<std::size_t>(i)]; break; }
                }
                if (!jump_to(target, next)) return bad(Err::InvInstr);
                break;
            }
            case CASETBL: return bad(Err::InvInstr);
            case SWAP_PRI: { if (!rd32(stk, t) || !wr32(stk, pri)) return bad(Err::MemAccess); pri = t; break; }
            case SWAP_ALT: { if (!rd32(stk, t) || !wr32(stk, alt)) return bad(Err::MemAccess); alt = t; break; }
            case PUSH_ADR: if (!push(W(std::int64_t{frm} + p0))) return bad(Err::StackErr); break;
            default: return bad(Err::InvInstr);
        }
        cip = next;
    }
}

}  // namespace f3vb::amx
