// v21: chao do mapa a partir dos triangulos do .8tre (MapData): altura por raio vertical, clique do mouse no chao, passo/colisao e caminho (A*).
// Portavel (sem Windows/OpenGL). Unidades do mapa (o personagem humano tem ~6 de altura).
//
// Regras (todas medidas ou escolhidas aqui, documentadas em docs/JOGO.md):
//  - altura(x, z, ref): entre as superficies que passam pelo ponto, a MAIS ALTA que fica ate `kStepUp` acima de `ref` (degrau que da para subir).
//    Assim um personagem na rua nao "sobe" no telhado de um predio vizinho, mas sobe uma calcada.
//  - Triangulos quase verticais (paredes) nao contam como chao.
//  - Passo bloqueado: o chao no destino nao existe, ou fica mais de kStepUp acima / kStepDown abaixo da altura atual.
//  - Caminho: A* numa grade de `cell` unidades sobre a caixa do mapa, testando o passo entre centros vizinhos (8 direcoes).
#pragma once
#include <cstdint>
#include <vector>

#include "f3map.h"

namespace f3v {

class Ground {
public:
    static constexpr float kStepUp = 2.0f, kStepDown = 6.0f;

    void build(const MapData& m, float cell = 4.f);
    bool empty() const { return tris_.empty(); }
    std::size_t triangles() const { return tris_.size(); }
    void bounds(float& x0, float& z0, float& x1, float& z1) const { x0 = x0_; z0 = z0_; x1 = x1_; z1 = z1_; }

    // Altura do chao em (x, z) ate kStepUp acima de ref. false = nao ha chao ali.
    bool height(float x, float z, float ref, float& out) const;
    // Altura mais alta em (x, z) (sem limite): para o clique do mouse e para pousar entidades soltas.
    bool top(float x, float z, float& out) const;
    // Raio da camera contra o chao (primeiro ponto onde o raio desce abaixo da superficie). false = nao acertou.
    bool pick(const float origin[3], const float dir[3], float hit[3]) const;
    // Da para ir de (x0,z0) a (x1,z1) a pe, partindo da altura y0? (amostra a cada 0,5 unidade)
    bool walkable(float x0, float z0, float y0, float x1, float z1, float* y_end = nullptr) const;
    // Caminho a pe (pontos x,z; o ultimo e o destino). false = sem caminho (fica vazio).
    bool path(float sx, float sz, float sy, float tx, float tz, std::vector<float>& out_xz, int max_nodes = 60000) const;

private:
    struct Tri { float a[3], b[3], c[3]; };
    std::vector<Tri> tris_;
    std::vector<std::vector<std::uint32_t>> cells_;
    float x0_ = 0, z0_ = 0, x1_ = 0, z1_ = 0, cell_ = 4;
    int nx_ = 0, nz_ = 0;
    template <class F> void each_at(float x, float z, F&& f) const;
};

}  // namespace f3v
