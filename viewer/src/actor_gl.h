// v20: desenho dos atores (actor.h) com OpenGL 1.x: skinning na CPU (Scene::skin), luz difusa simples por vertice
// (a demo ilumina personagens com uma luz direcional + ambiente; os mapas ja vem com a luz pronta nos lightmaps),
// textura modulada pela luz, corte de alfa (cabelos, barba) e mistura nos materiais ALPHABLEND.
#pragma once
#include <vector>

#include "actor.h"

namespace f3v {

struct ActorGL {
    std::vector<std::vector<unsigned>> part_tex;      // por parte: textura do OpenGL de cada textura da cena
    std::vector<unsigned> weapon_tex;
    bool uploaded = false;
};

struct ActorLight {
    float dir[3] = {-0.45f, -0.8f, 0.4f};             // direcao da luz (para onde ela vai), no mundo
    float ambient[3] = {0.9f, 0.9f, 0.95f};            // luz ambiente (o material ambiente da demo e 0,5)
    float diffuse[3] = {0.75f, 0.72f, 0.65f};
};

void actor_gl_upload(const Actor& a, ActorGL& g, float aniso);
void actor_gl_release(ActorGL& g);
// desenha com a camera ja montada (matriz MODELVIEW = camera)
void actor_gl_draw(const Actor& a, const ActorGL& g, const ActorLight& light, bool blend_pass);

}  // namespace f3v
