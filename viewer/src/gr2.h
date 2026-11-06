// v20: leitor de Granny 2 (.gr2: esqueletos SKEL_Skeleton e animacoes GR2_Animation) em C++ puro, direto da memoria (do .grp).
//
// Portado do parser MIT "blendergranny" que o projeto ja usa em Python (third_party/blendergranny, licenca e aviso preservados;
// registro em docs/LICENCAS_E_DIREITOS.md). Cobre o que a demo do Van Buren usa (medido nos 547 .gr2 da demo):
//   - arquivos versao 6, ponteiros de 32 bits, little-endian;
//   - secoes sem compressao ou com Oodle0 (todas as secoes com dados da demo sao Oodle0);
//   - fixups de ponteiro de 12 bytes (ou 8 no formato antigo), arvore de tipos (type definitions) com membros inline;
//   - esqueleto (nome, pai, Transform, InverseWorldTransform) e trilhas de transformacao com curvas "LegacyCurve32f"
//     (grau 0-3, nos e controles em float; e o unico tipo de curva da demo).
// Nada da granny2.dll e usado. Spec: docs/specs/gr2.md.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace f3v {

struct Gr2Bone {
    std::string name;
    int parent = -1;
    std::uint32_t flags = 0;
    float pos[3] = {0, 0, 0};
    float quat[4] = {0, 0, 0, 1};                 // x, y, z, w
    float scale_shear[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float inv_world[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};   // como no arquivo (vetor-linha, translacao na ultima linha)
};

struct Gr2Skeleton {
    std::string name;
    std::vector<Gr2Bone> bones;
};

// Curva "LegacyCurve32f": B-spline de grau 0..3 com `knots` (tempos) e `controls` (knots.size() x dim valores).
struct Gr2Curve {
    bool present = false;
    int degree = 0, dim = 0;
    std::vector<float> knots, controls;
    void eval(double t, double* out) const;        // mesmo algoritmo do parser Python, em double (grau 3 usa o controle do centro)
    void eval(float t, float* out) const;          // idem, saida em float
};

struct Gr2TransformTrack {
    std::string name;
    std::int32_t flags = 0;
    Gr2Curve orientation, position, scale_shear;
};

struct Gr2TrackGroup {
    std::string name;
    std::vector<Gr2TransformTrack> tracks;
};

struct Gr2Animation {
    std::string name;
    float duration = 0, time_step = 0, oversampling = 0;
    std::vector<std::string> track_groups;         // nomes dos grupos usados
};

struct Gr2File {
    int version = 0;
    std::vector<Gr2Skeleton> skeletons;
    std::vector<Gr2TrackGroup> track_groups;
    std::vector<Gr2Animation> animations;
};

bool load_gr2(const std::uint8_t* data, std::size_t size, Gr2File& out, std::string& err);

// Descompressor Oodle0 (exposto para os testes). expanded = tamanho final; stop16/stop8 = limites dos 3 blocos.
bool oodle0_decompress(const std::uint8_t* src, std::size_t src_size, std::size_t expanded, std::size_t first16, std::size_t first8,
                       std::vector<std::uint8_t>& out, std::string& err);

}  // namespace f3v
