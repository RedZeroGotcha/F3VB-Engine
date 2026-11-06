// Testes do leitor .f3map e do prefixo do log. Uso: test_f3map PASTA_DO_FIXTURE   (make_map_fixture.py gera os arquivos)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "../src/f3map.h"
#include "../src/f3tex.h"
#include "../src/log.h"

using namespace f3v;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static std::vector<std::uint8_t> slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("uso: test_f3map PASTA_DO_FIXTURE"); return 2; }
    const std::string dir = argv[1];
    // --- arquivo completo
    MapData m;
    std::string err;
    CHECK(load_f3map_file(dir + "/teste.f3map", m, err));
    CHECK(m.name == "Teste" && m.tre == "T.8" && m.rle == "T.rle");
    CHECK(m.cell == 0.5f && m.origin[0] == 0.f && m.origin[1] == 0.f);
    CHECK(m.grid_w == 30 && m.grid_rows == 26 && m.grid.size() == 30u * 26u);
    CHECK(m.blocks.size() == 1 && m.blocks[0].nv == 4 && m.blocks[0].xyz.size() == 12 && m.blocks[0].rgba.size() == 16);
    CHECK(m.tris.size() == 1 && m.tris[0].idx.size() == 6 && m.triangle_count() == 2);
    CHECK(m.entities.size() == 1 && m.entities[0].kind == "CRT" && m.entities[0].script == "Npc.amx" && m.entities[0].skin == "Skin_Test.dds");
    CHECK(m.entities[0].pos[0] == 10.f && m.entities[0].pos[2] == 20.f);
    CHECK(m.nav.size() == 2 && m.nav[0].links.size() == 1 && m.nav[0].links[0] == 1);
    CHECK(m.entries.size() == 1 && m.entries[0].pos.size() == 6 && m.entries[0].angles.size() == 6);
    CHECK(m.triggers.size() == 2 && m.triggers[0].kind == 2 && m.triggers[0].target == "Other_Map" && m.triggers[0].entry == 2 && m.triggers[0].pts.size() == 4);
    CHECK(m.triggers[1].kind == 1 && m.triggers[1].target == "Door.amx");
    CHECK(m.paths.size() == 1 && m.paths[0].name == "Waypoint01" && m.paths[0].pts.size() == 2);
    CHECK(m.sounds.size() == 1 && m.sounds[0].sound == "AMB_Wind.psf");
    CHECK(m.markers.size() == 1 && m.markers[0].text_id == 1906 && m.markers[0].icon == "Icon.dds");
    CHECK(m.effects.size() == 1 && m.effects[0].effect == "FX_Dust.veg");
    CHECK(m.water.size() == 1 && m.water[0].texture == "W_0.TGA");
    CHECK(m.lights.size() == 1 && m.lights[0].type == 1);
    // --- v2: material / pagina / textura nas listas, tabela de materiais e nomes de textura
    CHECK(m.version == 3 && m.materials.size() == 1 && m.materials[0].name == "SK_BASE" && m.materials[0].blend == "OPAQUE" && m.materials[0].surface == "METAL");
    CHECK(m.materials[0].rgba[0] == 127 && m.materials[0].rgba[3] == 255);
    CHECK(m.textures.size() == 2 && m.tga_names.size() == 1 && m.tga_names[0] == "a" && m.ctx_names.size() == 1 && m.ctx_names[0] == "lm_0");
    CHECK(m.tris[0].material == 0 && m.tris[0].page == 0 && m.tris[0].tex == 0);
    CHECK(m.diffuse_name(1) == "a" && m.diffuse_name(0).empty() && m.diffuse_name(9).empty() && m.lightmap_name(1) == "lm_0" && m.lightmap_name(2).empty());
    // --- v1 (formato do v15) continua legivel
    MapData m1;
    CHECK(load_f3map_file(dir + "/v1.f3map", m1, err));
    CHECK(m1.version == 1 && m1.materials.empty() && m1.textures.empty() && m1.tga_names.empty() && m1.triangle_count() == 2 && m1.tris[0].material == 0 && m1.tris[0].page == 0);
    // --- v2 com lotes
    MapData ml;
    CHECK(load_f3map_file(dir + "/lotes.f3map", ml, err));
    CHECK(ml.version == 3 && ml.tris.size() == 4 && ml.triangle_count() == 5 && ml.materials.size() == 3 && ml.tga_names.size() == 2 && ml.ctx_names.size() == 2);
    CHECK(ml.tris.size() == 4 && ml.tris[0].material == 0 && ml.tris[0].idx.size() == 6);
    CHECK(ml.tris[1].material == 1 && ml.tris[1].page == 2 && ml.tris[1].tex == 1);
    CHECK(ml.tris[2].material == 1 && ml.tris[2].page == 2 && ml.tris[2].tex == 2);
    CHECK(ml.tris[3].material == 2 && ml.tris[3].page == 1 && ml.tris[3].tex == 0);
    CHECK(ml.materials[2].name == "SK_MAT2" && ml.diffuse_name(2) == "b" && ml.lightmap_name(2) == "lm_1");

    // --- v3: UVs por vertice (difusa e lightmap); v2 e v1 continuam legiveis sem UVs
    CHECK(m.version == 3 && m.blocks[0].uv.size() == static_cast<std::size_t>(m.blocks[0].nv) * 2 && m.blocks[0].lmuv.size() == static_cast<std::size_t>(m.blocks[0].nv) * 2);
    CHECK(m.blocks[0].nv == 4 && m.blocks[0].uv[2] == 1.f && m.blocks[0].uv[3] == 0.f && m.blocks[0].uv[6] == 1.f && m.blocks[0].uv[7] == 1.f);
    {
        MapData m2;
        CHECK(load_f3map_file(dir + "/v2.f3map", m2, err));
        CHECK(m2.version == 2 && m2.blocks[0].uv.empty() && m2.blocks[0].lmuv.empty() && m2.triangle_count() == 2 && m2.textures.size() == 2);
    }
    // --- .f3tex e mipmaps
    {
        std::vector<std::uint8_t> t = {'F', '3', 'T', 'X', 3, 0, 0, 0, 2, 0, 0, 0};
        for (int i = 0; i < 3 * 2 * 4; ++i) t.push_back(static_cast<std::uint8_t>(i * 10));
        f3v::Image im;
        CHECK(f3v::load_f3tex(t.data(), t.size(), im, err) && im.w == 3 && im.h == 2 && im.rgba.size() == 24 && im.rgba[4] == 40);
        const std::vector<f3v::Image> mips = f3v::build_mips(im);
        CHECK(mips.size() == 2 && mips[0].w == 3 && mips[1].w == 1 && mips[1].h == 1 && mips[1].rgba.size() == 4);
        CHECK(mips[1].rgba[0] == static_cast<std::uint8_t>((0 + 40 + 120 + 160 + 2) / 4));              // media 2x2 do canto (borda repete o ultimo pixel)
        std::vector<std::uint8_t> bad = t;
        bad.pop_back();
        CHECK(!f3v::load_f3tex(bad.data(), bad.size(), im, err));
        bad = t; bad[0] = 'X';
        CHECK(!f3v::load_f3tex(bad.data(), bad.size(), im, err));
        bad = t; bad[7] = 0x7f;                                                                           // largura gigante
        CHECK(!f3v::load_f3tex(bad.data(), bad.size(), im, err));
        CHECK(!f3v::load_f3tex(t.data(), 5, im, err));
        f3v::Image one; one.w = 1; one.h = 1; one.rgba = {1, 2, 3, 4};
        CHECK(f3v::build_mips(one).size() == 1);
        CHECK(f3v::build_mips(f3v::Image()).empty());
        std::mt19937 rng2(5);
        for (int it = 0; it < 2000; ++it) {                                                               // fuzz do carregador
            std::vector<std::uint8_t> b = t;
            b[rng2() % b.size()] = static_cast<std::uint8_t>(rng2());
            if (it % 5 == 0) b.resize(rng2() % b.size());
            f3v::Image f;
            if (f3v::load_f3tex(b.data(), b.size(), f, err)) (void)f3v::build_mips(f);
        }
    }
    // --- regra celula = (mundo - origem) / 0,5
    int c = -1, r = -1;
    CHECK(m.cell_of({10.f, 0.f, 6.f}, c, r) && c == 20 && r == 12);
    CHECK(!m.cell_of({-1.f, 0.f, 6.f}, c, r) && !m.cell_of({1000.f, 0.f, 6.f}, c, r));
    CHECK(m.on_floor({10.f, 0.f, 6.f}));          // miolo preenchido da grade sintetica
    CHECK(!m.on_floor({0.2f, 0.f, 0.2f}));        // canto vazio
    std::size_t hits = 0, total = 0;
    m.floor_hits(hits, total);
    CHECK(total > 0 && hits <= total);
    float mn[3], mx[3];
    CHECK(m.bounds(mn, mx) && mn[0] <= 0.f && mx[0] >= 10.f);
    // --- so o mapa (sem grade nem geometria)
    MapData m2;
    CHECK(load_f3map_file(dir + "/so_mapa.f3map", m2, err));
    CHECK(m2.grid.empty() && m2.blocks.empty() && m2.entities.size() == 1);
    CHECK(!m2.on_floor({10.f, 0.f, 6.f}));
    // --- erros
    const std::vector<std::uint8_t> good = slurp(dir + "/teste.f3map");
    MapData bad;
    CHECK(!load_f3map(good.data(), 7, bad, err));
    std::vector<std::uint8_t> g2 = good; g2[0] = 'X';
    CHECK(!load_f3map(g2.data(), g2.size(), bad, err));
    CHECK(!load_f3map(good.data(), good.size() - 5, bad, err));
    std::vector<std::uint8_t> g3 = good; g3.push_back(0);                  // sobra no fim
    CHECK(!load_f3map(g3.data(), g3.size(), bad, err));
    CHECK(!load_f3map_file(dir + "/nao_existe.f3map", bad, err));
    // --- fuzz: nunca pode quebrar
    std::mt19937 rng(2026);
    int loaded = 0;
    for (int it = 0; it < 4000; ++it) {
        std::vector<std::uint8_t> b = good;
        const int k = 1 + static_cast<int>(rng() % 6);
        for (int q = 0; q < k; ++q) b[rng() % b.size()] = static_cast<std::uint8_t>(rng());
        if (it % 7 == 0) b.resize(rng() % b.size());
        MapData f;
        std::string e;
        if (load_f3map(b.data(), b.size(), f, e)) {
            ++loaded;
            std::size_t h = 0, t = 0;
            f.floor_hits(h, t);
            float a[3], z[3];
            (void)f.bounds(a, z);
            (void)f.triangle_count();
        }
    }
    std::printf("fuzz do .f3map: %d de 4000 mutacoes ainda carregaram (e foram usadas sem quebrar)\n", loaded);
    for (const char* nm2 : {"/lotes.f3map", "/v2.f3map", "/v1.f3map"}) {                // mesmo fuzz nos arquivos com lotes (v2) e no formato v1
        const std::vector<std::uint8_t> g = slurp(dir + nm2);
        int ok2 = 0;
        for (int it = 0; it < 3000; ++it) {
            std::vector<std::uint8_t> b = g;
            const int k = 1 + static_cast<int>(rng() % 6);
            for (int q = 0; q < k; ++q) b[rng() % b.size()] = static_cast<std::uint8_t>(rng());
            if (it % 7 == 0) b.resize(rng() % b.size());
            MapData f;
            std::string e;
            if (load_f3map(b.data(), b.size(), f, e)) {
                ++ok2;
                (void)f.triangle_count();
                (void)f.diffuse_name(1);
                (void)f.lightmap_name(1);
                for (const MapTriList& tl : f.tris) for (std::uint32_t x : tl.idx) if (x >= f.blocks[tl.block].nv) { std::printf("indice fora do bloco passou no fuzz\n"); ++fails; }
            }
        }
        std::printf("fuzz de %s: %d de 3000 mutacoes carregaram\n", nm2 + 1, ok2);
    }
    // --- prefixo do log
    const std::string nm = Log::file_name(1700000000, "f3mapviewer_log");
    CHECK(nm.rfind("f3mapviewer_log_", 0) == 0 && nm.substr(nm.size() - 4) == ".txt");
    CHECK(Log::file_name(1700000000) == Log::file_name(1700000000, "f3viewer_log"));
    Log lg;
    CHECK(lg.open(".", "f3mapviewer_log"));
    const std::string path = lg.path();
    CHECK(path.find("f3mapviewer_log_") != std::string::npos);
    lg.line("ok");
    lg.close();
    std::remove(path.c_str());
    if (fails) { std::printf("%d verificacao(oes) falharam\n", fails); return 1; }
    std::puts("f3map tests OK");
    return 0;
}
