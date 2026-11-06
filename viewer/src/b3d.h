// v20: leitor de .b3d (modelos G3D_Model da Jefferson Engine) em C++, direto da memoria (do .grp).
// Porta do tools/py/f3vb/b3d.py (layouts medidos em arquivos reais; docs/specs/b3d.md) + uma variante nova medida no v20:
//   superficie "64 com pesos" (ex.: IT_MainMenu_boxcar, o vagao do menu): o cabecalho da superficie NAO tem o byte de tipo de
//   vertice (vem u8, u8 e logo numVerts) e os ultimos 12 bytes do vertice de 64 bytes sao (s32 1, s32 osso, f32 peso) em vez da
//   UV de lightmap. O leitor Python recusava esse arquivo ("numVerts absurdo").
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace f3v {

struct B3dMaterial {
    std::string name, id, blend, type;
    std::vector<float> floats;                      // difusa4, ambiente4, especular4, emissiva4, brilho
    std::vector<std::string> textures;
};

struct B3dSurface {
    int family = 44;                                // 44 ou 64
    bool weighted64 = false;                        // variante "64 com pesos" (ver acima)
    std::vector<float> pos, nrm, col, uv0, uv1;     // nv*3, nv*3, nv*4 (0..1), nv*2, nv*2 (so familia 64 sem pesos)
    int bones_per_vertex = 0;
    std::vector<std::vector<std::pair<int, float>>> influences;   // por vertice: (osso, peso) com osso >= 0 e peso > 0
    std::vector<B3dMaterial> materials;
    std::string lightmap;
    struct Prim { int material = 0; std::vector<std::uint32_t> idx; };
    std::vector<Prim> prims;
    std::uint32_t nv() const { return static_cast<std::uint32_t>(pos.size() / 3); }
};

struct B3dNode {
    std::string name;
    bool has_t = false, has_r = false, has_s = false;
    float t[3] = {0, 0, 0}, r[4] = {0, 0, 0, 0}, s[3] = {1, 1, 1};   // r = eixo-angulo (graus)
    std::vector<B3dSurface> surfaces;
    std::vector<B3dNode> children;
};

struct B3dBone { std::string name; int unk = 0, flag = 0; float t[3] = {0, 0, 0}, r[4] = {0, 0, 0, 0}; float extra = 0; };

struct B3dModel {
    std::string signature, coordinate_system;
    float global_scale = 1, coordinate_scale = 0;
    std::vector<B3dMaterial> materials;
    std::vector<B3dBone> bones;
    std::vector<B3dNode> roots;
    template <class F> void each_surface(F&& f) const { for (const auto& r : roots) walk(r, f); }
    template <class F> static void walk(const B3dNode& n, F& f) { for (const auto& s : n.surfaces) f(n, s); for (const auto& c : n.children) walk(c, f); }
};

bool load_b3d(const std::uint8_t* data, std::size_t size, B3dModel& out, std::string& err);

}  // namespace f3v
