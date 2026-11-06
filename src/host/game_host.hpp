// Camada de "funcoes dos scripts" (natives) para a VM AMX da F3VB.
//
// Estado da implementacao (honesto):
//  - Implementadas de verdade: biblioteca de ponto flutuante (float, floatadd, floatsub, floatmul, floatdiv, floatcmp,
//    floatround, floatfract), RandomNumber, Myself, GetGlobal/SetGlobal e as de dialogo observadas em scripts reais
//    (SetCurrentNodeName, SetNodeTextFunction, RegisterReply, FloatingSpeechText). v17: FloatingSpeechText e PCSpeaker tambem guardam a fala em speeches().
//  - TODAS as outras natives importadas pelo script sao registradas como "somente log": registram nome e argumentos
//    em events() e devolvem 0. Assim qualquer script roda ate o fim e sabemos exatamente o que falta implementar.
//  - Semantica dos argumentos e PROVISORIA (deduzida dos nomes e de scripts reais); ver docs/NATIVES.md.
#pragma once
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "../amx/amx_vm.hpp"

namespace f3vb::host {

struct Event {
    std::string native;
    std::vector<std::int32_t> args;
    std::vector<std::string> strings;       // argumentos que parecem enderecos de texto, ja decodificados
    bool implemented = false;
};

// Fala registrada por FloatingSpeechText (entidade, id do texto, cor RGB) ou PCSpeaker (entidade 0 = jogador; semantica PROVISORIA).
struct Speech {
    std::string native;
    std::int32_t entity = 0, text_id = 0;
    std::int32_t r = 0, g = 0, b = 0;
};

// v19: RegisterReply(id, texto, ACAO, DESTINO). Medido no "00 Corporal Armstrong": a funcao de texto do no (NU2_N2) ja testa as condicoes (RCn_N2)
// ANTES de registrar cada resposta; o 3o argumento e a acao ao escolher (RAn_N2, ex. "ZERO.pri; RETN") e o 4o decide o proximo no (RLn_N2).
// Ate o v18 o 3o era tratado como "condicao" e, como devolve 0, TODAS as respostas sumiam. Os nomes antigos ficam como apelidos.
struct Reply {
    std::int32_t id = 0, text_id = 0;
    std::string action_fn, link_fn;
};

struct DialogNode {
    std::string name, text_fn;
    std::vector<Reply> replies;
};

class GameHost {
public:
    explicit GameHost(f3vb::amx::Machine& m, std::uint32_t seed = 1) : vm_(m), rng_(seed) {}

    // Registra todas as natives importadas pelo programa (implementadas ou "somente log").
    void install();

    const std::vector<Event>& events() const { return events_; }
    void clear_events() { events_.clear(); }
    const std::vector<DialogNode>& nodes() const { return nodes_; }
    std::vector<DialogNode>& nodes_rw() { return nodes_; }              // v17: a sessao de dialogo reconstroi as respostas a cada passo
    void reset_dialog() { nodes_.clear(); }
    const std::vector<Speech>& speeches() const { return speeches_; }
    void clear_speeches() { speeches_.clear(); }
    std::map<std::string, int> unimplemented_calls() const;

    std::int32_t self_id = 1000;
    std::map<std::int32_t, std::int32_t> globals;

    static float as_float(std::int32_t c);
    static std::int32_t from_float(float f);

private:
    void log(const std::string& name, const std::int32_t* p, bool implemented, bool decode_strings = false);
    DialogNode& current_node();

    f3vb::amx::Machine& vm_;
    std::mt19937 rng_;
    std::vector<Event> events_;
    std::vector<DialogNode> nodes_;
    std::vector<Speech> speeches_;
};

}  // namespace f3vb::host
