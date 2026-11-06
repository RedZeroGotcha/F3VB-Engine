// v20: F3VBCheck - confere a leitura DIRETA da demo original (resource.rht + .grp) e o carregador de mapas em C++.
//   F3VBCheck --demo <pasta da demo>                        -> le o indice, conta recursos por tipo, le todos os lumps
//   F3VBCheck --demo <pasta> --assets <original_assets>      -> + compara byte a byte com a copia extraida (esperado: todos iguais)
//   F3VBCheck --demo <pasta> --maps [--f3map <CONVERTED/maps>] -> monta todos os mapas em memoria (e compara com os .f3map do Python)
//   F3VBCheck --demo <pasta> --tudo                         -> v21: TUDO da demo sem abrir janela: cada script na VM (todas as publics),
//                                                              cada dialogo (percorre as respostas), animacoes e chao de cada personagem
//                                                              de cada mapa, e os sons (cabecalho de cada .wav)
//   F3VBCheck --demo <pasta> --models                       -> v20: monta os personagens/objetos 3D de TODAS as entidades de todos os mapas
//                                                              (.b3d + .gr2 + prototipos, direto dos .grp) e conta quantos deram certo
// Codigo de saida 0 = tudo certo. Portavel (Windows/Linux).
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "actor.h"
#include "ground.h"
#include "script_world.h"
#include "stf.h"
#include "f3map.h"
#include "map_build.h"
#include "vfs.h"

using namespace f3v;

namespace {

std::uint64_t fnv1a(const std::vector<std::uint8_t>& d) {
    std::uint64_t h = 1469598103934665603ull;
    for (std::uint8_t b : d) { h ^= b; h *= 1099511628211ull; }
    return h;
}

int check_maps(const Vfs& v, const std::string& f3map_dir) {
    int bad = 0;
    std::size_t tris_total = 0, nostate = 0, maps = 0;
    for (const ResEntry* e : v.list(RT_MAP)) {
        MapData m;
        std::string err;
        MapBuildStats st;
        if (!build_map(v, e->name, m, err, &st)) { std::printf("  MAPA %-36s FALHOU: %s\n", e->name.c_str(), err.c_str()); ++bad; continue; }
        ++maps;
        std::size_t ns = 0;
        for (const MapTriList& tl : m.tris) if (tl.material == 0 && tl.page == 0 && tl.tex == 0) ns += tl.idx.size() / 3;
        tris_total += m.triangle_count();
        nostate += ns;
        std::string cmp;
        if (!f3map_dir.empty()) {
            MapData py;
            std::string e2;
            if (load_f3map_file(path_join(f3map_dir, e->name + ".f3map"), py, e2)) {
                const bool same = py.triangle_count() == m.triangle_count() && py.tris.size() == m.tris.size() && py.entities.size() == m.entities.size() &&
                                  py.blocks.size() == m.blocks.size() && py.textures.size() == m.textures.size() && py.nav.size() == m.nav.size() &&
                                  py.triggers.size() == m.triggers.size() && py.grid.size() == m.grid.size();
                bool idx_same = same;
                for (std::size_t i = 0; idx_same && i < m.tris.size(); ++i)
                    idx_same = m.tris[i].block == py.tris[i].block && m.tris[i].material == py.tris[i].material && m.tris[i].page == py.tris[i].page &&
                               m.tris[i].tex == py.tris[i].tex && m.tris[i].idx == py.tris[i].idx;
                cmp = idx_same ? " | igual ao .f3map do Python" : " | DIFERENTE do .f3map do Python";
                if (!idx_same) {
                    ++bad;
                    cmp += " (py: " + std::to_string(py.triangle_count()) + " tri, " + std::to_string(py.tris.size()) + " listas, " + std::to_string(py.entities.size()) + " ent)";
                }
            } else cmp = " | (sem .f3map do Python para comparar)";
        }
        std::printf("  MAPA %-36s blocos %4zu  tri %7zu  listas %4zu  ent %3zu  nos %5zu/%5zu fecham  sem_estado %zu%s\n", e->name.c_str(), m.blocks.size(),
                    m.triangle_count(), m.tris.size(), m.entities.size(), st.nodes_full, st.nodes, ns, cmp.c_str());
    }
    std::printf("mapas: %zu montados, %d com problema | triangulos %zu | sem estado %zu\n", maps, bad, tris_total, nostate);
    return bad;
}

// Entidade sem modelo desenhavel (porta sem malha, gatilho...) nao e erro: so conta. Erro = prototipo com modelo que nao montou.
int check_models(const Vfs& v) {
    std::size_t ents = 0, built = 0, nomodel = 0, parts = 0, anims = 0;
    int bad = 0;
    for (const ResEntry* e : v.list(RT_MAP)) {
        MapData m;
        std::string err;
        if (!build_map(v, e->name, m, err, nullptr)) continue;
        std::size_t ok = 0;
        for (const MapEntity& ent : m.entities) {
            ++ents;
            Actor a;
            if (build_actor(v, ent, a, err)) {
                ++built; ++ok; parts += a.parts.size();
                if (!a.clip_name.empty()) ++anims;
            } else if (err.find("sem modelo") != std::string::npos || err.find("nao encontrado") != std::string::npos) ++nomodel;
            else { ++bad; if (bad <= 15) std::printf("  %-36s %-40s FALHOU: %s\n", e->name.c_str(), ent.prototype.c_str(), err.c_str()); }
        }
        std::printf("  %-36s %3zu de %3zu entidades com modelo 3D\n", e->name.c_str(), ok, m.entities.size());
    }
    std::printf("modelos: %zu entidades, %zu montadas (%zu pecas, %zu com animacao parada), %zu sem modelo, %d falhas\n", ents, built, parts, anims, nomodel, bad);
    return bad;
}


// v21: conferencia completa (scripts + dialogos + animacoes + chao + sons). Cada linha diz OK/FALHA e o motivo.
int check_all(const Vfs& v, const std::string& demo) {
    int bad = 0;
    Stf stf;
    std::string err;
    const bool have_stf = stf.load_file(demo + "/English.stf", err) || stf.load_file(demo + "\\English.stf", err);
    std::printf("textos: English.stf %s\n", have_stf ? "ok" : ("FALHA: " + err).c_str());
    // --- scripts na VM
    std::size_t n = 0, loaded = 0, pub_ok = 0, pub_fail = 0, dlg = 0, dlg_ok = 0, dlg_lines = 0, dlg_choices = 0;
    std::map<std::string, int> missing;
    std::vector<const ResEntry*> scripts = v.list(RT_AMX);
    for (const ResEntry* e : v.list(RT_SMA)) scripts.push_back(e);         // os .sma da demo tambem sao bytecode (os dialogos estao neles)
    for (const ResEntry* e : scripts) {
        ++n;
        std::vector<std::uint8_t> bytes;
        if (!v.read(*e, bytes, err)) { ++bad; std::printf("  script %s: FALHA lendo: %s\n", e->name.c_str(), err.c_str()); continue; }
        auto si = ScriptInstance::from_bytes(bytes, e->name, 1, 1234, err);
        if (!si) { ++bad; std::printf("  script %s: FALHA na VM: %s\n", e->name.c_str(), err.c_str()); continue; }
        ++loaded;
        const ScriptSurvey sv = survey_script(*si);
        pub_ok += sv.ran_ok; pub_fail += sv.ran_fail;
        for (const auto& kv : sv.unimplemented) missing[kv.first] += kv.second;
        for (const std::string& u : sv.unresolved) { ++bad; std::printf("  script %s: native sem registro: %s\n", e->name.c_str(), u.c_str()); }
        for (const std::string& f : sv.failures) std::printf("  script %s: public com erro: %s\n", e->name.c_str(), f.c_str());
        if (!sv.has_dialogue) continue;
        ++dlg;
        auto di = ScriptInstance::from_bytes(bytes, e->name, 1, 1234, err);
        if (!di) continue;
        DialogSession d(*di, have_stf ? &stf : nullptr);
        bool ok = d.start();
        for (int step = 0; ok && step < 40 && !d.finished() && !d.choices().empty(); ++step) ok = d.choose(0);   // sempre a 1a resposta
        if (ok) ++dlg_ok; else { ++bad; std::printf("  dialogo %s: FALHA: %s\n", e->name.c_str(), d.error().c_str()); }
        dlg_lines += d.lines().size(); dlg_choices += d.choices().size();
    }
    std::printf("scripts (.amx + .sma): %zu na demo, %zu carregados na VM | publics: %zu rodaram, %zu com erro | dialogos: %zu de %zu percorridos sem erro (%zu falas)\n",
                n, loaded, pub_ok, pub_fail, dlg_ok, dlg, dlg_lines);
    std::string ms;
    for (const auto& kv : missing) ms += (ms.empty() ? "" : ", ") + kv.first + " x" + std::to_string(kv.second);
    std::printf("natives ainda so registradas (sem efeito no jogo): %s\n", ms.empty() ? "nenhuma" : ms.c_str());
    // --- personagens: animacoes e chao, mapa por mapa
    for (const ResEntry* e : v.list(RT_MAP)) {
        MapData m;
        if (!build_map(v, e->name, m, err, nullptr)) continue;
        Ground g;
        g.build(m);
        int crit = 0, anim_full = 0, no_ground = 0;
        float worst = 0;
        std::string worst_name, miss;
        for (const MapEntity& ent : m.entities) {
            Actor a;
            if (!build_actor(v, ent, a, err)) continue;
            if (ent.kind != "CRT" || a.anim_length(AN_IDLE) <= 0.1f || a.clip_index[AN_WALK] < 0) continue;
            ++crit;
            bool all = true;
            for (int r = 0; r < AN_COUNT; ++r) if (a.clip_index[r] < 0) { all = false; miss += " " + ent.prototype + ":" + actor_anim_name(r); }
            if (all) ++anim_full;
            float gy;
            if (!g.height(ent.pos[0], ent.pos[2], ent.pos[1] + 0.5f, gy)) { ++no_ground; continue; }
            if (std::fabs(ent.pos[1] - gy) > std::fabs(worst)) { worst = ent.pos[1] - gy; worst_name = ent.prototype; }
        }
        std::printf("  %-34s chao %6zu tri | %2d personagens, %2d com as 6 animacoes, %d sem chao | maior diferenca do .map ao chao %.2f (%s)%s%s\n",
                    e->name.c_str(), g.triangles(), crit, anim_full, no_ground, worst, worst_name.c_str(), miss.empty() ? "" : " | faltam:", miss.c_str());
        if (no_ground) bad += no_ground;
    }
    // --- sons
    std::size_t wav = 0, wav_ok = 0;
    for (const ResEntry* e : v.list(RT_WAV)) {
        ++wav;
        std::vector<std::uint8_t> b;
        if (v.read(*e, b, err) && b.size() > 44 && !std::memcmp(b.data(), "RIFF", 4) && !std::memcmp(b.data() + 8, "WAVE", 4)) ++wav_ok;
        else if (wav - wav_ok <= 5) std::printf("  som %s: cabecalho nao e RIFF/WAVE\n", e->name.c_str());
    }
    std::printf("sons: %zu de %zu com cabecalho WAVE valido\n", wav_ok, wav);
    return bad;
}

}  // namespace

int main(int argc, char** argv) {
    std::string demo, assets, f3map_dir;
    bool maps = false, models = false, all = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--demo") demo = next();
        else if (a == "--assets") assets = next();
        else if (a == "--maps") maps = true;
        else if (a == "--models") models = true;
        else if (a == "--tudo") all = true;
        else if (a == "--f3map") f3map_dir = next();
        else if (a == "-h" || a == "--help") { std::puts("uso: F3VBCheck --demo <pasta da demo> [--assets <original_assets>] [--maps [--f3map <pasta dos .f3map>]] [--models]"); return 0; }
    }
    if (demo.empty()) { std::puts("diga a pasta da demo: F3VBCheck --demo \"<pasta>\""); return 2; }
    Vfs v;
    std::string err;
    if (!v.open_demo(demo, err)) { std::printf("demo NAO abriu: %s\n", err.c_str()); return 1; }
    std::printf("demo: %s | %zu recursos em %zu .grp\n", demo.c_str(), v.size(), v.packs().size());
    std::map<std::string, int> by_type;
    for (const ResEntry& e : v.entries()) ++by_type[res_type_name(e.type)];
    for (const auto& kv : by_type) std::printf("  %-18s %5d\n", kv.first.c_str(), kv.second);
    int fails = 0;
    std::uint64_t bytes = 0;
    std::vector<std::uint8_t> a, b;
    for (const ResEntry& e : v.entries()) { if (!v.read(e, a, err)) { ++fails; std::printf("  falha ao ler %s: %s\n", v.where(e).c_str(), err.c_str()); } bytes += a.size(); }
    std::printf("leitura direta: %zu recursos, %.1f MB, %d falhas\n", v.size(), static_cast<double>(bytes) / 1048576.0, fails);
    if (!assets.empty()) {
        Vfs x;
        if (!x.open_extracted(assets, err)) { std::printf("copia extraida NAO abriu: %s\n", err.c_str()); return 1; }
        int same = 0, diff = 0, missing = 0;
        for (const ResEntry& e : v.entries()) {
            const ResEntry* o = x.find_lump(v.packs()[e.pack], e.lump);
            if (!o) { ++missing; if (missing <= 5) std::printf("  sem par na copia: %s (%s)\n", v.where(e).c_str(), e.name.c_str()); continue; }
            if (!v.read(e, a, err) || !x.read(*o, b, err)) { ++diff; continue; }
            if (a.size() == b.size() && fnv1a(a) == fnv1a(b) && a == b) ++same;
            else { ++diff; if (diff <= 5) std::printf("  DIFERENTE: %s (%s) %zu x %zu bytes\n", v.where(e).c_str(), e.name.c_str(), a.size(), b.size()); }
            if (o->name != e.name && diff + missing < 5) std::printf("  nome diferente: %s vs %s\n", e.name.c_str(), o->name.c_str());
        }
        std::printf("comparacao com a copia extraida: %d iguais, %d diferentes, %d sem par (de %zu)\n", same, diff, missing, v.size());
        fails += diff + missing;
    }
    if (maps) fails += check_maps(v, f3map_dir);
    if (models) fails += check_models(v);
    if (all) fails += check_all(v, demo);
    std::puts(fails ? "RESULTADO: HA PROBLEMAS (veja acima)" : "RESULTADO: OK");
    return fails ? 1 : 0;
}
