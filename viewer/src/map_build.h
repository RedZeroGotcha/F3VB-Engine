// v20: monta um mapa EM MEMORIA direto dos recursos da demo (.map + .rle + .8tre), sem o conversor Python e sem .f3map.
// Porte de tools/py/f3vb/mapfile.py (read_map), rle.py (read_rle) e mapexport.py (build_f3map). Specs: docs/specs/map.md, rle.md, tre.md.
// O resultado e o mesmo MapData que o load_f3map preenche; o F3VBCheck compara os dois nos 38 mapas.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "f3map.h"

namespace f3v {

class Vfs;

struct MapBuildStats { std::size_t nodes = 0, nodes_full = 0, unparsed = 0, outside_bbox = 0, bad = 0; std::string tre_name, rle_name; };

// map_name: nome do recurso MAP_Map (ex.: "Mainmenu", "00_03_Tutorial_Junktown"); aceita com ou sem ".map"
bool build_map(const Vfs& vfs, const std::string& map_name, MapData& out, std::string& err, MapBuildStats* stats = nullptr);

// pecas separadas (testaveis sem Vfs)
bool parse_map_file(const std::uint8_t* data, std::size_t size, MapData& out, std::string& tre, std::string& rle, std::string& err);
bool parse_rle(const std::uint8_t* data, std::size_t size, MapData& out, std::string& err);

}  // namespace f3v
