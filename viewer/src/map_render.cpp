#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include "map_render.h"

#include <algorithm>
#include <cmath>

#include "f3tex.h"

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
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
#define GL_PREVIOUS 0x8578
#define GL_PRIMARY_COLOR 0x8577
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

namespace f3v {

namespace {

typedef void(APIENTRY* PFN_ActiveTex)(GLenum);
PFN_ActiveTex p_active = nullptr, p_client_active = nullptr;
bool g_mtex_tried = false;

void unit(int u) { if (p_active) { p_active(u ? GL_TEXTURE1 : GL_TEXTURE0); p_client_active(u ? GL_TEXTURE1 : GL_TEXTURE0); } }

float g_aniso = 1.f;
unsigned upload(const Image& img, bool clamp) {
    const std::vector<Image> mips = build_mips(img);
    if (mips.empty()) return 0;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (g_aniso > 1.f) glTexParameterf(GL_TEXTURE_2D, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */, g_aniso);
    for (std::size_t l = 0; l < mips.size(); ++l)
        glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(l), GL_RGBA, static_cast<GLsizei>(mips[l].w), static_cast<GLsizei>(mips[l].h), 0, GL_RGBA, GL_UNSIGNED_BYTE, mips[l].rgba.data());
    return id;
}

// unidade ativa: COMBINE com fonte0 = a, fonte1 = b, modulando, com escala
void combine_modulate(GLenum src0, GLenum src1, int scale) {
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, static_cast<GLint>(src0));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, static_cast<GLint>(src1));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, static_cast<GLint>(src0));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, static_cast<GLint>(src1));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, scale);
}

}  // namespace

unsigned gl_upload_texture(const Image& img, bool clamp, float aniso) { g_aniso = aniso; return upload(img, clamp); }
void gl_single_texture_unit() {
    if (p_active) { unit(1); glDisable(GL_TEXTURE_2D); glDisableClientState(GL_TEXTURE_COORD_ARRAY); unit(0); }
}

bool gl_init_multitexture() {
    if (!g_mtex_tried) {
        g_mtex_tried = true;
        p_active = reinterpret_cast<PFN_ActiveTex>(reinterpret_cast<void*>(wglGetProcAddress("glActiveTexture")));
        p_client_active = reinterpret_cast<PFN_ActiveTex>(reinterpret_cast<void*>(wglGetProcAddress("glClientActiveTexture")));
        if (!p_active || !p_client_active) p_active = p_client_active = nullptr;
    }
    return p_active != nullptr;
}

unsigned MapRenderer::get_texture(const std::string& name, bool clamp) {
    std::string key;
    for (char c : name) key.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    const auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    std::string file;
    for (char c : name) file.push_back((c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') ? '_' : c);
    Image img;
    std::string err;
    unsigned id = 0;
    g_aniso = aniso_;
    if (tex_loader_) { if (tex_loader_(name, img)) id = upload(img, clamp); }
    else if (!tex_dir_.empty() && load_f3tex_file(tex_dir_ + "\\" + file + ".f3tex", img, err)) id = upload(img, clamp);
    cache_[key] = id;
    return id;
}

bool MapRenderer::load(const std::string& path, const std::string& tex_dir, Log* log, std::string& err) {
    release();
    if (!load_f3map_file(path, map_, err)) return false;
    path_ = path;
    tex_dir_ = tex_dir;
    tex_loader_ = nullptr;
    return finish_load(log);
}

bool MapRenderer::load_data(MapData&& data, TexLoader tex, Log* log, std::string& err) {
    release();
    (void)err;
    map_ = std::move(data);
    path_ = "demo:" + map_.name;
    tex_dir_.clear();
    tex_loader_ = std::move(tex);
    return finish_load(log);
}

bool MapRenderer::finish_load(Log* log) {
    mtex_ = gl_init_multitexture();
    const Image white{1, 1, {255, 255, 255, 255}};
    white_ = upload(white, false);
    found_ = missing_ = 0;
    std::string miss;
    for (const std::string& n : map_.tga_names) { const unsigned id = get_texture(n, false); diffuse_.push_back(id); if (id) ++found_; else { ++missing_; if (missing_ <= 6) miss += " " + n; } }
    for (const std::string& n : map_.ctx_names) { const unsigned id = get_texture(n, true); lightmap_.push_back(id); if (id) ++found_; else { ++missing_; if (missing_ <= 6) miss += " " + n + "(lm)"; } }
    sky_tl_.assign(map_.tris.size(), 0);
    for (std::size_t k = 0; k < map_.tris.size(); ++k) {                  // parede de ceu: plana e enorme
        const MapTriList& tl = map_.tris[k];
        if (tl.block >= map_.blocks.size() || tl.idx.empty() || tl.tex == 0) continue;
        const MapBlock& b = map_.blocks[tl.block];
        float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
        for (std::uint32_t i : tl.idx)
            for (int c = 0; c < 3; ++c) { const float v = b.xyz[static_cast<std::size_t>(i) * 3 + static_cast<std::size_t>(c)]; mn[c] = std::min(mn[c], v); mx[c] = std::max(mx[c], v); }
        const float ex = mx[0] - mn[0], ey = mx[1] - mn[1], ez = mx[2] - mn[2];
        const bool flat = std::min(ex, ez) < 1.f, huge = std::max(ex, ez) > 120.f && ey > 30.f;
        sky_tl_[k] = flat && huge ? 1 : 0;
    }
    loaded_ = true;
    if (log) {
        log->linef("mapa '%s': %zu blocos, %zu triangulos, %zu listas, %zu entidades | texturas %zu ok, %zu faltando%s | multitextura %s | UVs %s",
                   map_.name.c_str(), map_.blocks.size(), map_.triangle_count(), map_.tris.size(), map_.entities.size(), found_, missing_, miss.c_str(),
                   mtex_ ? "sim" : "nao", (!map_.blocks.empty() && !map_.blocks[0].uv.empty()) ? "sim (.f3map v3)" : "NAO (reconverta)");
        if (!tex_loader_) {
            std::size_t no_state = 0;
            for (const MapTriList& tl : map_.tris) if (tl.material == 0 && tl.page == 0 && tl.tex == 0) no_state += tl.idx.size() / 3;
            if (no_state) log->linef("aviso: %zu triangulos com material 0 sem pagina/textura (normal em alguns mapas de teste; em Mainmenu/Junktown indica .f3map de antes do v19)", no_state);
        }
    }
    return true;
}

void MapRenderer::release() {
    for (auto& kv : cache_) if (kv.second) { GLuint id = kv.second; glDeleteTextures(1, &id); }
    if (white_) { GLuint id = white_; glDeleteTextures(1, &id); }
    cache_.clear(); diffuse_.clear(); lightmap_.clear();
    white_ = 0;
    loaded_ = false;
    map_ = MapData();
}

bool MapRenderer::bounds(float mn[3], float mx[3]) const {
    bool any = false;
    for (int k = 0; k < 3; ++k) { mn[k] = 1e30f; mx[k] = -1e30f; }
    for (const MapBlock& b : map_.blocks)
        for (std::size_t i = 0; i + 2 < b.xyz.size(); i += 3) {
            any = true;
            for (int k = 0; k < 3; ++k) { mn[k] = std::min(mn[k], b.xyz[i + k]); mx[k] = std::max(mx[k], b.xyz[i + k]); }
        }
    return any;
}

void MapRenderer::draw(bool lightmaps, bool wire) const {
    if (!loaded_ || map_.tris.empty() || map_.blocks.empty() || map_.blocks[0].uv.empty()) return;
    const bool lm = lightmaps && mtex_;
    glPolygonMode(GL_FRONT_AND_BACK, wire ? GL_LINE : GL_FILL);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    for (int pass = 0; pass < 2; ++pass) {
        std::size_t k = static_cast<std::size_t>(-1);
        if (pass == 1) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
        for (const MapTriList& tl : map_.tris) {
            ++k;
            const MapMaterial* mat = tl.material < map_.materials.size() ? &map_.materials[tl.material] : nullptr;
            const bool blended = mat && mat->blend == "ALPHABLEND";
            if (blended != (pass == 1)) continue;
            const MapBlock& b = map_.blocks[tl.block];
            if (b.uv.empty()) continue;
            const bool cutout = mat && mat->name.find("IMAGE_ALPHA") != std::string::npos;
            const unsigned dtex = tl.tex >= 1 && tl.tex <= diffuse_.size() ? diffuse_[tl.tex - 1] : 0;
            const unsigned ltex = lm && tl.page >= 1 && tl.page <= lightmap_.size() ? lightmap_[tl.page - 1] : 0;
            glVertexPointer(3, GL_FLOAT, 0, b.xyz.data());
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, b.rgba.data());
            unit(0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, dtex ? dtex : white_);
            // v20: o "x2" so vale para os materiais com _2X no nome (BASE_2X, BASE_LM_2X, SK_BASE_LM_2X); NO_TEXTURE* usa a cor do
            // vertice como esta (medido no menu: as alavancas dos trilhos saiam 1,8x mais claras que no original)
            combine_modulate(0x1702 /* GL_TEXTURE */, GL_PRIMARY_COLOR, (mat && mat->name.find("_2X") == std::string::npos) ? 1 : 2);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            const bool sky = k < sky_tl_.size() && sky_tl_[k] && (sky_u_per_s != 0 || sky_v_per_s != 0);
            if (sky) {
                // v21: as nuvens andam mudando as UVs NA CPU (so os vertices da parede de ceu). O v20 usava a matriz de textura do OpenGL,
                // e na placa NVIDIA do autor ela "vazava" para os personagens (texturas deslocadas: mais escuros e "envelhecidos").
                const float du = std::fmod(sky_time * sky_u_per_s, 1.f), dv = std::fmod(sky_time * sky_v_per_s, 1.f);
                sky_uv_.assign(b.uv.begin(), b.uv.end());
                for (std::uint32_t i : tl.idx) { sky_uv_[static_cast<std::size_t>(i) * 2] = b.uv[static_cast<std::size_t>(i) * 2] + du; sky_uv_[static_cast<std::size_t>(i) * 2 + 1] = b.uv[static_cast<std::size_t>(i) * 2 + 1] + dv; }
                glTexCoordPointer(2, GL_FLOAT, 0, sky_uv_.data());
            } else glTexCoordPointer(2, GL_FLOAT, 0, b.uv.data());
            if (mtex_) {
                unit(1);
                if (ltex && !b.lmuv.empty()) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, ltex);
                    combine_modulate(GL_PREVIOUS, 0x1702, 2);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glTexCoordPointer(2, GL_FLOAT, 0, b.lmuv.data());
                } else { glDisable(GL_TEXTURE_2D); glDisableClientState(GL_TEXTURE_COORD_ARRAY); }
                unit(0);
            }
            if (cutout) { glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f); } else glDisable(GL_ALPHA_TEST);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(tl.idx.size()), GL_UNSIGNED_INT, tl.idx.data());
        }
    }
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    if (mtex_) { unit(1); glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1); glDisable(GL_TEXTURE_2D); glDisableClientState(GL_TEXTURE_COORD_ARRAY); }
    unit(0);
    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void MapRenderer::draw_entity_markers(int selected) const {
    if (!loaded_) return;
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glLineWidth(2.f);
    glBegin(GL_LINES);
    for (std::size_t i = 0; i < map_.entities.size(); ++i) {
        const MapEntity& e = map_.entities[i];
        const bool crit = e.kind.find("CRT") != std::string::npos || e.kind.find("Crit") != std::string::npos || e.prototype.find(".CRT") != std::string::npos ||
                          e.prototype.find(".crt") != std::string::npos;
        if (static_cast<int>(i) == selected) glColor3f(1.f, 1.f, 1.f);
        else if (crit) glColor3f(0.95f, 0.35f, 0.25f);
        else glColor3f(0.95f, 0.85f, 0.3f);
        const float h = crit ? 1.8f : 0.8f;
        glVertex3f(e.pos[0], e.pos[1], e.pos[2]);
        glVertex3f(e.pos[0], e.pos[1] + h, e.pos[2]);
    }
    glEnd();
    glLineWidth(1.f);
}

}  // namespace f3v
