// f3v_analyze: gera o MESMO relatorio do log do visualizador, sem abrir janela.
//   f3v_analyze modelo.f3m [esqueleto_ou_animacoes.f3m ...] [--scale=X] [--out=relatorio.txt]
// Util para mandar o relatorio de um modelo sem enviar o arquivo do jogo.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "analyze.h"
#include "f3m.h"
#include "scene.h"

int main(int argc, char** argv) {
    std::vector<std::string> files;
    std::string out;
    float scale = 1.f;
    for (int i = 1; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--scale=", 8)) scale = static_cast<float>(std::atof(argv[i] + 8));
        else if (!std::strncmp(argv[i], "--out=", 6)) out = argv[i] + 6;
        else files.push_back(argv[i]);
    }
    if (files.empty()) {
        std::fprintf(stderr, "uso: f3v_analyze modelo.f3m [esqueleto_ou_animacoes.f3m ...] [--scale=X] [--out=relatorio.txt]\n");
        return 2;
    }
    f3v::Scene scene;
    for (const std::string& path : files) {
        f3v::F3MFile f;
        std::string err;
        if (!f3v::load_f3m_file(path, f, err)) { std::fprintf(stderr, "erro em %s: %s\n", path.c_str(), err.c_str()); return 1; }
        const f3v::LoadReport r = scene.add(std::move(f));
        std::fprintf(stderr, "%s: %s\n", path.c_str(), r.message.c_str());
    }
    scene.mesh_scale = scale;
    const std::string text = f3v::analyze_scene(scene, files[0]);
    std::fputs(text.c_str(), stdout);
    if (!out.empty()) {
        std::FILE* fp = std::fopen(out.c_str(), "wb");
        if (!fp) { std::fprintf(stderr, "nao consegui gravar %s\n", out.c_str()); return 1; }
        std::fwrite(text.data(), 1, text.size(), fp);
        std::fclose(fp);
    }
    return 0;
}
