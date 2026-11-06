// v20: monta um modelo pronto para desenhar (F3MFile, o mesmo do visualizador) a partir do B3D + esqueleto GR2 + animacoes GR2,
// direto da demo, sem o conversor Python. Porta fiel de tools/py/f3vb/export.py (Exporter.scene, apply_initial_placement,
// resolve_bone_aliases, _fix_winding) e gr2export.py (read_skeleton, read_clips): o resultado e conferido byte a byte com os
// .f3m que o Python gera (tests/test_v20.cpp e a ferramenta F3VBCheck --models).
//
// Espaco de saida (igual ao Python): mao-direita, Y para cima (o Z do jogo e invertido), geometria na escala do esqueleto
// (malha x 100/1024). Para desenhar no mapa (mao-esquerda, unidades do jogo) quem chama aplica a escala 10,24 e inverte o Z.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "b3d.h"
#include "f3m.h"
#include "gr2.h"

namespace f3v {

constexpr float kMeshScale = 100.0f / 1024.0f;   // geometria do B3D -> unidades do esqueleto (MEDIDO no v12: CR_Dog, Eyebot)

// Juntas "cruas" (espaco do jogo, antes da conversao), como o gr2export.read_skeleton do Python.
struct RawJoint { std::string name; int parent = -1; double pos[3]; double quat[4]; double scale[3]; double inv_world[16]; };
// Clipe "cru" (espaco do jogo): por osso, chaves amostradas no passo do GR2.
struct RawTrack { std::string bone; bool has_t = false, has_r = false, has_s = false;
                  std::vector<double> tt, tr, ts; std::vector<double> vt, vr, vs; };   // valores: 3, 4, 3 por chave
struct RawClip { std::string name; double duration = 0; std::vector<RawTrack> tracks; };

// Primeiro esqueleto "de verdade" (ignora Light/Camera e esqueletos de 1 osso). false se nao houver.
bool raw_skeleton(const Gr2File& f, std::vector<RawJoint>& out, std::string* name = nullptr);
// Todas as animacoes do arquivo, so com trilhas de ossos que existem em `bones`. clip_name vazio = nome da animacao.
void raw_clips(const Gr2File& f, const std::vector<RawJoint>& joints, const std::string& clip_name, std::vector<RawClip>& out);

struct ModelBuildReport {
    std::vector<std::string> warnings;
    bool placement_applied = false;
    double placement[3] = {0, 0, 0};
    int aliased_bones = 0;
};

// texture_loader(nome sem extensao) -> bytes de TGA/BMP (vazio se nao achar)
using TextureLoader = std::function<std::vector<std::uint8_t>(const std::string& stem)>;

// Monta o F3MFile. joints/clips podem ser vazios (modelo estatico). Sem dependencia de janela/OpenGL.
// mesh_scale: kMeshScale (padrao, igual ao Python) ou o valor de guess_mesh_scale().
F3MFile build_model(const B3dModel& model, const std::string& name, const std::vector<RawJoint>& joints, const std::vector<RawClip>& clips,
                    const TextureLoader& tex, ModelBuildReport* report = nullptr, double mesh_scale = kMeshScale);

// v20: escala da malha em relacao ao esqueleto. Quase todos os modelos tem a malha 10,24x maior que o esqueleto (kMeshScale),
// mas os B3D antigos ("not re-converted", como avisa o log da demo; ex.: IT_MainMenu_boxcar, o vagao do menu) ja estao na
// escala do esqueleto (1.0). Escolhe o valor que deixa o tamanho da malha mais perto do tamanho do esqueleto na pose de bind.
double guess_mesh_scale(const B3dModel& model, const std::vector<RawJoint>& joints);

// Grava no formato F3M1 (o mesmo do tools/py/f3vb/f3m.py), para conferencia e cache.
std::vector<std::uint8_t> save_f3m(const F3MFile& f);

// Prefixos de busca de animacao (igual ao extractor.anim_prefixes): "<nome>_" e, para humanos, "HumM_"/"HumF_".
std::vector<std::string> anim_prefixes(const std::string& model_name);

}  // namespace f3v
