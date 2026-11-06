// Testa o nucleo portavel do visualizador (leitor F3M, cena, animacao, skinning) contra valores calculados em Python.
// Uso: test_core PASTA_DO_FIXTURE
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "../src/f3m.h"
#include "../src/scene.h"

using namespace f3v;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::vector<std::uint8_t> slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static bool load(const std::string& path, F3MFile& f) {
    std::string err;
    if (!load_f3m_file(path, f, err)) { std::printf("erro ao carregar %s: %s\n", path.c_str(), err.c_str()); return false; }
    return true;
}

struct Expect { std::string clip; float t; std::vector<float> pts; };

static std::vector<Expect> read_expected(const std::string& path) {
    std::vector<Expect> out;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream is(line);
        std::string tag; Expect e; std::size_t nv;
        is >> tag >> e.clip >> e.t >> nv;
        for (std::size_t i = 0; i < nv * 3; ++i) { float v; is >> v; e.pts.push_back(v); }
        out.push_back(std::move(e));
    }
    return out;
}

static void compare_scene(Scene& s, const std::vector<Expect>& exp, const char* label) {
    for (const Expect& e : exp) {
        int ci = -1;
        for (std::size_t i = 0; i < s.clips.size(); ++i) if (s.clips[i].name == e.clip) ci = static_cast<int>(i);
        if (e.clip != "-" && ci < 0) { std::printf("[%s] clipe %s ausente\n", label, e.clip.c_str()); ++fails; continue; }
        s.evaluate(ci, e.t);
        std::vector<float> pos, nrm;
        s.skin(0, pos, nrm);
        CHECK(pos.size() == e.pts.size());
        float worst = 0;
        for (std::size_t i = 0; i < pos.size() && i < e.pts.size(); ++i) worst = std::fmax(worst, std::fabs(pos[i] - e.pts[i]));
        if (worst > 1e-4f) std::printf("[%s] clipe=%s t=%g erro maximo %g\n", label, e.clip.c_str(), e.t, worst);
        CHECK(worst <= 1e-4f);
        for (float v : pos) CHECK(std::isfinite(v));
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: test_core PASTA"); return 2; }
    const std::string dir = argv[1];
    const auto expected = read_expected(dir + "/expected.txt");
    CHECK(!expected.empty());

    // 1) arquivo completo
    {
        F3MFile f;
        CHECK(load(dir + "/model_full.f3m", f));
        CHECK(f.meshes.size() == 1 && f.joints.size() == 3 && f.clips.size() == 1 && f.textures.size() == 1);
        Scene s;
        LoadReport r = s.add(std::move(f));
        CHECK(r.replaced_model && r.replaced_skeleton && r.clips_added == 1);
        CHECK(r.channels_unmatched == 1);                                   // "OssoInexistente"
        compare_scene(s, expected, "completo");
        float mn[3], mx[3];
        CHECK(s.bounds(mn, mx) && std::fabs(mn[1]) < 1e-6f && std::fabs(mx[1] - 2.f) < 1e-6f);
    }
    // 2) mesclagem: modelo e depois complemento; e na ordem inversa
    for (int order = 0; order < 2; ++order) {
        F3MFile a, b;
        CHECK(load(dir + "/model_only.f3m", a) && load(dir + "/addon.f3m", b));
        Scene s;
        if (order == 0) { s.add(std::move(a)); LoadReport r = s.add(std::move(b)); CHECK(r.adopted_skeleton && r.clips_added == 1); }
        else { s.add(std::move(b)); LoadReport r = s.add(std::move(a)); CHECK(r.replaced_model && !r.replaced_skeleton); }
        CHECK(s.joints.size() == 3 && s.clips.size() == 1 && s.meshes.size() == 1);
        compare_scene(s, expected, order == 0 ? "modelo+complemento" : "complemento+modelo");
        F3MFile again;
        CHECK(load(dir + "/addon.f3m", again));
        s.add(std::move(again));                                            // mesma animacao de novo: ganha sufixo
        CHECK(s.clips.size() == 2 && s.clips[1].name == "bend (2)");
        s.clear_clips();
        CHECK(s.clips.empty());
    }
    // 3) truncamento: nenhum prefixo (menos o arquivo inteiro) pode carregar, e nada pode quebrar
    {
        const auto bytes = slurp(dir + "/model_full.f3m");
        std::size_t accepted = 0;
        for (std::size_t n = 0; n < bytes.size(); ++n) {
            F3MFile f; std::string err;
            if (load_f3m(bytes.data(), n, f, err)) ++accepted;
        }
        CHECK(accepted == 0);
        F3MFile f; std::string err;
        CHECK(load_f3m(bytes.data(), bytes.size(), f, err));
    }
    // 4) mutacoes aleatorias: nunca quebrar; se carregar, avaliar e fazer skinning com valores finitos
    {
        const auto bytes = slurp(dir + "/model_full.f3m");
        std::mt19937 rng(2024);
        int loaded = 0;
        for (int it = 0; it < 4000; ++it) {
            auto b = bytes;
            const int k = 1 + static_cast<int>(rng() % 5);
            for (int i = 0; i < k; ++i) b[rng() % b.size()] = static_cast<std::uint8_t>(rng());
            F3MFile f; std::string err;
            if (!load_f3m(b.data(), b.size(), f, err)) continue;
            ++loaded;
            Scene s;
            s.add(std::move(f));
            for (int c = -1; c < static_cast<int>(s.clips.size()); ++c) {
                s.evaluate(c, 0.37f);
                for (std::size_t m = 0; m < s.meshes.size(); ++m) { std::vector<float> p, n; s.skin(static_cast<int>(m), p, n); }
            }
        }
        std::printf("fuzz: %d mutacoes carregaram de 4000\n", loaded);
    }
    // 5) textura potencia de dois
    {
        Texture t; t.name = "x"; t.w = 3; t.h = 5; t.rgba.assign(3 * 5 * 4, 0);
        for (std::size_t i = 0; i < t.rgba.size(); i += 4) { t.rgba[i] = 200; t.rgba[i + 1] = 100; t.rgba[i + 2] = 50; t.rgba[i + 3] = 255; }
        std::vector<std::uint8_t> out; std::uint32_t w = 0, h = 0;
        resize_pow2(t, out, w, h);
        CHECK(w == 4 && h == 4 && out.size() == 4 * 4 * 4);
        for (std::size_t i = 0; i < out.size(); i += 4) CHECK(out[i] == 200 && out[i + 1] == 100 && out[i + 2] == 50 && out[i + 3] == 255);
        Texture p; p.w = 8; p.h = 2; p.rgba.assign(8 * 2 * 4, 7);
        resize_pow2(p, out, w, h);
        CHECK(w == 8 && h == 2 && out == p.rgba);
    }
    if (fails == 0) std::puts("viewer core tests OK");
    return fails ? 1 : 0;
}
