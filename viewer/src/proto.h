// v20: leitura minima dos prototipos de entidade da demo (.CRT criaturas/NPCs, .ARM armaduras, .WEA armas, .USE usaveis,
// .ITM/.AMO/.CON/.DOR), o suficiente para MONTAR o modelo 3D da entidade. Medido nos arquivos da demo (docs/specs/prototipos.md):
//   - chunks com etiqueta de 4 letras SEM tamanho fixo conhecido: EEN2 (modelo), EEOV (nome exibido, script, pele, inventario),
//     GENT, GCRE (criatura: SPECIAL...), GITM (item), GIWP (arma), GIAR (armadura), GOBJ, GCHR (personagem salvo: nome);
//   - textos sao "u16 tamanho + bytes" (latin-1).
//   CRT: 1o texto = modelo base (HumMNor, HumMStr, CR_Dog...); pele "Skin_*.dds"; retrato "port_*.dds"; partes do corpo
//        (HaiBald, MusStubble, BodPrisonSuit... + textura opcional "<Parte>_NN_LG.dds"); DEPOIS do GCRE: armadura (.ARM) e arma
//        (.WEA) equipadas. Antes do GCRE: inventario.
//   ARM: modelo no chao (GD_*), icone, e as partes que ela veste (HeaArmingCap, BodPowerArmor, HanPowerArmor...).
//   WEA: modelo no chao (GD_WP_*), icones, e o modelo NA MAO (WP_*: tem esqueleto proprio).
//   USE/ITM/AMO/CON/DOR: 1o texto = modelo.
// O resto (numeros de SPECIAL, pericias, pesos...) fica para quando precisarmos; o parser guarda todos os textos em ordem.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace f3v {

struct ProtoText { std::size_t at = 0; std::string text; };

struct Prototype {
    std::string kind;                                 // "CRT", "ARM", "WEA", "USE"...
    std::string model;                                // 1o texto do EEN2
    std::string display_name, script, skin, portrait;
    std::vector<std::string> parts;                   // partes do corpo (CRT) ou que a armadura veste (ARM)
    std::map<std::string, std::string> part_texture;  // parte -> textura propria (ex.: BacPrisonSuit -> BacPrisonSuit_13_LG)
    std::vector<std::string> inventory;               // arquivos .WEA/.AMO/... carregados (CRT, antes do GCRE)
    std::string armor, weapon;                        // equipados (CRT, depois do GCRE)
    std::string held_model;                           // WEA: modelo na mao (WP_*)
    std::string character_name;                       // GCHR (personagem salvo)
    std::vector<ProtoText> texts;                     // todos os textos, em ordem
};

bool parse_prototype(const std::uint8_t* data, std::size_t size, const std::string& kind, Prototype& out);

// "HaiBald" -> "Hai"; "" se nao for nome de parte
std::string part_slot(const std::string& part);

}  // namespace f3v
