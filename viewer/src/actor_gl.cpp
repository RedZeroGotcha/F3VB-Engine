// v20: desenho dos atores (ver actor_gl.h).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include "actor_gl.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "map_render.h"

#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_RGB_SCALE 0x8573
#define GL_PRIMARY_COLOR 0x8577
#endif

namespace f3v {

namespace {

void upload_scene(const Scene& s, std::vector<unsigned>& out, float aniso) {
    out.clear();
    for (const Texture& t : s.textures) {
        Image im;
        im.w = t.w; im.h = t.h; im.rgba = t.rgba;
        out.push_back(t.w && t.h ? gl_upload_texture(im, false, aniso) : 0u);
    }
}

void draw_part(const ActorPart& p, const std::vector<unsigned>& tex, const float model[16], const ActorLight& L, bool blend_pass) {
    const Scene& s = p.scene;
    // luz no espaco do modelo: inversa da rotacao (a matriz tem escala uniforme, entao a transposta da parte 3x3 normalizada)
    const float k = std::sqrt(model[0] * model[0] + model[1] * model[1] + model[2] * model[2]);
    float l[3] = {0, 0, 0};
    if (k > 1e-6f) for (int c = 0; c < 3; ++c) l[c] = -(model[c * 4 + 0] * L.dir[0] + model[c * 4 + 1] * L.dir[1] + model[c * 4 + 2] * L.dir[2]) / k;
    const float ll = std::sqrt(l[0] * l[0] + l[1] * l[1] + l[2] * l[2]);
    if (ll > 1e-6f) for (float& x : l) x /= ll;
    // com o espelho do Z na matriz, o determinante e negativo: o sentido dos triangulos inverte
    const float det = model[0] * (model[5] * model[10] - model[9] * model[6]) - model[4] * (model[1] * model[10] - model[9] * model[2]) + model[8] * (model[1] * model[6] - model[5] * model[2]);
    (void)det;
    glPushMatrix();
    glMultMatrixf(model);
    std::vector<float> pos, nrm, col;
    for (std::size_t i = 0; i < s.meshes.size(); ++i) {
        if (i < p.hidden.size() && p.hidden[i]) continue;
        const Mesh& m = s.meshes[i];
        const Material* mat = (m.material >= 0 && static_cast<std::size_t>(m.material) < s.materials.size()) ? &s.materials[static_cast<std::size_t>(m.material)] : nullptr;
        // tipo de sombreamento pelo id do material (o mesmo esquema dos mapas): *_2X = textura x luz x 2,
        // *VERTEX_ALPHA* = transparencia pela cor de vertice (misturado), *IMAGE_ALPHA* = recorte pelo alfa da textura
        const std::string sh = mat ? mat->shader : std::string();
        const bool x2 = sh.find("_2X") != std::string::npos;
        const bool valpha = sh.find("VERTEX_ALPHA") != std::string::npos;
        const bool blend = (mat && mat->alpha_mode != 0) || valpha;
        if (blend != blend_pass) continue;
        s.skin(static_cast<int>(i), pos, nrm);
        const std::uint32_t nv = m.nv();
        col.resize(static_cast<std::size_t>(nv) * 4);
        const float* mc = mat ? mat->color : nullptr;
        for (std::uint32_t v = 0; v < nv; ++v) {
            const float d = std::max(0.f, nrm[v * 3] * l[0] + nrm[v * 3 + 1] * l[1] + nrm[v * 3 + 2] * l[2]);
            for (int c = 0; c < 3; ++c) {
                // D3D: material ambiente 0,5 x luz ambiente + material difuso x luz x N.L (medido nos B3D: difusa 1, ambiente 0,5);
                // nos materiais *_2X o resultado ainda e multiplicado por 2 (RGB_SCALE 2 abaixo)
                const float md = mc ? mc[c] : 1.f;
                col[v * 4 + static_cast<std::uint32_t>(c)] = std::min(1.f, 0.5f * L.ambient[c] + md * L.diffuse[c] * d);
            }
            float a = (mc && mat->alpha_mode != 0) ? mc[3] : 1.f;
            if (valpha && m.col.size() >= static_cast<std::size_t>(nv) * 4) a *= m.col[v * 4 + 3];
            col[v * 4 + 3] = a;
        }
        const unsigned t = (mat && mat->texture >= 0 && static_cast<std::size_t>(mat->texture) < tex.size()) ? tex[static_cast<std::size_t>(mat->texture)] : 0u;
        if (t) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, t);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, x2 ? 2.f : 1.f);
        } else {
            glDisable(GL_TEXTURE_2D);
            for (std::uint32_t v = 0; v < nv; ++v) for (int c = 0; c < 3; ++c) col[v * 4 + static_cast<std::uint32_t>(c)] = std::min(1.f, col[v * 4 + static_cast<std::uint32_t>(c)] * (x2 ? 2.f : 1.f));
        }
        glVertexPointer(3, GL_FLOAT, 0, pos.data());
        glColorPointer(4, GL_FLOAT, 0, col.data());
        if (!m.uv.empty()) { glEnableClientState(GL_TEXTURE_COORD_ARRAY); glTexCoordPointer(2, GL_FLOAT, 0, m.uv.data()); }
        else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m.idx.size()), GL_UNSIGNED_INT, m.idx.data());
    }
    glPopMatrix();
}

}  // namespace

void actor_gl_upload(const Actor& a, ActorGL& g, float aniso) {
    actor_gl_release(g);
    g.part_tex.resize(a.parts.size());
    for (std::size_t i = 0; i < a.parts.size(); ++i) upload_scene(a.parts[i].scene, g.part_tex[i], aniso);
    if (a.has_weapon) upload_scene(a.weapon.scene, g.weapon_tex, aniso);
    g.uploaded = true;
}

void actor_gl_release(ActorGL& g) {
    for (auto& v : g.part_tex) for (unsigned t : v) if (t) { GLuint id = t; glDeleteTextures(1, &id); }
    for (unsigned t : g.weapon_tex) if (t) { GLuint id = t; glDeleteTextures(1, &id); }
    g.part_tex.clear();
    g.weapon_tex.clear();
    g.uploaded = false;
}

void actor_gl_draw(const Actor& a, const ActorGL& g, const ActorLight& L, bool blend_pass) {
    if (!g.uploaded) return;
    gl_single_texture_unit();
    // v21: matriz de textura identidade (defesa: nenhum estado de outro desenho pode deslocar as UVs dos personagens)
    glMatrixMode(GL_TEXTURE); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glDisable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);                              // dupla face: cabelos/barbas sao folhas, e os espelhos (Z do modelo, X da projecao) se anulam
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (blend_pass) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); glDisable(GL_ALPHA_TEST); }
    else { glDisable(GL_BLEND); glDepthMask(GL_TRUE); glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f); }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    float m[16];
    a.model_matrix(m);
    for (std::size_t i = 0; i < a.parts.size() && i < g.part_tex.size(); ++i) draw_part(a.parts[i], g.part_tex[i], m, L, blend_pass);
    if (a.has_weapon) { float w[16]; a.weapon_matrix(w); draw_part(a.weapon, g.weapon_tex, w, L, blend_pass); }
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.f);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1, 1, 1, 1);
}

}  // namespace f3v
