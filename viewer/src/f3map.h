// Leitor do formato .f3map (mapa decodificado: grade + geometria + entidades + navegacao...). Ver tools/py/f3vb/mapexport.py para o layout.
// Portatil (so C++ padrao), com checagem de limites em tudo: arquivos corrompidos sao rejeitados, nunca derrubam o programa.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace f3v {

using Vec3f = std::array<float, 3>;

// uv = coordenadas da textura difusa, lmuv = do lightmap (2 floats por vertice; so no .f3map v3, vazios nas versoes antigas)
struct MapBlock { std::uint32_t nv = 0; std::vector<float> xyz, nrm; std::vector<std::uint8_t> rgba; std::vector<float> uv, lmuv; };
// Lista de triangulos de um bloco com o MESMO material / pagina de lightmap / textura difusa (v2; no v1 os tres ficam 0).
// material = indice 0-based em MapData::materials; page = 1-based na lista ".ctx" (0 = nenhuma); tex = 1-based na lista ".tga" (0 = nenhuma).
struct MapTriList { std::uint32_t block = 0, material = 0, page = 0, tex = 0; std::vector<std::uint32_t> idx; };
struct MapMaterial { std::uint32_t id = 0; std::string name; std::uint8_t rgba[4] = {255, 255, 255, 255}; std::string blend, surface; std::uint32_t flags = 0; };
struct MapEntity { std::string kind, prototype, instance, name, script, skin; Vec3f pos{}; float yaw = 0; };
struct MapNavNode { std::uint8_t kind = 0; Vec3f pos{}; std::vector<std::uint8_t> links; };
struct MapEntry { std::uint8_t id = 0; std::vector<Vec3f> pos; std::vector<float> angles; };
struct MapTrigger { std::uint8_t kind = 0; std::string target; std::int32_t entry = -1; std::vector<Vec3f> pts; };   // 0 regiao, 1 script, 2 viagem, 3 vinculo
struct MapPath { std::string name; std::vector<Vec3f> pts; };
struct MapSound { std::string name; Vec3f pos{}; std::string sound; };
struct MapMarker { Vec3f pos{}; std::string icon; std::uint32_t text_id = 0; };
struct MapEffect { std::string name; Vec3f pos{}; std::string effect; };
struct MapWater { Vec3f pos{}; std::string texture; };
struct MapLight { std::uint32_t type = 0; Vec3f color{}, pos{}, dir{}; };

struct MapData {
    std::uint32_t version = 0;               // versao do arquivo lido (1 ou 2)
    std::string name, tre, rle;
    float origin[2] = {0, 0};
    float cell = 0.5f;                       // lado da celula da grade, em unidades do mundo
    std::uint32_t grid_w = 0, grid_rows = 0;
    std::vector<std::uint8_t> grid;          // grid_w * grid_rows; 0x2F = vazio
    std::vector<MapBlock> blocks;
    std::vector<MapTriList> tris;
    std::vector<MapEntity> entities;
    std::vector<MapNavNode> nav;
    std::vector<MapEntry> entries;
    std::vector<MapTrigger> triggers;
    std::vector<MapPath> paths;
    std::vector<MapSound> sounds;
    std::vector<MapMarker> markers;
    std::vector<MapEffect> effects;
    std::vector<MapWater> water;
    std::vector<MapLight> lights;
    // v19: ECAM do .map (limites da camera: xmin, zmax, xmax, zmin; padrao 0,1000,1000,0) e cabecalho de ambiente (cor A, cor B, floats 250/500/1)
    bool has_camera = false; float camera[4] = {0, 1000, 1000, 0};
    bool has_env = false; std::uint8_t env_a[4] = {0, 0, 0, 0}, env_b[4] = {0, 0, 0, 0}; float env_f[3] = {0, 0, 0};
    std::vector<MapMaterial> materials;      // v2 (MATL)
    std::vector<std::string> textures;       // v2 (TEXN): nomes na ordem do .8tre (".tga" difusas, ".ctx" paginas de lightmap)
    std::vector<std::string> tga_names, ctx_names;   // derivadas de textures (preenchidas ao carregar)

    std::size_t triangle_count() const;
    // nomes (sem extensao) das listas de textura: k e p sao 1-based como em MapTriList; "" quando 0 ou fora da lista
    std::string diffuse_name(std::uint32_t k) const { return k >= 1 && k <= tga_names.size() ? tga_names[k - 1] : std::string(); }
    std::string lightmap_name(std::uint32_t p) const { return p >= 1 && p <= ctx_names.size() ? ctx_names[p - 1] : std::string(); }
    // Caixa de tudo que tem posicao (geometria, entidades, navegacao...). false se nao ha nada.
    bool bounds(float mn[3], float mx[3]) const;
    // Regra medida: celula = (mundo - origem) / tamanho da celula. false se cair fora da grade.
    bool cell_of(const Vec3f& p, int& col, int& row) const;
    bool on_floor(const Vec3f& p) const;     // celula valida e nao vazia
    // Quantos pontos (entidades, nos de navegacao, entradas) caem em celulas nao vazias.
    void floor_hits(std::size_t& hits, std::size_t& total) const;
};

bool load_f3map(const std::uint8_t* data, std::size_t size, MapData& out, std::string& err);
bool load_f3map_file(const std::string& path, MapData& out, std::string& err);
void map_split_texture_names(MapData& m);   // v20: preenche tga_names/ctx_names a partir de textures (usado tambem pelo map_build)

}  // namespace f3v
