// Cena do visualizador: junta modelo, esqueleto e animacoes (carregados em qualquer ordem), avalia a pose e faz o skinning na CPU.
// Portavel (sem Windows/OpenGL): e a parte testada automaticamente.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "f3m.h"
#include "vmath.h"

namespace f3v {

struct LoadReport {
    bool replaced_model = false, adopted_skeleton = false, replaced_skeleton = false;
    int clips_added = 0, channels_unmatched = 0;
    std::string message;
};

class Scene {
public:
    // Regras: arquivo COM malhas substitui o modelo (se trouxer esqueleto, substitui esqueleto e animacoes);
    // arquivo SEM malhas e um "complemento": adota o esqueleto se a cena nao tiver um e sempre acrescenta animacoes.
    LoadReport add(F3MFile&& f);
    void clear_clips();

    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Joint> joints;
    std::vector<Clip> clips;
    std::string model_name;

    // pose: clip < 0 -> pose de bind. Preenche joint_world e joint_skin (= mundo * ibm).
    void evaluate(int clip, float t);
    // skinning de uma malha com a pose atual (sem esqueleto/pesos: devolve a posicao original).
    void skin(int mesh, std::vector<float>& pos, std::vector<float>& nrm) const;
    bool bounds(float mn[3], float mx[3]) const;       // caixa envolvente da pose de bind

    // Escala EXTRA aplicada na geometria (vertices) antes do skinning, 1 = sem mudanca. Serve para F3M gerados pelo
    // conversor v11 (malha 10,24x maior que o esqueleto) e para experimentar valores no visualizador (teclas M , . N).
    float mesh_scale = 1.f;

    std::vector<Mat4> joint_world, joint_skin;
    // Junta pelo nome do osso. Alem do nome exato aceita variacoes que o exportador de origem introduz (medido em HumFSkl/HumMNor):
    // "L Clavicle" -> "L Clavicle_1", "X_1" -> "X", e diferenca de maiusculas. -1 se nao houver.
    int find_joint(const std::string& bone) const;
    int bone_to_joint(int mesh, int bone_index) const;

private:
    void rebuild();
    std::vector<std::vector<int>> mesh_map_;            // por malha: osso -> junta (-1 = sem correspondencia)
    std::unordered_map<std::string, int> joint_index_;
    std::vector<std::vector<int>> clip_bind_;            // por clipe: joint*3 + caminho -> canal (-1)
    void bind_clip(int clip);
};

// Reamostra para potencia de dois (OpenGL 1.x nao garante textura nao-potencia-de-dois). Maximo 2048.
void resize_pow2(const Texture& t, std::vector<std::uint8_t>& out, std::uint32_t& w, std::uint32_t& h);

}  // namespace f3v
