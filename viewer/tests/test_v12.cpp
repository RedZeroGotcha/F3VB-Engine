// Testes v12 do nucleo portavel: log com carimbo, analise de modelo, estimativa de escala malha x esqueleto,
// Scene::mesh_scale e robustez da analise contra arquivos corrompidos.
// Uso: test_v12 [PASTA_DO_FIXTURE]   (a pasta do fixture e opcional: habilita o teste de mutacoes aleatorias)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "../src/analyze.h"
#include "../src/f3m.h"
#include "../src/log.h"
#include "../src/scene.h"

using namespace f3v;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::string slurp_text(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static bool contains(const std::string& s, const char* t) { return s.find(t) != std::string::npos; }

// Modelo sintetico: 3 juntas em linha (Root em y=0, Bone em y=1, Tip em y=2) e UM tubo continuo de 10 aneis (y = 0 a 1.8).
// Aneis em y<0.8 pesam 1 na raiz, y>=1 pesam 1 no osso, o anel em y=0.8 mistura 50/50 (como a "emenda" de uma articulacao).
// `k` multiplica as posicoes da malha (k=10.24 imita o F3M do conversor v11).
static F3MFile make_model(float k, bool with_clip) {
    F3MFile f;
    f.flags = kFlagModel | kFlagSkeleton | (with_clip ? kFlagClips : 0u);
    f.name = "sintetico";
    Material mat;
    mat.name = "m";
    f.materials.push_back(mat);
    Joint root, child, tip;
    root.name = "Root";
    child.name = "Bone";
    child.parent = 0;
    child.t[1] = 1.f;
    tip.name = "Tip";
    tip.parent = 1;
    tip.t[1] = 1.f;
    Mat4 id, inv1, inv2;
    inv1.m[13] = -1.f;                                  // ibm = inverso do mundo (translacao negativa)
    inv2.m[13] = -2.f;
    std::memcpy(root.ibm, id.m, sizeof root.ibm);
    std::memcpy(child.ibm, inv1.m, sizeof child.ibm);
    std::memcpy(tip.ibm, inv2.m, sizeof tip.ibm);
    f.joints = {root, child, tip};
    Mesh m;
    m.name = "tubo";
    m.material = 0;
    m.has_skin = true;
    m.bones = {"Root", "Bone"};
    const int ring = 8, rings = 10;
    for (int h = 0; h < rings; ++h)
        for (int i = 0; i < ring; ++i) {
            const float a = 6.2831853f * static_cast<float>(i) / ring, y = 0.2f * static_cast<float>(h);
            const float p[3] = {0.02f * std::cos(a), y, 0.02f * std::sin(a)};
            for (int c = 0; c < 3; ++c) m.pos.push_back(p[c] * k);
            const float n[3] = {std::cos(a), 0.f, std::sin(a)};
            for (int c = 0; c < 3; ++c) m.nrm.push_back(n[c]);
            m.uv.push_back(static_cast<float>(i) / ring);
            m.uv.push_back(static_cast<float>(h) / rings);
            m.joints.insert(m.joints.end(), {0, 1, 0, 0});
            float w0 = h < 4 ? 1.f : (h == 4 ? 0.5f : 0.f);
            m.weights.insert(m.weights.end(), {w0, 1.f - w0, 0.f, 0.f});
        }
    for (int h = 0; h + 1 < rings; ++h)
        for (int i = 0; i < ring; ++i) {
            const std::uint32_t base = static_cast<std::uint32_t>(h * ring);
            const std::uint32_t a = base + static_cast<std::uint32_t>(i), b = base + static_cast<std::uint32_t>((i + 1) % ring);
            const std::uint32_t c = a + ring, d = b + ring;
            m.idx.insert(m.idx.end(), {a, b, c, b, d, c});
        }
    f.meshes.push_back(m);
    if (with_clip) {
        Clip c;
        c.name = "dobra";
        c.duration = 1.f;
        Channel ch;
        ch.bone = "Bone";
        ch.path = 1;                                    // rotacao: 90 graus em Z no fim
        ch.times = {0.f, 1.f};
        ch.values = {0, 0, 0, 1, 0, 0, 0.70710678f, 0.70710678f};
        c.channels.push_back(ch);
        f.clips.push_back(c);
    }
    return f;
}

static void test_log() {
    const std::string name = Log::file_name(1700000000);       // 2023-11-14 (qualquer fuso cai entre 13 e 15)
    CHECK(name.size() == std::string("f3viewer_log_MM-DD-YY_hh-mm-ss.txt").size());
    CHECK(name.rfind("f3viewer_log_", 0) == 0 && name.substr(name.size() - 4) == ".txt");
    CHECK(name[15] == '-' && name[18] == '-' && name[21] == '_' && name[24] == '-' && name[27] == '-');
    Log a, b;
    CHECK(a.open("."));
    CHECK(b.open("."));                                         // mesmo segundo: nome diferente (sufixo)
    CHECK(a.path() != b.path());
    a.line("primeira linha");
    a.linef("numero %d e texto %s", 42, "ok");
    a.block("bloco\nem duas linhas");
    const std::string path = a.path();
    a.close();
    const std::string txt = slurp_text(path);
    CHECK(contains(txt, "primeira linha") && contains(txt, "numero 42 e texto ok") && contains(txt, "bloco\r\nem duas linhas\r\n"));
    CHECK(txt.size() > 4 && txt[0] == '[' && txt[3] == ':' && txt[6] == ':' && txt[9] == '.' && txt[13] == ']');   // [hh:mm:ss.mmm]
    const std::string pb = b.path();
    b.close();
    std::remove(path.c_str());
    std::remove(pb.c_str());
    Log bad;
    CHECK(!bad.open("/pasta/que/nao/existe"));
}

static void test_scale_and_analysis() {
    // escala correta: estimativa ~1, sem avisos de escala
    {
        Scene s;
        s.add(make_model(1.f, true));
        double en = 0, eb = 0;
        int ns = 0;
        const double f = estimate_mesh_scale(s, &en, &eb, &ns);
        CHECK(ns >= 20);
        CHECK(std::fabs(f - 1.0) < 0.05);
        const std::string rep = analyze_scene(s, "sintetico");
        CHECK(contains(rep, "[RESUMO] 0 aviso(s)"));
        CHECK(contains(rep, "ok: escala coerente"));
        CHECK(contains(rep, "[ANIMACOES] 1") && contains(rep, "dobra"));
    }
    // malha 10.24x maior (como no v11): estimativa ~ 1/10.24 e aviso
    {
        Scene s;
        s.add(make_model(10.24f, true));
        const double f = estimate_mesh_scale(s);
        CHECK(std::fabs(f * 10.24 - 1.0) < 0.05);
        const std::string rep = analyze_scene(s);
        CHECK(contains(rep, "malha fora da escala do esqueleto"));
        // aplicar a escala estimada corrige: estimativa volta a ~1 e o esticamento some
        s.mesh_scale = static_cast<float>(f);
        CHECK(std::fabs(estimate_mesh_scale(s) - 1.0) < 0.05);
        const PoseStats p = measure_pose(s, 0, 1.0f);
        CHECK(p.ok && p.nonfinite == 0 && p.edge_p95 < 2.0);
    }
    // sem a correcao, a animacao estica as arestas (o que o usuario viu no cao)
    {
        Scene s;
        s.add(make_model(10.24f, true));
        const PoseStats p = measure_pose(s, 0, 1.0f);
        CHECK(p.edge_p95 > 2.0);
        CHECK(contains(analyze_pose(s, 0, 1.0f, false), "SUSPEITO"));
    }
    // mesh_scale em Scene::skin: malha 10x com escala 0.1 == malha 1x (pose de bind e animada)
    {
        Scene big, ref;
        big.add(make_model(10.f, true));
        ref.add(make_model(1.f, true));
        big.mesh_scale = 0.1f;
        for (int clip : {-1, 0}) {
            big.evaluate(clip, 0.7f);
            ref.evaluate(clip, 0.7f);
            std::vector<float> a, an, b, bn;
            big.skin(0, a, an);
            ref.skin(0, b, bn);
            CHECK(a.size() == b.size());
            double worst = 0;
            for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) worst = std::max(worst, std::fabs(static_cast<double>(a[i]) - b[i]));
            CHECK(worst < 1e-5);
        }
        float mn[3], mx[3], rn[3], rx[3];
        CHECK(big.bounds(mn, mx) && ref.bounds(rn, rx));
        CHECK(std::fabs(mx[1] - rx[1]) < 1e-4f);
    }
    // deslocamento constante do mundo*ibm (posicionamento inicial do GR2) e juntas sem correspondencia
    {
        F3MFile f = make_model(1.f, false);
        for (Joint& j : f.joints) { Mat4 m; std::memcpy(m.m, j.ibm, sizeof m.m); m.m[12] += 0.02f; m.m[14] -= 0.03f; std::memcpy(j.ibm, m.m, sizeof j.ibm); }
        f.meshes[0].bones[1] = "OssoQueNaoExiste";
        Scene s;
        s.add(std::move(f));
        const std::string rep = analyze_scene(s);
        CHECK(contains(rep, "deslocamento CONSTANTE"));
        CHECK(contains(rep, "ossos SEM junta: OssoQueNaoExiste"));
    }
    // dump de ossos e resumo de pose
    {
        Scene s;
        s.add(make_model(1.f, true));
        const std::string d = dump_bones(s, 0, 1.0f);
        CHECK(contains(d, "Root") && contains(d, "Bone") && contains(d, "maior deslocamento: Tip"));
        CHECK(contains(analyze_pose(s, -1, 0.f, true), "pose: BIND"));
        CHECK(contains(analyze_pose(s, 0, 0.5f, false), "animacao #0"));
    }
    // modelo sem esqueleto e cena vazia: nao pode quebrar
    {
        Scene empty;
        CHECK(!analyze_scene(empty).empty());
        CHECK(estimate_mesh_scale(empty) == 0);
        F3MFile f = make_model(1.f, false);
        f.joints.clear();
        f.meshes[0].has_skin = false;
        Scene s;
        s.add(std::move(f));
        CHECK(!analyze_scene(s).empty());
        CHECK(estimate_mesh_scale(s) == 0);
        CHECK(contains(dump_bones(s, -1, 0), "nao tem esqueleto"));
    }
}

static void test_bone_alias_and_missing() {
    // 1) "Bone" no malha e "Bone_1" no esqueleto (como "L Clavicle" x "L Clavicle_1" em HumFSkl): mesmo resultado que o nome exato
    {
        F3MFile a = make_model(1.f, true), b = make_model(1.f, true);
        b.joints[1].name = "Bone_1";
        Scene sa, sb;
        sa.add(std::move(a));
        sb.add(std::move(b));
        CHECK(sb.find_joint("Bone") == 1 && sb.find_joint("Bone_1") == 1 && sb.find_joint("bone") == 1 && sb.find_joint("Nope") == -1);
        CHECK(sb.bone_to_joint(0, 1) == 1);
        std::vector<float> pa, pb, n;
        sa.evaluate(0, 1.0f);
        sb.evaluate(0, 1.0f);
        sa.skin(0, pa, n);
        sb.skin(0, pb, n);
        double worst = 0;
        for (std::size_t i = 0; i < pa.size() && i < pb.size(); ++i) worst = std::max(worst, std::fabs(static_cast<double>(pa[i]) - pb[i]));
        CHECK(pa.size() == pb.size() && worst < 1e-6);
        CHECK(contains(analyze_scene(sb), "casados por variacao de nome"));
        CHECK(contains(analyze_scene(sb), "[RESUMO] 0 aviso(s)"));
        // o clipe com o nome do osso "Bone" tambem casa com a junta "Bone_1" (nenhum canal sem osso)
        CHECK(!contains(analyze_scene(sb), "ossos sem junta: 1"));
    }
    // 2) osso realmente inexistente: a parte do peso fica no bind (antes a malha colapsava em direcao a origem)
    {
        F3MFile f = make_model(1.f, true);
        f.meshes[0].bones[1] = "OssoQueNaoExiste";
        Scene s;
        s.add(std::move(f));
        s.evaluate(0, 1.0f);
        std::vector<float> pos, n;
        s.skin(0, pos, n);
        const Mesh& m = s.meshes[0];
        // anel do topo (h=9, peso 1 no osso ausente): fica exatamente onde esta no bind
        const std::size_t v = static_cast<std::size_t>(9 * 8);
        CHECK(std::fabs(pos[v * 3 + 1] - m.pos[v * 3 + 1]) < 1e-6f && std::fabs(pos[v * 3] - m.pos[v * 3]) < 1e-6f);
        // anel misto (h=4: 0.5 raiz + 0.5 osso ausente): sem colapso, continua perto do bind
        const std::size_t u = static_cast<std::size_t>(4 * 8);
        CHECK(std::fabs(pos[u * 3 + 1] - m.pos[u * 3 + 1]) < 1e-5f);
        const PoseStats p = measure_pose(s, 0, 1.0f);
        CHECK(p.nonfinite == 0 && p.edge_p99 < 1.5);
    }
}

static void test_fuzz(const std::string& dir) {
    std::ifstream in(dir + "/model_full.f3m", std::ios::binary);
    if (!in) { std::printf("(sem fixture: mutacoes puladas)\n"); return; }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::mt19937 rng(77);
    int analyzed = 0;
    for (int it = 0; it < 1500; ++it) {
        auto b = bytes;
        const int k = 1 + static_cast<int>(rng() % 5);
        for (int i = 0; i < k; ++i) b[rng() % b.size()] = static_cast<std::uint8_t>(rng());
        F3MFile f;
        std::string err;
        if (!load_f3m(b.data(), b.size(), f, err)) continue;
        Scene s;
        s.add(std::move(f));
        if (it % 3 == 0) s.mesh_scale = 0.5f;
        const std::string rep = analyze_scene(s, "mutado");
        (void)estimate_mesh_scale(s);
        (void)analyze_pose(s, s.clips.empty() ? -1 : 0, 0.3f, s.clips.empty());
        (void)dump_bones(s, s.clips.empty() ? -1 : 0, 0.3f);
        CHECK(!rep.empty());
        ++analyzed;
    }
    std::printf("fuzz da analise: %d arquivos mutados carregaram e foram analisados sem quebrar\n", analyzed);
    CHECK(analyzed > 100);
}

int main(int argc, char** argv) {
    test_log();
    test_scale_and_analysis();
    test_bone_alias_and_missing();
    if (argc > 1) test_fuzz(argv[1]);
    if (fails) { std::printf("%d verificacao(oes) falharam\n", fails); return 1; }
    std::puts("v12 core tests OK");
    return 0;
}
