// Leitor do formato F3M (ver tools/py/f3vb/f3m.py para a especificacao). Portavel: nao depende de Windows/OpenGL.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace f3v {

enum : std::uint32_t { kFlagModel = 1, kFlagSkeleton = 2, kFlagClips = 4 };

struct Texture { std::string name; std::uint32_t w = 0, h = 0; std::vector<std::uint8_t> rgba; };
struct Material { std::string name; int texture = -1; float color[4] = {1, 1, 1, 1}; std::uint8_t alpha_mode = 0, double_sided = 0;
                  std::string shader; };   // v20: id do material no B3D (BASE_2X, BASE_VERTEX_ALPHA_2X, BASE_IMAGE_ALPHA_2X...); nao vai para o arquivo
struct Mesh {
    std::string name;
    int material = -1;
    std::vector<std::string> bones;                 // nomes dos ossos citados em `joints`
    bool has_skin = false;
    std::vector<float> pos, nrm, uv, col;           // nv*3, nv*3, nv*2, nv*4
    std::vector<std::uint16_t> joints;              // nv*4 (indices em `bones`)
    std::vector<float> weights;                     // nv*4
    std::vector<std::uint32_t> idx;
    std::uint32_t nv() const { return static_cast<std::uint32_t>(pos.size() / 3); }
};
struct Joint { std::string name; int parent = -1; float t[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1}; float ibm[16]; };
struct Channel { std::string bone; std::uint8_t path = 0; std::vector<float> times, values; };   // 0 T, 1 R, 2 S
struct Clip { std::string name; float duration = 0; std::vector<Channel> channels; };

struct F3MFile {
    std::uint32_t flags = 0;
    std::string name;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Joint> joints;
    std::vector<Clip> clips;
};

// Devolve false e preenche `error` em qualquer problema (nunca le fora do buffer).
bool load_f3m(const std::uint8_t* data, std::size_t size, F3MFile& out, std::string& error);
bool load_f3m_file(const std::string& path, F3MFile& out, std::string& error);

}  // namespace f3v
