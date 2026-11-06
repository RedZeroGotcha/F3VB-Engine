// v20: sistema de arquivos virtual. O jogo le TUDO por aqui, de uma destas origens:
//   1) a demo ORIGINAL: resource.rht + data\*.grp inteiros (nada extraido) -> Vfs::open_demo
//   2) a copia extraida pelo kit (original_assets: index.csv + RETAIL\...)   -> Vfs::open_extracted (depuracao/analise)
// Formatos (verificados na demo real; specs em docs/specs/rht.md e grp.md):
//   resource.rht: u32 vMajor, vMinor, nEntries, offsetPacks, offsetResources; nEntries x {u32 one, number, type, offset, pack};
//                 regiao de packs = nomes dos .grp (texto com NUL); regiao de recursos = nomes dos recursos (texto com NUL)
//   .grp:         u32 2, u32 1, u32 nFiles; nFiles x {u32 offset, u32 length}; dados. Sem compressao, sem nomes.
// Portavel (sem Windows): testado no Linux contra a copia extraida.
#pragma once
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace f3v {

// ids de tipo do resource.rht (lista completa em tools/py/f3vb/knowledge.py)
enum ResType : std::uint32_t {
    RT_MODEL = 100, RT_TGA = 200, RT_SMA = 300, RT_TRE = 400, RT_SKEL = 600, RT_GR2 = 700, RT_INI = 800,
    RT_WAV = 1100, RT_AMX = 1200, RT_MAP = 1300, RT_CRT = 1400, RT_VEG = 1500, RT_INT = 1600, RT_RLE = 1700,
    RT_PSF = 1900, RT_GSF = 2000, RT_GLS = 2100, RT_SCO = 2200, RT_PCE = 2300, RT_WEA = 2400, RT_ARM = 2500,
    RT_DOR = 2600, RT_USE = 2700, RT_AMO = 3500, RT_CON = 3600, RT_TOK = 3700
};
const char* res_type_name(std::uint32_t type);     // "TGA_Image", ...
const char* res_type_ext(std::uint32_t type);      // "tga", "8", ... ("" se desconhecido)
std::uint32_t res_type_for_ext(const std::string& ext);   // "tga"/"bmp"/"dds" -> RT_TGA, "int" -> RT_INT... (0 = desconhecido)

struct ResEntry {
    std::string name;            // como esta no .rht (ex.: "Mainmenu", "f3_logo", "Int10x10_Cave1_Corner{2}Cave Corner2 (0)")
    std::uint32_t type = 0;
    std::uint32_t pack = 0;      // indice em Vfs::packs()
    std::uint32_t lump = 0;      // numero do lump dentro do .grp
    std::uint64_t offset = 0;    // no .grp (modo demo)
    std::uint32_t size = 0;
    std::string path;            // modo extraido: caminho relativo em RETAIL
};

class Vfs {
public:
    Vfs() = default;
    Vfs(const Vfs&) = delete;
    Vfs& operator=(const Vfs&) = delete;
    ~Vfs();

    bool open_demo(const std::string& demo_dir, std::string& err);            // pasta com resource.rht e data\*.grp
    bool open_extracted(const std::string& assets_dir, std::string& err);     // pasta original_assets (index.csv + RETAIL)
    void close();

    bool is_open() const { return !entries_.empty(); }
    bool demo_mode() const { return demo_; }
    const std::string& root() const { return root_; }
    std::size_t size() const { return entries_.size(); }
    const std::vector<ResEntry>& entries() const { return entries_; }
    const std::vector<std::string>& packs() const { return packs_; }

    // nome sem diferenciar maiusculas; aceita com ou sem extensao (".tga", ".int"...). type = 0: qualquer tipo
    const ResEntry* find(const std::string& name, std::uint32_t type = 0) const;
    // nome de ARQUIVO como os dados citam ("f3_logo.tga", "Mainmenu.int", "skin.dds"): o tipo vem da extensao; cai para qualquer tipo
    const ResEntry* find_file(const std::string& filename) const;
    const ResEntry* find_lump(const std::string& pack, std::uint32_t lump) const;
    std::vector<const ResEntry*> list(std::uint32_t type) const;

    bool read(const ResEntry& e, std::vector<std::uint8_t>& out, std::string& err) const;
    bool read(const std::string& name, std::uint32_t type, std::vector<std::uint8_t>& out) const;   // false se nao achar/ler
    bool read_file(const std::string& filename, std::vector<std::uint8_t>& out) const;

    std::string where(const ResEntry& e) const;     // "Interface.grp#80" ou "RETAIL\Interface\INT_Interface\Mainmenu.int"

private:
    bool add_entry(ResEntry&& e);
    static std::string key(const std::string& name);  // minusculo, sem extensao conhecida
    std::vector<ResEntry> entries_;
    std::vector<std::string> packs_;                  // nomes dos .grp ("Interface", "Maps"...)
    std::vector<std::uint64_t> pack_size_;
    mutable std::vector<std::FILE*> files_;           // abertos sob demanda (modo demo)
    std::multimap<std::string, std::size_t> by_name_; // chave -> indice em entries_
    std::string root_;
    bool demo_ = false;
};

// Confere se uma pasta e a demo original. Explica o motivo quando nao e.
bool is_demo_dir(const std::string& dir, std::string& why);
// Confere se uma pasta e uma copia extraida pelo kit (original_assets). Explica o motivo quando nao e.
bool is_assets_dir(const std::string& dir, std::string& why);

std::string path_join(const std::string& a, const std::string& b);
bool file_exists(const std::string& p);
bool dir_exists(const std::string& p);
bool read_whole_file(const std::string& path, std::vector<std::uint8_t>& out);
std::string to_lower(std::string s);

}  // namespace f3v
