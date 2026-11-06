#include "chargen.h"

#include <algorithm>
#include <cstring>

namespace f3v {

const char* special_code(int s) {
    static const char* c[SPECIAL_COUNT] = {"ST", "PE", "EN", "CH", "IN", "AG", "LK"};
    return s >= 0 && s < SPECIAL_COUNT ? c[s] : "?";
}

bool Character::inc(int s) {
    if (s < 0 || s >= SPECIAL_COUNT || points <= 0 || special[static_cast<std::size_t>(s)] >= kMaxStat) return false;
    ++special[static_cast<std::size_t>(s)];
    --points;
    return true;
}

bool Character::dec(int s) {
    if (s < 0 || s >= SPECIAL_COUNT || special[static_cast<std::size_t>(s)] <= kMinStat) return false;
    --special[static_cast<std::size_t>(s)];
    ++points;
    return true;
}

bool Character::toggle_tag(int k) {
    if (k < 1 || k > kSkills) return false;
    if (tags.erase(k)) return true;
    if (static_cast<int>(tags.size()) >= kTags) return false;
    tags.insert(k);
    return true;
}

bool Character::toggle_trait(int t) {
    if (traits.erase(t)) return true;
    if (static_cast<int>(traits.size()) >= kMaxTraits) return false;
    traits.insert(t);
    return true;
}

int Character::skill_base(int k) const {
    if (k < 1 || k > kSkills) return 0;
    return skill_points[static_cast<std::size_t>(k - 1)] + (tags.count(k) ? kTagBonus : 0);
}

// Bonus dos atributos. Os dois prints da tela original dao, para todos 5: Firearms/Melee +50, o resto +15;
// para ST, PE, EN, CH, IN = 6 e AG, LK = 5: Firearms/Melee +50, Unarmed..Science +18, Security/Sneak +17, Steal +16.
// Conclusao certa: Firearms e Melee nao usam ST..IN (aqui 10 x AG); as outras somam 3 atributos (as que dao 18 so usam ST..IN,
// Security/Sneak usam 1 de AG/LK, Steal usa AG e LK). QUAIS atributos exatamente e hipotese (tematica); 2 casos nao bastam para separar.
int Character::skill_bonus(int k) const {
    const int st = special[S_ST], pe = special[S_PE], en = special[S_EN], ch = special[S_CH], in = special[S_IN], ag = special[S_AG], lk = special[S_LK];
    switch (k) {
        case 1: return 10 * ag;              // Firearms
        case 2: return 10 * ag;              // Melee
        case 3: return st + en + pe;         // Unarmed
        case 4: return ch + in + pe;         // Barter
        case 5: return ch + ch + in;         // Persuasion
        case 6: return ch + in + pe;         // Deception
        case 7: return in + pe + st;         // Mechanics
        case 8: return in + pe + en;         // Medic
        case 9: return en + pe + in;         // Outdoorsman
        case 10: return in + in + pe;        // Science
        case 11: return pe + in + ag;        // Security
        case 12: return pe + en + ag;        // Sneak
        case 13: return pe + ag + lk;        // Steal
        default: return 0;
    }
}

int Character::hit_points() const { return 100; }   // 100/100 nos dois prints (EN 5 e 6): nao depende de EN no inicio

std::string Character::derived(int i) const {
    const int st = special[S_ST], pe = special[S_PE], en = special[S_EN], ag = special[S_AG], lk = special[S_LK], in = special[S_IN];
    switch (i) {
        case 1: return std::to_string(20 + ag);                    // Action Points: 25 com AG 5 (os dois prints; 20+AG e hipotese)
        case 2: return std::to_string(25 + 25 * st);               // Carry Weight: 150 (ST 5), 175 (ST 6)
        case 3: return std::to_string(2 * pe);                     // Combat Sequence: 10, 12
        case 4: return std::to_string(lk) + "%";                   // Critical Chance: 5% com LK 5
        case 5: return "0";                                        // Evasion: 0 nos dois
        case 6: return std::to_string(en);                         // Healing Rate: 5, 6
        case 7: return "+" + std::to_string(st);                   // Melee Damage: +5, +6
        case 8: return std::to_string(40 + 3 * pe);                // Perception Range: 55, 58
        case 9: return "1/3";                                      // Perk Rate
        case 10: return std::to_string(5 + 2 * in) + "/1";         // Skill Rate: 15/1, 17/1
        case 11: return std::to_string(3 * st);                    // Throwing Range: 15, 18
        case 12: return "0%";                                      // Biological
        case 13: return "0%";                                      // Flash
        case 14: return "0%";                                      // Radiation
        default: return "";
    }
}

// .CRT = chunks (tag[4], u32 versao, u32 tamanho com cabecalho). GCRE: 7 x i32 SPECIAL, i32 1, 3 x i32, i32 idade, i32, i32, i32,
// i32 N, N x {i32 pericia (0-based), i32 pontos}. v4 (protótipo) traz so as pericias diferentes de 0; v5 (ficha salva) traz as 13.
bool Character::load_crt(const std::uint8_t* d, std::size_t size, bool take_special) {
    auto rd = [&](std::size_t o) -> std::int32_t { std::int32_t v; std::memcpy(&v, d + o, 4); return v; };
    std::size_t p = 0;
    while (d && p + 12 <= size) {
        std::uint32_t csize;
        std::memcpy(&csize, d + p + 8, 4);
        if (csize < 12 || p + csize > size) return false;
        if (!std::memcmp(d + p, "GCRE", 4)) {
            const std::size_t g = p + 12, end = p + csize;
            if (g + 64 > end) return false;
            if (take_special) for (int s = 0; s < SPECIAL_COUNT; ++s) special[static_cast<std::size_t>(s)] = std::clamp<int>(rd(g + 4 * static_cast<std::size_t>(s)), kMinStat, kMaxStat);
            const std::int32_t a = rd(g + 44);
            if (a >= 16 && a <= 99) age = a;
            const std::int32_t n = rd(g + 60);
            if (n < 0 || n > 64 || g + 64 + 8ull * static_cast<std::size_t>(n) > end) return false;
            skill_points.fill(0);
            for (std::int32_t i = 0; i < n; ++i) {
                const std::int32_t k = rd(g + 64 + 8 * static_cast<std::size_t>(i)), v = rd(g + 68 + 8 * static_cast<std::size_t>(i));
                if (k >= 0 && k < kSkills) skill_points[static_cast<std::size_t>(k)] = std::clamp<int>(v, 0, 300);
            }
            return true;
        }
        p += csize;
    }
    return false;
}

}  // namespace f3v
