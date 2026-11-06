// Analise de modelos F3M: texto para o log do visualizador e para a ferramenta f3v_analyze.
// Tudo aqui e portatil (so C++ padrao). As funcoes REAVALIAM a pose da cena (Scene::evaluate): depois de chama-las,
// quem desenha deve avaliar a pose desejada de novo (o visualizador ja faz isso a cada quadro).
#pragma once
#include <string>

#include "scene.h"

namespace f3v {

// Hipotese de trabalho (ver docs/ACHADOS_DEMO.md): a geometria do B3D vem em unidades ~10,24x maiores que o esqueleto do GR2.
constexpr double kMeshScaleHypothesis = 100.0 / 1024.0;

struct PoseStats {
    bool ok = false;                          // havia malha para medir
    float bb_min[3] = {0, 0, 0}, bb_max[3] = {0, 0, 0};
    double bbox_growth = 0;                   // maior razao (tamanho da caixa na pose / tamanho no bind) entre os eixos
    double edge_p50 = 0, edge_p95 = 0, edge_p99 = 0, edge_gt2 = 0;   // razao comprimento da aresta pose/bind e fracao > 2x
    int edges = 0, nonfinite = 0;
    double max_joint_shift = 0;               // maior deslocamento de uma junta em relacao ao bind (unidades do esqueleto)
};

// clip < 0 = pose de bind. Se `bind_cache` != nullptr reaproveita as posicoes de bind (uma lista por malha).
PoseStats measure_pose(Scene& s, int clip, float t);

// Fator EXTRA f tal que (malha * f) encaixa melhor nos ossos que a controlam. 1 = escala ja correta; 0 = sem dados.
// err_now = erro medio com f=1, err_best = erro medio em f* (distancia ate o segmento do osso, unidades do esqueleto).
double estimate_mesh_scale(Scene& s, double* err_now = nullptr, double* err_best = nullptr, int* samples = nullptr);

std::string analyze_scene(Scene& s, const std::string& source = "");                 // relatorio completo
std::string analyze_pose(Scene& s, int clip, float t, bool bind_mode);               // resumo da pose atual (tecla P)
std::string dump_bones(Scene& s, int clip, float t);                                 // ossos na pose atual (tecla K)

}  // namespace f3v
