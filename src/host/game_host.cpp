#include "game_host.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace f3vb::host {

using f3vb::amx::Machine;

float GameHost::as_float(std::int32_t c) {
    float f;
    std::memcpy(&f, &c, sizeof f);
    return f;
}
std::int32_t GameHost::from_float(float f) {
    std::int32_t c;
    std::memcpy(&c, &f, sizeof c);
    return c;
}

static std::int32_t arg(const std::int32_t* p, int i) {          // argumento i (1-based); 0 se ausente
    return i <= p[0] / 4 ? p[i] : 0;
}

void GameHost::log(const std::string& name, const std::int32_t* p, bool implemented, bool decode_strings) {
    Event e;
    e.native = name;
    e.implemented = implemented;
    const int n = std::min(p[0] / 4, 16);
    for (int i = 1; i <= n; ++i) {
        e.args.push_back(p[i]);
        if (decode_strings) {
            std::string s;
            if (p[i] >= 4 && p[i] % 4 == 0) s = vm_.read_string(p[i], 64);       // endereco de texto plausivel
            const bool printable = s.size() >= 2 && std::all_of(s.begin(), s.end(), [](unsigned char c) { return c >= 32 && c < 127; });
            e.strings.push_back(printable ? s : std::string());
        }
    }
    events_.push_back(std::move(e));
}

DialogNode& GameHost::current_node() {
    if (nodes_.empty()) nodes_.push_back(DialogNode{"(sem nome)", "", {}});
    return nodes_.back();
}

std::map<std::string, int> GameHost::unimplemented_calls() const {
    std::map<std::string, int> out;
    for (const auto& e : events_) if (!e.implemented) ++out[e.native];
    return out;
}

void GameHost::install() {
    // ---- implementadas: ponto flutuante (semantica do float.inc do Pawn) ----
    auto bin = [&](const char* name, float (*f)(float, float)) {
        vm_.register_native(name, [f](Machine&, const std::int32_t* p) {
            return from_float(f(as_float(arg(p, 1)), as_float(arg(p, 2))));
        });
    };
    bin("floatadd", [](float a, float b) { return a + b; });
    bin("floatsub", [](float a, float b) { return a - b; });
    bin("floatmul", [](float a, float b) { return a * b; });
    bin("floatdiv", [](float a, float b) { return a / b; });
    vm_.register_native("float", [](Machine&, const std::int32_t* p) { return from_float(static_cast<float>(arg(p, 1))); });
    vm_.register_native("floatcmp", [](Machine&, const std::int32_t* p) {
        const float a = as_float(arg(p, 1)), b = as_float(arg(p, 2));
        return a == b ? 0 : (a > b ? 1 : -1);
    });
    vm_.register_native("floatround", [](Machine&, const std::int32_t* p) {
        const float v = as_float(arg(p, 1));
        switch (arg(p, 2)) {
            case 1: return static_cast<std::int32_t>(std::floor(v));
            case 2: return static_cast<std::int32_t>(std::ceil(v));
            case 3: return static_cast<std::int32_t>(std::trunc(v));
            default: return static_cast<std::int32_t>(std::floor(v + 0.5f));
        }
    });
    vm_.register_native("floatfract", [](Machine&, const std::int32_t* p) {
        const float v = as_float(arg(p, 1));
        return from_float(v - std::floor(v));
    });

    // ---- implementadas: utilitarios e estado ----
    vm_.register_native("RandomNumber", [this](Machine&, const std::int32_t* p) {
        std::int32_t lo = arg(p, 1), hi = arg(p, 2);
        if (hi < lo) std::swap(lo, hi);
        std::uniform_int_distribution<std::int32_t> d(lo, hi);
        const auto v = d(rng_);
        log("RandomNumber", p, true);
        return v;
    });
    vm_.register_native("Myself", [this](Machine&, const std::int32_t* p) { log("Myself", p, true); return self_id; });
    vm_.register_native("GetGlobal", [this](Machine&, const std::int32_t* p) {
        log("GetGlobal", p, true);
        const auto it = globals.find(arg(p, 1));
        return it == globals.end() ? 0 : it->second;
    });
    vm_.register_native("SetGlobal", [this](Machine&, const std::int32_t* p) {
        log("SetGlobal", p, true);
        globals[arg(p, 1)] = arg(p, 2);
        return 0;
    });

    // ---- implementadas: dialogo (observadas em scripts reais) ----
    vm_.register_native("SetCurrentNodeName", [this](Machine& m, const std::int32_t* p) {
        log("SetCurrentNodeName", p, true, true);
        nodes_.push_back(DialogNode{m.read_string(arg(p, 1)), "", {}});
        return 0;
    });
    vm_.register_native("SetNodeTextFunction", [this](Machine& m, const std::int32_t* p) {
        log("SetNodeTextFunction", p, true, true);
        current_node().text_fn = m.read_string(arg(p, 1));
        return 0;
    });
    vm_.register_native("RegisterReply", [this](Machine& m, const std::int32_t* p) {
        log("RegisterReply", p, true, true);
        current_node().replies.push_back(Reply{arg(p, 1), arg(p, 2), m.read_string(arg(p, 3)), m.read_string(arg(p, 4))});
        return 0;
    });
    // v19: fala do NPC no no atual (medido: "00 Corporal Armstrong" -> RegisterNodeText(850) = "How can I help you, citizen?", id 1-based no .stf)
    vm_.register_native("RegisterNodeText", [this](Machine&, const std::int32_t* p) {
        log("RegisterNodeText", p, true);
        speeches_.push_back(Speech{"RegisterNodeText", self_id, arg(p, 1), 255, 255, 255});
        return 0;
    });
    vm_.register_native("FloatingSpeechText", [this](Machine&, const std::int32_t* p) {
        log("FloatingSpeechText", p, true);
        speeches_.push_back(Speech{"FloatingSpeechText", arg(p, 1), arg(p, 2), arg(p, 4), arg(p, 5), arg(p, 6)});   // (entidade, id do texto, 0, R, G, B)
        return 0;
    });
    // PCSpeaker: fala do jogador. PROVISORIO: assume (id do texto) como 1o argumento; continua contada como "nao implementada" no relatorio.
    vm_.register_native("PCSpeaker", [this](Machine&, const std::int32_t* p) {
        log("PCSpeaker", p, false);
        speeches_.push_back(Speech{"PCSpeaker", 0, arg(p, 1), 255, 255, 255});
        return 0;
    });

    // ---- todas as demais: somente log ----
    for (const auto& n : vm_.program().natives()) {
        const std::string name = n.name;
        if (vm_.program().find_native(name) >= 0) {
            bool registered = true;
            for (const auto& u : vm_.unresolved_natives()) if (u == name) registered = false;
            if (registered) continue;
        }
        vm_.register_native(name, [this, name](Machine&, const std::int32_t* p) {
            log(name, p, false, true);                  // v17: decodifica argumentos que parecem texto (ajuda a entender natives de UI/menus)
            return 0;
        });
    }
}

}  // namespace f3vb::host
