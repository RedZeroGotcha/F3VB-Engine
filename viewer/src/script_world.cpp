#include "script_world.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace f3v {

namespace fs = std::filesystem;

// ------------------------------------------------------------------------------------------------ biblioteca
std::string ScriptLibrary::normalize(const std::string& s) {
    std::size_t b = s.find_last_of("/\\");
    std::string t = b == std::string::npos ? s : s.substr(b + 1);
    for (char& c : t) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        else if (c == '<' || c == '>' || c == ':' || c == '"' || c == '|' || c == '?' || c == '*' || (static_cast<unsigned char>(c) < 32)) c = '_';
    }
    while (!t.empty() && t.back() == ' ') t.pop_back();
    if (t.size() >= 4 && t.compare(t.size() - 4, 4, ".amx") == 0) t.erase(t.size() - 4);
    while (!t.empty() && (t.back() == ' ' || t.back() == '.')) t.pop_back();
    while (!t.empty() && t.front() == ' ') t.erase(t.begin());
    return t;
}

std::size_t ScriptLibrary::scan(const std::string& dir) {
    dir_ = dir;
    by_key_.clear();
    shown_.clear();
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return 0;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string fn = it->path().filename().string();
        std::string low = fn;
        for (char& c : low) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (low.size() < 4 || low.compare(low.size() - 4, 4, ".amx") != 0) continue;
        const std::string key = normalize(fn);
        by_key_[key] = it->path().string();
        shown_[key] = fn.substr(0, fn.size() - 4);
    }
    return by_key_.size();
}

void ScriptLibrary::add(const std::string& name, const std::string& path) {
    const std::string key = normalize(name);
    by_key_[key] = path;
    shown_[key] = name;
    dlg_cached_ = false;
}

bool ScriptLibrary::read(const std::string& path, std::vector<std::uint8_t>& out) const {
    if (reader) return reader(path, out);
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

std::string ScriptLibrary::find(const std::string& script_name) const {
    const auto it = by_key_.find(normalize(script_name));
    return it == by_key_.end() ? std::string() : it->second;
}

std::vector<std::string> ScriptLibrary::names() const {
    std::vector<std::string> v;
    for (const auto& kv : shown_) v.push_back(kv.second);
    return v;
}

const std::vector<ScriptLibrary::Named>& ScriptLibrary::dialogue_scripts() const {
    if (dlg_cached_) return dlg_cache_;
    dlg_cached_ = true;
    dlg_cache_.clear();
    for (const auto& kv : by_key_) {
        std::vector<std::uint8_t> bytes;
        if (!read(kv.second, bytes)) continue;
        std::string err;
        auto prog = f3vb::amx::Program::load(bytes, &err);
        if (prog && prog->find_public("OnStartDialogue") >= 0) {
            const auto it = shown_.find(kv.first);
            dlg_cache_.push_back(Named{it == shown_.end() ? kv.first : it->second, kv.second});
        }
    }
    return dlg_cache_;
}

static std::string lower_ascii(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; }

std::vector<ScriptLibrary::Named> ScriptLibrary::match_dialogue(const std::string& entity_text) const {
    const std::string hay = lower_ascii(entity_text);
    std::vector<std::pair<int, Named>> scored;
    for (const Named& n : dialogue_scripts()) {
        int score = 0;
        std::string word;
        auto flush = [&]() { if (word.size() >= 4 && hay.find(word) != std::string::npos) score += static_cast<int>(word.size()); word.clear(); };
        for (char c : lower_ascii(n.name)) { if (c >= 'a' && c <= 'z') word.push_back(c); else flush(); }
        flush();
        if (score > 0) scored.push_back({score, n});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const std::pair<int, Named>& a, const std::pair<int, Named>& b) { return a.first > b.first; });
    std::vector<Named> out;
    for (auto& s : scored) out.push_back(s.second);
    return out;
}

// ------------------------------------------------------------------------------------------------ instancia
std::unique_ptr<ScriptInstance> ScriptInstance::from_bytes(const std::vector<std::uint8_t>& bytes, const std::string& name, std::int32_t self_id, std::uint32_t seed, std::string& err) {
    std::unique_ptr<ScriptInstance> si(new ScriptInstance());
    si->name = name;
    auto prog = f3vb::amx::Program::load(bytes, &err);
    if (!prog) return nullptr;
    si->prog.reset(new f3vb::amx::Program(std::move(*prog)));
    si->vm.reset(new f3vb::amx::Machine(*si->prog));
    si->host.reset(new f3vb::host::GameHost(*si->vm, seed));
    si->host->self_id = self_id;
    si->host->install();
    return si;
}

std::unique_ptr<ScriptInstance> ScriptInstance::from_file(const std::string& path, const std::string& name, std::int32_t self_id, std::uint32_t seed, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "nao consegui abrir " + path; return nullptr; }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto si = from_bytes(bytes, name, self_id, seed, err);
    if (si) si->path = path;
    return si;
}

// ------------------------------------------------------------------------------------------------ dialogo
std::string DialogSession::node_name() const { return s_.host->nodes().empty() ? std::string() : s_.host->nodes().back().name; }

std::string strip_color_codes(const std::string& s, std::int32_t& color) {
    std::string out;
    color = -1;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '{' && i + 2 < s.size() && (s[i + 1] == 'c' || s[i + 1] == 'C')) {
            const std::size_t e = s.find('}', i);
            if (e != std::string::npos && e > i + 2) {
                bool digits = true;
                for (std::size_t k = i + 2; k < e; ++k) digits = digits && s[k] >= '0' && s[k] <= '9';
                if (digits && e - i - 2 <= 10) { color = static_cast<std::int32_t>(std::stoll(s.substr(i + 2, e - i - 2)) & 0x00FFFFFF); i = e; continue; }
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

namespace {
// v19: ids 1-based (id N = entrada N-1 do .stf); devolve o texto sem os codigos de cor
std::string stf_text_1based(const Stf* stf, std::int32_t id, std::int32_t& color) {
    color = -1;
    if (!stf || !stf->loaded() || id < 1 || !stf->has(static_cast<std::uint32_t>(id - 1))) return std::string();
    return strip_color_codes(stf->text(static_cast<std::uint32_t>(id - 1)), color);
}
}  // namespace

void DialogSession::absorb() {
    for (const f3vb::host::Speech& sp : s_.host->speeches()) {
        DialogLine l;
        l.text_id = sp.text_id;
        l.who = sp.native == "PCSpeaker" ? "Jogador" : (sp.entity == s_.host->self_id ? "NPC" : "Entidade " + std::to_string(sp.entity));
        l.text = stf_text_1based(stf_, sp.text_id, l.color);
        lines_.push_back(std::move(l));
    }
    s_.host->clear_speeches();
}

bool DialogSession::call(const std::string& fn, std::int32_t* ret) {
    if (fn.empty()) return false;
    if (s_.prog->find_public(fn) < 0) { trace.push_back("funcao '" + fn + "' nao existe no script"); return false; }
    std::int32_t r = 0;
    s_.vm->set_instruction_limit(2'000'000);
    const f3vb::amx::Err e = s_.vm->call(fn, {}, &r);
    trace.push_back("chamou " + fn + " -> " + f3vb::amx::err_name(e) + " (ret " + std::to_string(r) + ")");
    if (ret) *ret = r;
    absorb();
    if (e != f3vb::amx::Err::None) { error_ = fn + ": " + f3vb::amx::err_name(e); return false; }
    return true;
}

void DialogSession::refresh() {
    for (int guard = 0; guard < 64; ++guard) {
        choices_.clear();
        hidden_ = 0;
        auto& nodes = s_.host->nodes_rw();
        if (nodes.empty()) { trace.push_back("nenhum no de dialogo criado: fim"); finished_ = true; return; }
        const std::string fn = nodes.back().text_fn;
        if (fn.empty() || fn == "EndDialogue") { trace.push_back("funcao de texto '" + fn + "': fim do dialogo"); finished_ = true; return; }
        const std::size_t nodes_before = nodes.size();
        nodes.back().replies.clear();
        if (!call(fn, nullptr)) { finished_ = true; return; }
        auto& after = s_.host->nodes_rw();
        if (after.size() != nodes_before || after.back().text_fn != fn) continue;           // a funcao trocou de no ou de funcao: reavalia
        for (const f3vb::host::Reply& r : after.back().replies) {             // v19: as condicoes ja foram testadas pela funcao de texto (RCn)
            DialogChoice c;
            c.reply_id = r.id; c.text_id = r.text_id; c.action_fn = r.action_fn; c.link_fn = r.link_fn;
            c.text = stf_text_1based(stf_, r.text_id, c.color);
            choices_.push_back(std::move(c));
        }
        if (choices_.empty()) { trace.push_back("sem respostas disponiveis: fim do dialogo"); finished_ = true; }
        return;
    }
    error_ = "laco de nos de dialogo (64 passos sem estabilizar)";
    finished_ = true;
}

bool DialogSession::start() {
    lines_.clear(); choices_.clear(); trace.clear(); error_.clear(); finished_ = false; hidden_ = 0;
    s_.host->reset_dialog();
    s_.host->clear_events();
    s_.host->clear_speeches();
    if (s_.prog->find_public("OnStartDialogue") < 0) { error_ = "o script nao tem a public OnStartDialogue"; finished_ = true; return false; }
    if (!call("OnStartDialogue", nullptr)) { finished_ = true; return false; }
    refresh();
    return true;
}

bool DialogSession::choose(std::size_t i) {
    if (finished_ || i >= choices_.size()) return false;
    const DialogChoice c = choices_[i];
    DialogLine l;
    l.who = "Jogador"; l.text_id = c.text_id; l.text = c.text; l.color = c.color;
    lines_.push_back(l);
    const std::size_t nodes_before = s_.host->nodes().size();
    const std::string fn_before = nodes_before ? s_.host->nodes().back().text_fn : std::string();
    if (!c.action_fn.empty()) call(c.action_fn, nullptr);                     // acao (RAn)
    std::int32_t link = 0;
    if (error_.empty() && !c.link_fn.empty()) call(c.link_fn, &link);          // destino (RLn): troca de no / funcao de texto, ou EndDialogue
    if (!error_.empty()) { finished_ = true; return true; }
    // destino que nao criou no nem trocou a funcao de texto: o dialogo acaba (senao a mesma pergunta voltaria para sempre)
    if (s_.host->nodes().size() == nodes_before && (nodes_before == 0 || s_.host->nodes().back().text_fn == fn_before) && !c.link_fn.empty()) {
        trace.push_back("destino " + c.link_fn + " nao mudou de no (ret " + std::to_string(link) + "): fim do dialogo");
        finished_ = true;
        return true;
    }
    if (!error_.empty()) { finished_ = true; return true; }
    refresh();
    return true;
}

// ------------------------------------------------------------------------------------------------ levantamento
ScriptSurvey survey_script(ScriptInstance& s) {
    ScriptSurvey r;
    r.name = s.name;
    r.loaded = true;
    r.publics = s.prog->publics().size();
    r.natives = s.prog->natives().size();
    r.has_dialogue = s.prog->find_public("OnStartDialogue") >= 0;
    r.unresolved = s.vm->unresolved_natives();
    s.vm->set_instruction_limit(2'000'000);
    s.host->clear_events();
    const std::int32_t zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (std::size_t i = 0; i < s.prog->publics().size(); ++i) {
        std::int32_t ret = 0;
        const f3vb::amx::Err e = s.vm->call(static_cast<int>(i), std::span<const std::int32_t>(zeros, 8), &ret);
        if (e == f3vb::amx::Err::None) ++r.ran_ok;
        else { ++r.ran_fail; r.failures.push_back(s.prog->publics()[i].name + ": " + f3vb::amx::err_name(e)); }
    }
    r.unimplemented = s.host->unimplemented_calls();
    for (const f3vb::host::Event& ev : s.host->events()) {
        ++r.called[ev.native];
        std::vector<std::string>& smp = r.samples[ev.native];
        if (ev.implemented || smp.size() >= 2) continue;
        std::string t = "(";
        for (std::size_t k = 0; k < ev.args.size(); ++k) {
            t += (k ? ", " : "") + std::to_string(ev.args[k]);
            if (k < ev.strings.size() && !ev.strings[k].empty()) t += " \"" + ev.strings[k] + "\"";
        }
        smp.push_back(t + ")");
    }
    return r;
}

std::vector<ScriptRef> map_scripts(const MapData& m, const ScriptLibrary& lib) {
    std::map<std::string, ScriptRef> by;
    for (const MapEntity& e : m.entities) {
        if (e.script.empty()) continue;
        const std::string key = ScriptLibrary::normalize(e.script);
        if (key.empty()) continue;
        ScriptRef& r = by[key];
        if (r.name.empty()) { r.name = e.script; r.path = lib.find(e.script); }
        ++r.entities;
    }
    std::vector<ScriptRef> out;
    for (auto& kv : by) out.push_back(std::move(kv.second));
    return out;
}

}  // namespace f3v
