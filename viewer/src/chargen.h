// Regras da criacao de personagem (CharacterGeneration.int). v20: deixam de copiar o Fallout 2.
// Base de evidencia (docs/JOGO.md, secao "Criacao de personagem"):
//   * Default_PC.CRT da demo (chunk GCRE v4): pericias iniciais Firearms 80, Melee 80, Unarmed 80, Persuasion 75, Medic 75; idade 21.
//   * Fichas salvas pelo jogo original (Documentos\F3\Characters\*.CRT, GCRE v5): 7 SPECIAL, idade, 13 pericias (pontos-base), tracos.
//   * Dois prints da tela original (todos 5 / ST..IN 6 + AG, LK 5): marcar uma pericia soma +20 na BASE; o TOTAL = base + bonus dos atributos.
//     Os atributos derivados abaixo reproduzem os dois prints exatamente (teste em tests/test_ui.cpp).
// O que ainda e hipotese esta marcado em cada formula.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

namespace f3v {

enum Special { S_ST = 0, S_PE, S_EN, S_CH, S_IN, S_AG, S_LK, SPECIAL_COUNT };   // prefixo S_: o windows.h define IN e OUT como macros vazias
const char* special_code(int s);                  // "ST", "PE"...

struct Character {
    std::string name = "None";                    // o jogo original comeca com "None" (texto 7 do .stf)
    bool female = false;
    int age = 21;                                 // Default_PC.CRT
    std::array<int, SPECIAL_COUNT> special{5, 5, 5, 5, 5, 5, 5};
    int points = 5;                               // pontos livres (print: CHAR POINTS 05)
    // pontos-base das 13 pericias, na ordem do arquivo .CRT e da tela: Firearms, Melee, Unarmed, Barter, Persuasion, Deception,
    // Mechanics, Medic, Outdoorsman, Science, Security, Sneak, Steal
    std::array<int, 13> skill_points{80, 80, 80, 0, 75, 0, 0, 75, 0, 0, 0, 0, 0};
    std::set<int> tags;                           // pericias marcadas (1..13), exatamente 3 para comecar
    std::set<int> traits;                         // tracos opcionais (ids dos botoes 11..18, 21..28), ate 2
    // v20: aparencia: indice escolhido em cada coluna (1 Body Type .. 7 Hair Color) da lista da tela; -1 = como o prototipo Default_PC
    std::array<int, 8> look{-1, -1, -1, -1, -1, -1, -1, -1};
    static constexpr int kMinStat = 1, kMaxStat = 10, kTags = 3, kMaxTraits = 2, kSkills = 13, kTagBonus = 20;
    bool inc(int s);                              // +1 num atributo (gasta 1 ponto); false se nao pode
    bool dec(int s);                              // -1 (devolve 1 ponto)
    bool toggle_tag(int skill);                   // false se ja ha 3 e tentou marcar outra
    bool toggle_trait(int trait);
    bool ready() const { return points == 0 && static_cast<int>(tags.size()) == kTags; }
    static int descriptor_text_id(int value) { return 40 + (value < 1 ? 1 : (value > 10 ? 10 : value)); }
    int skill_base(int skill) const;              // coluna "Base": pontos + 20 se marcada
    int skill_bonus(int skill) const;             // vem dos atributos
    int skill_total(int skill) const { return skill_base(skill) + skill_bonus(skill); }   // coluna "Total"
    static int skill_text_id(int skill) { return 82 + skill; }
    std::string derived(int index) const;          // index 1..14 (11 derivados + 3 resistencias), ja formatado como na tela ("+6", "1/3", "17/1", "5%")
    int hit_points() const;
    // Le SPECIAL, idade e pericias de um .CRT (protótipo GCRE v4 ou ficha salva v5). false se nao reconhecer.
    bool load_crt(const std::uint8_t* data, std::size_t size, bool take_special);
};

}  // namespace f3v
