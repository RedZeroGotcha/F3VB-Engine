// v20: leitor de .8tre (geometria estatica do mapa) em C++, porte fiel de tools/py/f3vb/tre.py (spec: docs/specs/tre.md).
// Chunks = tag[4] + u32 tamanho do payload; raiz 8TRE com HEAD, LGTD, LVLD, MATD, TXTD, VTXD, TREE (NODE/LEAF em profundidade).
// IDXS de cada no: os 12 bytes iniciais sao o cabecalho do 1o lote ([0x8000|mat][A][0x4000|pagina][B][k][C], achado do v19) seguidos dos u16;
// o fluxo inteiro e lido pela gramatica de grupos/paginas/sublotes (split_full); se nao fechar, cai no modo antigo (split_batches, v16).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "f3map.h"

namespace f3v {

struct TreBatch { std::uint32_t material = 0, page = 0, texture = 0; std::vector<std::uint16_t> idx; };

struct TreNode {
    bool leaf = false;
    float bbox[6] = {0, 0, 0, 0, 0, 0};       // xmin, ymin, zmin, xmax, ymax, zmax
    std::uint32_t block = 0;
    std::vector<std::uint16_t> indices;       // u16 depois dos 12 bytes do cabecalho
    std::uint16_t head[6] = {0, 0, 0, 0, 0, 0};
    bool has_head = false;
    std::size_t tri_count = 0;                // = tamanho do FLGS (1 byte por triangulo)
    std::vector<TreBatch> batches;
    bool parsed = true, full_grammar = false;
};

struct TreLight { std::uint32_t type = 0, flags = 0; float color[3] = {0, 0, 0}, pos[3] = {0, 0, 0}, dir[3] = {0, 0, 0}; };

struct TreData {
    std::uint32_t head[4] = {0, 0, 0, 0};
    std::vector<TreLight> lights;
    bool has_level = false;
    std::uint32_t level_w = 0, level_h = 0;
    float level_f[2] = {0, 0};                // o 1o e o lado da celula da grade (0,5)
    std::vector<MapMaterial> materials;
    std::vector<std::string> textures;        // ".tga" = difusas, ".ctx" = paginas de lightmap (ordem do arquivo)
    std::vector<MapBlock> blocks;
    std::vector<TreNode> nodes;
    bool bounds(float mn[3], float mx[3]) const;
};

struct TreStats { std::size_t nodes = 0, nodes_full = 0, unparsed = 0, indices = 0, outside_bbox = 0, masked = 0, bad = 0; };

bool read_tre(const std::uint8_t* data, std::size_t size, TreData& out, std::string& err);
// triangulos agrupados por (bloco, material, pagina, textura) em ordem crescente da chave, como o build_f3map do Python
void tre_triangle_lists(const TreData& t, std::vector<MapTriList>& out, TreStats* stats = nullptr, float eps = 0.6f);

}  // namespace f3v
