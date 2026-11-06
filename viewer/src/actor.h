// v20: "atores": entidades do mapa com modelo 3D (personagens, criaturas, o vagao do menu, itens no chao), montados DIRETO da
// demo (prototipo .CRT/.USE/... -> B3D + esqueleto GR2 + animacao GR2), animados e com skinning na CPU. Portavel (sem OpenGL):
// o desenho fica em actor_gl.cpp. Regras medidas na demo (docs/JOGO.md, secao "Personagens"):
//   - Humano = corpo base (HumMNor, HumMStr...; regioes Chest/Waist/UpperArms/ForeArms/Hands/Legs/Shins/Feet/Head) + partes
//     "<base>_<Parte>" (HaiBald, BodPrisonSuit, BodPowerArmor...). A armadura equipada (.ARM) traz as proprias partes e substitui
//     as do mesmo encaixe (Bod, Fee, Han...). Regioes do corpo cobertas pelas partes ficam escondidas.
//   - Pele: as texturas "Skin_Hum*" do corpo e das partes viram a pele do prototipo (ex.: Skin_HumM_Black01_LG).
//   - Arma equipada (.WEA): modelo "na mao" (WP_*) preso ao osso "R Handle_1".
//   - Animacao parada: HumM_/HumF_ + Def_<classe>_Idle_01 (classe: Una sem arma, 1H, 2HH para armas pesadas), ou
//     <modelo>_Com_Una_Idle_01 para criaturas e objetos.
#pragma once
#include <memory>
#include <map>
#include <string>
#include <vector>

#include "f3map.h"
#include "scene.h"

namespace f3v {

class Vfs;

struct ActorPart {
    std::string name;                 // nome do modelo (ex.: HumMStr_BodPowerArmor)
    Scene scene;
    std::vector<char> hidden;         // por malha: 1 = nao desenhar (regiao do corpo coberta por roupa)
};

// v21: animacoes por papel. Os nomes vem dos .gr2 da demo (HumM_Com_Una_Walk_01, HumM_Com_Una_Punch_Jab_01, CR_Dog_Com_Una_Attack1_01...).
enum ActorAnim { AN_IDLE = 0, AN_WALK, AN_RUN, AN_ATTACK, AN_HIT, AN_DEATH, AN_COUNT };
const char* actor_anim_name(int a);

struct Actor {
    std::string prototype, model, clip_name, description;
    int clip_index[AN_COUNT] = {-1, -1, -1, -1, -1, -1};   // indice do clipe de cada papel (-1 = nao achado na demo)
    std::string clip_names[AN_COUNT];
    int cur_anim = AN_IDLE;
    float anim_start = 0;             // relogio em que a animacao atual comecou
    bool anim_loop = true;
    std::string weapon_class = "Una"; // Una, 1H, 2HH
    std::string script;               // script do prototipo (ex.: 00_MainMenu_Train no vagao do menu)
    bool melee = true;                // ataque corpo a corpo (soco, faca) ou arma de fogo
    void play(int anim, float now, bool loop);       // troca a animacao (nao reinicia se ja for a mesma em laco)
    float anim_length(int anim) const;               // duracao do clipe (0 se nao houver)
    bool anim_done(float now) const;                 // animacao sem laco chegou ao fim
    std::vector<ActorPart> parts;     // [0] = corpo/modelo base
    bool has_weapon = false;
    ActorPart weapon;
    int hand_joint = -1;              // junta da mao no corpo
    float pos[3] = {0, 0, 0}, yaw = 0;
    float scale = 10.24f;             // esqueleto -> unidades do mapa
    float phase = 0;                  // deslocamento da animacao (cada um num ponto diferente)
    int entity = -1;                  // indice em MapData::entities
    float height = 0;                 // altura aproximada (para a fala flutuante / clique), em unidades do mapa

    void animate(float t);            // avalia a pose da animacao atual no tempo t (Idle: com fase propria; laco ou para no ultimo quadro)
    // matriz do ator (esqueleto -> mapa), coluna-major como o OpenGL: T(pos) * Ry(yaw) * S(scale) * espelho Z
    void model_matrix(float out[16]) const;
    // matriz da arma (esqueleto da arma -> mapa)
    void weapon_matrix(float out[16]) const;
};

// v20: aparencia escolhida na criacao de personagem (so humanos). Vazio = como esta no prototipo.
struct ActorLook {
    std::string body;                               // "Nor", "Fat", "Str", "Wir" (troca HumMNor -> HumM<body>)
    bool female = false, set_sex = false;           // set_sex: troca HumM <-> HumF
    std::map<std::string, std::string> slots;       // encaixe ("Hai", "Pon", "Mus", "Bea") -> parte ("HaiAfro"); "" = tirar a parte do encaixe
    std::string skin;                               // "Tan01" -> Skin_HumM_Tan01_LG
    std::string hair_color;                         // "brown", "black", "blonde", "red", "white", "brownGrey": <Parte>_<cor>_LG.tga
    bool empty() const { return body.empty() && !set_sex && slots.empty() && skin.empty() && hair_color.empty(); }
};
struct ActorBuildOptions { bool with_weapon = true; std::vector<std::string>* log = nullptr; const ActorLook* look = nullptr; };

// Monta o ator da entidade. false (com err) quando o prototipo nao tem modelo desenhavel.
bool build_actor(const Vfs& vfs, const MapEntity& e, Actor& out, std::string& err, const ActorBuildOptions& opt = {});

}  // namespace f3v
