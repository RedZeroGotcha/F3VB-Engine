// Utilitario: carrega um ou mais .f3m (na ordem dada), avalia a pose e imprime as posicoes apos o skinning (para conferir contra o Python).
// Uso: skin_cli CLIPE T arq1.f3m [arq2.f3m ...]     (CLIPE "-" = pose de bind)
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../src/scene.h"

int main(int argc, char** argv) {
    if (argc < 4) { std::puts("uso: skin_cli CLIPE T arq.f3m [...]"); return 2; }
    f3v::Scene s;
    for (int i = 3; i < argc; ++i) {
        f3v::F3MFile f; std::string err;
        if (!f3v::load_f3m_file(argv[i], f, err)) { std::printf("ERRO %s: %s\n", argv[i], err.c_str()); return 1; }
        const auto r = s.add(std::move(f));
        std::fprintf(stderr, "%s: %s\n", argv[i], r.message.c_str());
    }
    int ci = -1;
    for (std::size_t i = 0; i < s.clips.size(); ++i) if (s.clips[i].name == argv[1]) ci = static_cast<int>(i);
    s.evaluate(ci, static_cast<float>(std::atof(argv[2])));
    for (std::size_t m = 0; m < s.meshes.size(); ++m) {
        std::vector<float> p, n;
        s.skin(static_cast<int>(m), p, n);
        for (std::size_t i = 0; i < p.size(); i += 3) std::printf("%d %.6f %.6f %.6f\n", static_cast<int>(m), p[i], p[i + 1], p[i + 2]);
    }
    return 0;
}
