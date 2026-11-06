// Desenho de um mapa (.f3map) com as texturas da demo - v19. Win32 + OpenGL 1.x (multitextura por glActiveTexture, se houver).
//
// Formula de cor (medida nos dados reais, ver docs/JOGO.md "Cores dos materiais"):
//   - Toda a luz da demo e "assada": nos materiais com lightmap (_LM_) a cor do vertice e SEMPRE 127 (100% dos vertices medidos) e a luz vem do lightmap;
//     nos outros (BASE_2X, NO_TEXTURE...) a luz esta na cor do vertice (mediana ~30, bem escura). Por isso o OpenGL NAO acende luz nenhuma aqui.
//   - Cor do vertice 127 = neutro: unidade 0 = textura difusa (ou branco) x cor do vertice x 2; unidade 1 = x lightmap x 2 (o "_LM_2X").
//   - ALPHABLEND: desenhado depois, com mistura por alfa (alfa do vertice x alfa da textura). IMAGE_ALPHA: recorte de alfa 0,5 (folhagem).
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "f3map.h"
#include "f3tex.h"
#include "log.h"

namespace f3v {

class MapRenderer {
public:
    // Carrega o .f3map e as texturas de tex_dir (CONVERTED\textures\<nome>.f3tex). false = mapa nao abriu (err diz por que).
    bool load(const std::string& f3map_path, const std::string& tex_dir, Log* log, std::string& err);
    // v20: mapa ja montado em memoria (map_build) e texturas por um carregador (Vfs: TGA_Image pelo nome). Mesmo desenho do load().
    using TexLoader = std::function<bool(const std::string& name, Image& out)>;
    bool load_data(MapData&& data, TexLoader tex, Log* log, std::string& err);
    void set_anisotropy(float a) { aniso_ = a; }      // v20: filtro anisotropico (graficos modernos); 1 = desligado
    void release();                                   // apaga as texturas do OpenGL (precisa do contexto ativo)
    bool loaded() const { return loaded_; }
    const MapData& map() const { return map_; }
    const std::string& path() const { return path_; }

    // Desenha a geometria. lightmaps=false: so difusa x cor do vertice. Chame com a matriz de camera ja montada.
    void draw(bool lightmaps, bool wire) const;
    // v20: o "ceu" de fundo (no Mainmenu: uma parede de 188 x 74 unidades em z = 100, com a metade de nuvens do PS_MainMenu01).
    // No original as nuvens andam devagar para a esquerda (medido: ~34 px em 7,5 s a 800x600). Os campos do material sao iguais aos de
    // outro BASE_2X, entao o ceu e reconhecido pela GEOMETRIA: lote plano (espessura < 1) e enorme (lado > 120). Hipotese documentada.
    // sky_* = deslocamento da coordenada de textura (U, V) por segundo; o jogo poe o relogio em sky_time a cada quadro.
    float sky_time = 0, sky_u_per_s = 0, sky_v_per_s = 0;
    std::size_t sky_lists() const { std::size_t n = 0; for (char c : sky_tl_) n += c ? 1 : 0; return n; }
    // Marcadores simples das entidades (ate termos os modelos no mapa): pilar colorido por tipo.
    void draw_entity_markers(int selected) const;

    // Caixa do mapa (geometria) e centro; false se vazio.
    bool bounds(float mn[3], float mx[3]) const;
    std::size_t textures_found() const { return found_; }
    std::size_t textures_missing() const { return missing_; }
    bool multitexture() const { return mtex_; }

private:
    unsigned get_texture(const std::string& name, bool clamp);
    bool finish_load(Log* log);
    TexLoader tex_loader_;
    float aniso_ = 1.f;
    MapData map_;
    std::string path_, tex_dir_;
    bool loaded_ = false, mtex_ = false;
    unsigned white_ = 0;
    std::map<std::string, unsigned> cache_;
    std::vector<unsigned> diffuse_, lightmap_;
    std::vector<char> sky_tl_;
    mutable std::vector<float> sky_uv_;            // UVs da parede de ceu deslocadas no quadro atual                     // por lote de triangulos: 1 = parede de ceu (ver sky_*)
    std::size_t found_ = 0, missing_ = 0;
};

// Liga glActiveTexture/glClientActiveTexture (uma vez por contexto). false = so 1 unidade de textura.
bool gl_init_multitexture();

// v20: sobe uma imagem com mipmaps (e anisotropico, se aniso > 1). 0 = falhou.
unsigned gl_upload_texture(const Image& img, bool clamp, float aniso);
// v20: deixa so a unidade 0 de textura ligada (depois do mapa, que usa a 1 para o lightmap)
void gl_single_texture_unit();

}  // namespace f3v
