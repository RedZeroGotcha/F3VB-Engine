// v21: criacao de personagem contra os TRES prints do jogo original enviados pelo autor:
//   (1) todos os atributos 5 ("None"); (2) ST/PE/EN/CH/IN 6, AG/LK 5 ("Test", Firearms/Barter/Mechanics marcadas); (3) AG/LK 6, o resto 5.
// Cada linha: SPECIAL, pericias marcadas, e o que o print mostra (Total de cada pericia + derivados).
#include <cstdio>
#include <string>

#include "chargen.h"

using namespace f3v;

static int fails = 0;
static void eq(const std::string& what, const std::string& got, const std::string& want) {
    if (got != want) { std::printf("FALHOU %s: deu %s, o print mostra %s\n", what.c_str(), got.c_str(), want.c_str()); ++fails; }
}

int main() {
    struct Case { const char* name; int sp[7]; int ntags; int tags[3]; int total[13]; const char* der[11]; };
    const Case cases[] = {
        {"None (todos 5)", {5, 5, 5, 5, 5, 5, 5}, 0, {0, 0, 0},
         {130, 130, 95, 15, 90, 15, 15, 90, 15, 15, 15, 15, 15},
         {"25", "150", "10", "5%", "0", "5", "+5", "55", "1/3", "15/1", "15"}},
        {"Test (ST..IN 6)", {6, 6, 6, 6, 6, 5, 5}, 3, {1, 4, 7},
         {150, 130, 98, 38, 93, 18, 38, 93, 18, 18, 17, 17, 16},
         {"25", "175", "12", "5%", "0", "6", "+6", "58", "1/3", "17/1", "18"}},
        {"AG/LK 6", {5, 5, 5, 5, 5, 6, 6}, 0, {0, 0, 0},
         {140, 140, 95, 15, 90, 15, 15, 90, 15, 15, 16, 16, 17},
         {"26", "150", "10", "6%", "0", "5", "+5", "55", "1/3", "15/1", "15"}},
    };
    for (const Case& k : cases) {
        Character c;
        for (int i = 0; i < 7; ++i) c.special[static_cast<std::size_t>(i)] = k.sp[i];
        c.tags.clear();
        for (int i = 0; i < k.ntags; ++i) c.tags.insert(k.tags[i]);   // "TAG SKILLS 03" no print = nenhuma marcada ainda
        for (int s = 1; s <= 13; ++s) eq(std::string(k.name) + " pericia " + std::to_string(s), std::to_string(c.skill_total(s)), std::to_string(k.total[s - 1]));
        for (int d = 1; d <= 11; ++d) eq(std::string(k.name) + " derivado " + std::to_string(d), c.derived(d), k.der[d - 1]);
        eq(std::string(k.name) + " HP", std::to_string(c.hit_points()), "100");
    }
    std::printf(fails ? "v21 chargen: %d diferenca(s) dos prints\n" : "v21 chargen OK (3 prints do original)\n", fails);
    return fails ? 1 : 0;
}
