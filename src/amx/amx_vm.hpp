// Máquina abstrata Pawn/Small, instruction set CLÁSSICO (AMX file_version 6, células de 32 bits).
//
// Por que existe: o Pawn 4.x recusa estes arquivos (exige file_version >= 11 e mudou o conjunto de
// instruções na versão 4.0). Esta VM roda os scripts de 2003 da demo do Van Buren como estão.
// A tabela de opcodes foi VALIDADA decodificando por completo o código dos scripts reais (ver
// docs/ACHADOS_DEMO.md). Referência de semântica: Pawn 4.1 (Apache-2.0, CompuPhase), ver NOTICE-PAWN.md.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace f3vb::amx {

enum class Err : int {
    None = 0, Exit = 1, Assert = 2, StackErr = 3, Bounds = 4, MemAccess = 5, InvInstr = 6, StackLow = 7,
    HeapLow = 8, Callback = 9, Native = 10, Divide = 11, Sleep = 12, InvState = 13,
    Memory = 16, Format = 17, Version = 18,
    NotFound = 19, Params = 20, Limit = 21
};
const char* err_name(Err e);

struct Header {
    std::int32_t size = 0;
    std::uint16_t magic = 0;
    std::int8_t file_version = 0, amx_version = 0;
    std::int16_t flags = 0, defsize = 0;
    std::int32_t cod = 0, dat = 0, hea = 0, stp = 0, cip = -1;
    std::int32_t publics = 0, natives = 0, libraries = 0, pubvars = 0, tags = 0, nametable = 0;   // nametable: so a partir da versao 7
};

struct FuncStub {
    std::uint32_t address = 0;
    std::string name;
};

class Program {
public:
    // Carrega e valida (cabeçalho, tabelas, e a decodificação completa do código). Em erro devolve nullopt.
    static std::optional<Program> load(std::span<const std::uint8_t> bytes, std::string* error = nullptr);

    const Header& header() const { return hdr_; }
    const std::vector<std::int32_t>& code() const { return code_; }
    const std::vector<std::uint8_t>& initial_data() const { return data_; }
    const std::vector<FuncStub>& publics() const { return publics_; }
    const std::vector<FuncStub>& natives() const { return natives_; }
    bool is_instruction_start(std::size_t cell) const { return cell < starts_.size() && starts_[cell]; }
    int find_public(std::string_view name) const;
    int find_native(std::string_view name) const;
    std::int32_t memory_size() const { return hdr_.stp - hdr_.dat; }   // dados + heap + pilha
    std::int32_t heap_start() const { return hdr_.hea - hdr_.dat; }

private:
    Header hdr_{};
    std::vector<std::int32_t> code_;
    std::vector<std::uint8_t> data_;
    std::vector<FuncStub> publics_, natives_;
    std::vector<bool> starts_;
};

class Machine;
// params[0] = bytes de argumentos; params[1..] = argumentos (cópia: seguro para ler/escrever).
using NativeFn = std::function<std::int32_t(Machine&, const std::int32_t* params)>;

class Machine {
public:
    explicit Machine(const Program& program);

    // Associa uma função nativa pelo nome (devolve false se o script não importa esse nome).
    bool register_native(std::string_view name, NativeFn fn);
    std::vector<std::string> unresolved_natives() const;

    // Executa uma função pública. Argumentos na ordem de declaração.
    Err call(int public_index, std::span<const std::int32_t> args, std::int32_t* result = nullptr);
    Err call(std::string_view name, std::span<const std::int32_t> args, std::int32_t* result = nullptr);

    // Acesso à memória do script (para as nativas). Endereços são relativos ao início dos dados.
    std::optional<std::int32_t> read_cell(std::int32_t addr) const;
    bool write_cell(std::int32_t addr, std::int32_t value);
    // Strings Pawn: desempacotadas (1 caractere por célula) ou empacotadas (4 por célula, o primeiro no byte alto).
    std::string read_string(std::int32_t addr, std::size_t max_len = 1024) const;
    bool write_string(std::int32_t addr, std::string_view text, std::size_t max_cells);
    // Reserva espaço no heap do script (para as nativas devolverem dados); devolve o endereço ou nullopt.
    std::optional<std::int32_t> heap_alloc(std::int32_t cells);

    // Uma nativa chama isto para abortar a execução com um erro.
    void abort(Err e) { pending_error_ = e; }

    void set_instruction_limit(std::uint64_t n) { limit_ = n; }   // por chamada a call()
    std::uint64_t instructions_executed() const { return executed_; }
    const Program& program() const { return prog_; }
    // Estatística: quantas vezes cada opcode foi executado (índice = opcode).
    const std::vector<std::uint64_t>& opcode_counts() const { return op_counts_; }

private:
    bool valid(std::int64_t addr, std::int64_t len) const;
    Err run(std::int32_t start_cip, std::int32_t* result);

    const Program& prog_;
    std::vector<std::uint8_t> mem_;
    std::vector<NativeFn> natives_;
    std::int32_t pri_ = 0, alt_ = 0, frm_ = 0, stk_ = 0, hea_ = 0;
    Err pending_error_ = Err::None;
    std::uint64_t limit_ = 50'000'000, executed_ = 0, call_start_ = 0;   // limite por chamada
    std::vector<std::uint64_t> op_counts_;
};

// Tabela de opcodes (nome e número de parâmetros; -1 = FILE/SYMBOL de tamanho variável; -2 = CASETBL).
struct OpInfo { const char* name; int params; };
const std::vector<OpInfo>& opcode_table();

}  // namespace f3vb::amx
