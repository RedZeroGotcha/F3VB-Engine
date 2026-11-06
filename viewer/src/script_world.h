// Scripts do jogo ligados ao mapa (v17): biblioteca de .amx, instancia (Program + Machine + GameHost), sessao de dialogo e levantamento.
// Portavel: sem Windows nem OpenGL (o F3VBMapViewer so desenha o resultado).
//
// SEMANTICA PROVISORIA (observada em scripts reais, docs/DIALOGOS.md; sera refinada com os logs do usuario):
//   - Iniciar dialogo = chamar a public "OnStartDialogue" (sem argumentos).
//   - O no atual e o ultimo criado por SetCurrentNodeName; sua "funcao de texto" (SetNodeTextFunction) e chamada para (re)registrar as respostas
//     (RegisterReply). "EndDialogue" (ou vazio) como funcao de texto encerra o dialogo.
//   - v19 (medido no '00 Corporal Armstrong'): a funcao de texto ja testa as condicoes (RCn) antes de registrar cada resposta; toda resposta registrada vale.
//   - Escolher uma resposta = chamar a ACAO (3o arg do RegisterReply) e depois o DESTINO (4o arg); entao a funcao de texto do no atual roda de novo.
//   - RegisterNodeText(id) = fala do NPC no no. Ids de texto 1-based no .stf (id N = entrada N-1).
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../src/amx/amx_vm.hpp"
#include "../../src/host/game_host.hpp"
#include "f3map.h"
#include "stf.h"

namespace f3v {

// Biblioteca: pasta com os .amx convertidos (CONVERTED/scripts). Nomes comparados sem pasta, sem ".amx", sem maiusculas e com os caracteres
// proibidos do Windows trocados por '_' (igual a safe_name do extrator).
class ScriptLibrary {
public:
    std::size_t scan(const std::string& dir);
    // v20: biblioteca vinda da demo original (Vfs). path = qualquer chave que o leitor entenda (ex.: "vfs:CaptainArmstrong").
    void clear() { by_key_.clear(); shown_.clear(); dlg_cache_.clear(); dlg_cached_ = false; }
    void add(const std::string& name, const std::string& path);
    std::function<bool(const std::string& path, std::vector<std::uint8_t>& out)> reader;   // se vazio, le do disco
    bool read(const std::string& path, std::vector<std::uint8_t>& out) const;
    std::string find(const std::string& script_name) const;      // caminho do .amx ou ""
    std::size_t size() const { return by_key_.size(); }
    const std::string& dir() const { return dir_; }
    std::vector<std::string> names() const;
    static std::string normalize(const std::string& script_name);

    // v18: scripts que tem a public OnStartDialogue (os de NPC falante, ex. "10 Mormon"). Carrega cada .amx uma vez (cache).
    // O script de COMPORTAMENTO citado pela entidade do mapa (CaptainArmstrong.amx, Template_*...) normalmente NAO tem dialogo: o dialogo mora em outro script.
    struct Named { std::string name, path; };
    const std::vector<Named>& dialogue_scripts() const;
    // Candidatos de dialogo para uma entidade, por nome: palavras do nome do script (>= 4 letras) que aparecem no texto da entidade (prototipo, instancia, nome).
    // Heuristica PROVISORIA (nao sabemos ainda como a engine liga a entidade ao script de dialogo).
    std::vector<Named> match_dialogue(const std::string& entity_text) const;

private:
    mutable std::vector<Named> dlg_cache_;
    mutable bool dlg_cached_ = false;
    std::string dir_;
    std::map<std::string, std::string> by_key_;     // chave normalizada -> caminho
    std::map<std::string, std::string> shown_;      // chave -> nome do arquivo sem extensao
};

struct ScriptInstance {
    std::string name, path;
    std::unique_ptr<f3vb::amx::Program> prog;
    std::unique_ptr<f3vb::amx::Machine> vm;
    std::unique_ptr<f3vb::host::GameHost> host;
    static std::unique_ptr<ScriptInstance> from_bytes(const std::vector<std::uint8_t>& bytes, const std::string& name, std::int32_t self_id, std::uint32_t seed, std::string& err);
    static std::unique_ptr<ScriptInstance> from_file(const std::string& path, const std::string& name, std::int32_t self_id, std::uint32_t seed, std::string& err);
};

// text: sem os codigos de cor {cNNN}; color: COLORREF do ultimo {cNNN} (0x00BBGGRR; -1 = nenhum). Ids de texto 1-BASED no .stf (v19).
struct DialogLine { std::string who; std::int32_t text_id = 0; std::string text; std::int32_t color = -1; };
struct DialogChoice { std::int32_t reply_id = 0, text_id = 0; std::string text, action_fn, link_fn; std::int32_t color = -1; };
// Tira os codigos de cor "{c255}" do texto do .stf e devolve a ultima cor (COLORREF) em `color` (-1 se nao ha).
std::string strip_color_codes(const std::string& s, std::int32_t& color);

class DialogSession {
public:
    DialogSession(ScriptInstance& s, const Stf* stf) : s_(s), stf_(stf) {}
    bool start();                                    // false se nao deu para iniciar (ver error())
    bool choose(std::size_t i);                      // i = indice em choices() (so as disponiveis)
    bool finished() const { return finished_; }
    const std::vector<DialogLine>& lines() const { return lines_; }
    const std::vector<DialogChoice>& choices() const { return choices_; }
    std::size_t hidden_choices() const { return hidden_; }     // respostas cuja condicao deu 0
    const std::string& error() const { return error_; }
    std::string node_name() const;
    std::vector<std::string> trace;                  // passos executados (para o log)

private:
    bool call(const std::string& fn, std::int32_t* ret);
    void absorb();
    void refresh();
    ScriptInstance& s_;
    const Stf* stf_;
    std::vector<DialogLine> lines_;
    std::vector<DialogChoice> choices_;
    std::size_t hidden_ = 0;
    bool finished_ = false;
    std::string error_;
};

struct ScriptSurvey {
    std::string name, error;
    bool loaded = false, has_dialogue = false;
    std::size_t publics = 0, natives = 0, ran_ok = 0, ran_fail = 0;
    std::map<std::string, int> unimplemented;        // natives so-log -> chamadas
    std::map<std::string, int> called;               // TODAS as natives chamadas -> chamadas (v18: mostra o que cada script realmente usa)
    std::map<std::string, std::vector<std::string>> samples;   // ate 2 exemplos de argumentos por native sem implementar: "(1, 2, \"texto\")"
    std::vector<std::string> failures;               // "public: erro"
    std::vector<std::string> unresolved;             // natives que o script importa e nao foram registradas (deve ficar vazio)
};
// Roda TODAS as publics (com 8 argumentos zero, limite de instrucoes) como o dialog_dump e resume. Os zeros evitam falsos erros de "acesso invalido":
// quase todas as publics de objetos/criaturas leem parametros (currentGameTime, atacante...).
ScriptSurvey survey_script(ScriptInstance& s);

struct ScriptRef { std::string name; std::size_t entities = 0; std::string path; };   // path vazio = .amx nao encontrado
// Scripts citados pelas entidades do mapa (distintos pelo nome normalizado), em ordem alfabetica.
std::vector<ScriptRef> map_scripts(const MapData& m, const ScriptLibrary& lib);

}  // namespace f3v
