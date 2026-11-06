// v20: sistema de arquivos virtual (veja vfs.h). Caminhos sao UTF-8; no Windows os arquivos sao abertos pela versao "larga" (acentos funcionam).
#include "vfs.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace f3v {

namespace {

struct TypeInfo { std::uint32_t id; const char* name; const char* ext; };
const TypeInfo kTypes[] = {
    {100, "G3D_Model", "b3d"}, {200, "TGA_Image", "tga"}, {300, "SMA_Script", "amx"}, {400, "TRE_Octree", "8tre"},
    {500, "SKN_Skin", "skn"}, {600, "SKEL_Skeleton", "skl"}, {700, "GR2_Animation", "gr2"}, {800, "INI_Config", "ini"},
    {900, "RLZ", "rlz"}, {1000, "RTD", "rtd"}, {1100, "WAV_Audio", "wav"}, {1200, "AMX", "amx"}, {1300, "MAP_Map", "map"},
    {1400, "CRT_ENT_Critter", "crt"}, {1500, "VEG_Vfx", "veg"}, {1600, "INT_Interface", "int"}, {1700, "RLE", "rle"},
    {1800, "ITM", "itm"}, {1900, "PSF", "psf"}, {2000, "GSF", "gsf"}, {2100, "GLS", "gls"}, {2200, "SCO", "sco"},
    {2300, "PCE", "pce"}, {2400, "WEA_ENT_Weapon", "wea"}, {2500, "ARM_ENT_Armor", "arm"}, {2600, "DOR_ENT_Door", "dor"},
    {2700, "USE_ENT_Use", "use"}, {2800, "SST", "sst"}, {2900, "STR", "str"}, {3000, "ENC", "enc"}, {3100, "WMP", "wmp"},
    {3200, "PRO", "pro"}, {3300, "SPL", "spl"}, {3400, "FNT", "fnt"}, {3500, "AMO_ENT_Ammo", "amo"}, {3600, "CON_ENT", "con"},
    {3700, "TOK_Tokens", "tok"},
};

std::filesystem::path fs_path(const std::string& utf8) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::FILE* open_file(const std::string& utf8, const char* mode) {
#ifdef _WIN32
    const std::filesystem::path p = fs_path(utf8);
    std::wstring wm;
    for (const char* c = mode; *c; ++c) wm.push_back(static_cast<wchar_t>(*c));
    return _wfopen(p.c_str(), wm.c_str());
#else
    return std::fopen(utf8.c_str(), mode);
#endif
}

bool seek64(std::FILE* f, std::uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, static_cast<__int64>(off), SEEK_SET) == 0;
#else
    return fseeko(f, static_cast<off_t>(off), SEEK_SET) == 0;
#endif
}

std::uint32_t rd32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }

// le um campo CSV (com aspas opcionais); avanca i
std::string csv_field(const std::string& line, std::size_t& i) {
    std::string out;
    if (i < line.size() && line[i] == '"') {
        ++i;
        while (i < line.size()) {
            if (line[i] == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { out.push_back('"'); i += 2; continue; }
                ++i;
                break;
            }
            out.push_back(line[i++]);
        }
        if (i < line.size() && line[i] == ',') ++i;
        return out;
    }
    while (i < line.size() && line[i] != ',') out.push_back(line[i++]);
    if (i < line.size()) ++i;
    return out;
}

std::uint32_t type_by_name(const std::string& n) {
    for (const TypeInfo& t : kTypes) if (n == t.name) return t.id;
    return 0;
}

}  // namespace

const char* res_type_name(std::uint32_t type) {
    for (const TypeInfo& t : kTypes) if (t.id == type) return t.name;
    return "?";
}
const char* res_type_ext(std::uint32_t type) {
    for (const TypeInfo& t : kTypes) if (t.id == type) return t.ext;
    return "";
}
std::uint32_t res_type_for_ext(const std::string& ext_in) {
    const std::string e = to_lower(ext_in);
    if (e == "tga" || e == "bmp" || e == "dds" || e == "ctx") return RT_TGA;   // os dados citam .bmp/.dds/.ctx, mas tudo e TGA_Image na demo
    if (e == "8" || e == "8tre") return RT_TRE;
    if (e == "sma") return RT_SMA;
    for (const TypeInfo& t : kTypes) if (e == t.ext) return t.id;
    return 0;
}

std::string to_lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string path_join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    const char last = a.back();
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    return (last == '\\' || last == '/') ? a + b : a + sep + b;
}

bool file_exists(const std::string& p) { std::error_code ec; return std::filesystem::is_regular_file(fs_path(p), ec); }
bool dir_exists(const std::string& p) { std::error_code ec; return std::filesystem::is_directory(fs_path(p), ec); }

bool read_whole_file(const std::string& path, std::vector<std::uint8_t>& out) {
    out.clear();
    std::FILE* f = open_file(path, "rb");
    if (!f) return false;
    std::uint8_t tmp[65536];
    std::size_t n;
    while ((n = std::fread(tmp, 1, sizeof tmp, f)) > 0) out.insert(out.end(), tmp, tmp + n);
    std::fclose(f);
    return true;
}

// ------------------------------------------------------------------------------------------------ Vfs
Vfs::~Vfs() { close(); }

void Vfs::close() {
    for (std::FILE*& f : files_) if (f) { std::fclose(f); f = nullptr; }
    files_.clear();
    entries_.clear();
    packs_.clear();
    pack_size_.clear();
    by_name_.clear();
    root_.clear();
    demo_ = false;
}

std::string Vfs::key(const std::string& name) {
    std::string k = to_lower(name);
    const std::size_t dot = k.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < k.size() && k.size() - dot <= 5 && res_type_for_ext(k.substr(dot + 1))) k.erase(dot);
    return k;
}

bool Vfs::add_entry(ResEntry&& e) {
    by_name_.emplace(key(e.name), entries_.size());
    entries_.push_back(std::move(e));
    return true;
}

bool Vfs::open_demo(const std::string& dir, std::string& err) {
    close();
    std::string why;
    if (!is_demo_dir(dir, why)) { err = why; return false; }
    std::vector<std::uint8_t> rht;
    if (!read_whole_file(path_join(dir, "resource.rht"), rht)) { err = "nao consegui ler resource.rht"; return false; }
    if (rht.size() < 20) { err = "resource.rht pequeno demais"; return false; }
    const std::uint32_t n = rd32(&rht[8]), off_packs = rd32(&rht[12]), off_res = rd32(&rht[16]);
    if (n == 0 || n > 1000000 || 20ull + 20ull * n > rht.size() || off_packs > off_res || off_res > rht.size()) {
        err = "cabecalho do resource.rht incoerente (entradas=" + std::to_string(n) + ")";
        return false;
    }
    // nomes dos packs: valor de 'pack' = deslocamento na regiao de packs
    std::map<std::uint32_t, std::uint32_t> pack_index;   // deslocamento -> indice em packs_
    auto cstr = [&](std::size_t start, std::size_t limit_end) -> std::string {
        std::string s;
        for (std::size_t i = start; i < limit_end && rht[i]; ++i) s.push_back(static_cast<char>(rht[i]));
        return s;
    };
    const std::string data_dir = path_join(dir, "data");
    std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> tables;   // por pack: (offset, length) de cada lump
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint8_t* e = &rht[20 + 20 * static_cast<std::size_t>(i)];
        const std::uint32_t number = rd32(e + 4), type = rd32(e + 8), noff = rd32(e + 12), pack = rd32(e + 16);
        if (pack == 0xFFFFFFFFu || off_packs + static_cast<std::size_t>(pack) >= off_res) continue;
        auto pit = pack_index.find(pack);
        if (pit == pack_index.end()) {
            const std::string pname = cstr(off_packs + pack, off_res);
            if (pname.empty()) continue;
            // abre a tabela do .grp (nome sem diferenciar maiusculas)
            std::string gpath = path_join(data_dir, pname + ".grp");
            if (!file_exists(gpath)) {
                std::error_code ec;
                for (const auto& de : std::filesystem::directory_iterator(fs_path(data_dir), ec)) {
                    const std::u8string u = de.path().filename().u8string();
                    const std::string fn(reinterpret_cast<const char*>(u.data()), u.size());
                    if (to_lower(fn) == to_lower(pname + ".grp")) { gpath = path_join(data_dir, fn); break; }
                }
            }
            std::vector<std::pair<std::uint32_t, std::uint32_t>> table;
            std::uint64_t fsize = 0;
            std::FILE* f = open_file(gpath, "rb");
            if (f) {
                std::uint8_t hdr[12];
                if (std::fread(hdr, 1, 12, f) == 12) {
                    const std::uint32_t nf = rd32(hdr + 8);
                    std::error_code ec;
                    fsize = static_cast<std::uint64_t>(std::filesystem::file_size(fs_path(gpath), ec));
                    if (nf < 1000000 && 12ull + 8ull * nf <= fsize) {
                        std::vector<std::uint8_t> tb(8 * static_cast<std::size_t>(nf));
                        if (nf == 0 || std::fread(tb.data(), 1, tb.size(), f) == tb.size())
                            for (std::uint32_t k = 0; k < nf; ++k) table.emplace_back(rd32(&tb[8 * k]), rd32(&tb[8 * k + 4]));
                    }
                }
                std::fclose(f);
            }
            pack_index[pack] = static_cast<std::uint32_t>(packs_.size());
            pit = pack_index.find(pack);
            packs_.push_back(pname);
            pack_size_.push_back(fsize);
            tables.push_back(std::move(table));
            files_.push_back(nullptr);
            root_ = dir;
            (void)gpath;
        }
        const std::uint32_t pi = pit->second;
        if (number >= tables[pi].size()) continue;                       // .grp ausente/curto: o recurso fica de fora (relatado por quem chamar)
        const auto& lm = tables[pi][number];
        if (static_cast<std::uint64_t>(lm.first) + lm.second > pack_size_[pi]) continue;
        ResEntry re;
        re.name = (off_res + static_cast<std::size_t>(noff) < rht.size()) ? cstr(off_res + noff, rht.size()) : std::string();
        if (re.name.empty()) re.name = packs_[pi] + "#" + std::to_string(number);
        re.type = type;
        re.pack = pi;
        re.lump = number;
        re.offset = lm.first;
        re.size = lm.second;
        add_entry(std::move(re));
    }
    if (entries_.empty()) { err = "nenhum recurso legivel (os .grp da pasta data estao faltando ou corrompidos?)"; close(); return false; }
    root_ = dir;
    demo_ = true;
    return true;
}

bool Vfs::open_extracted(const std::string& dir, std::string& err) {
    close();
    std::string why;
    if (!is_assets_dir(dir, why)) { err = why; return false; }
    std::vector<std::uint8_t> csv;
    if (!read_whole_file(path_join(dir, "index.csv"), csv)) { err = "nao consegui ler index.csv"; return false; }
    std::string text(csv.begin(), csv.end());
    std::size_t pos = 0;
    bool header = true;
    std::map<std::string, std::uint32_t> pk;
    while (pos < text.size()) {
        std::size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (header) { header = false; continue; }
        std::size_t i = 0;
        const std::string pack = csv_field(line, i), num = csv_field(line, i), tname = csv_field(line, i), name = csv_field(line, i), rel = csv_field(line, i), sz = csv_field(line, i);
        if (pack.empty() || rel.empty()) continue;
        auto it = pk.find(pack);
        if (it == pk.end()) { it = pk.emplace(pack, static_cast<std::uint32_t>(packs_.size())).first; packs_.push_back(pack); pack_size_.push_back(0); files_.push_back(nullptr); }
        ResEntry re;
        re.name = name;
        re.type = type_by_name(tname);
        re.pack = it->second;
        re.lump = static_cast<std::uint32_t>(std::strtoul(num.c_str(), nullptr, 10));
        re.size = static_cast<std::uint32_t>(std::strtoul(sz.c_str(), nullptr, 10));
        re.path = rel;
        add_entry(std::move(re));
    }
    if (entries_.empty()) { err = "index.csv vazio"; close(); return false; }
    root_ = dir;
    demo_ = false;
    return true;
}

const ResEntry* Vfs::find(const std::string& name, std::uint32_t type) const {
    const std::string k = key(name);
    const auto range = by_name_.equal_range(k);
    const ResEntry* any = nullptr;
    for (auto it = range.first; it != range.second; ++it) {
        const ResEntry& e = entries_[it->second];
        if (type == 0 || e.type == type) return &e;
        if (!any) any = &e;
    }
    if (type == 0) return any;
    // os scripts de dialogo sao SMA_Script mas citados como .amx
    if (type == RT_AMX) return find(name, RT_SMA);
    return nullptr;
}

const ResEntry* Vfs::find_file(const std::string& filename) const {
    const std::size_t dot = filename.find_last_of('.');
    const std::uint32_t t = dot == std::string::npos ? 0 : res_type_for_ext(filename.substr(dot + 1));
    const ResEntry* e = t ? find(filename, t) : nullptr;
    return e ? e : find(filename, 0);
}

const ResEntry* Vfs::find_lump(const std::string& pack, std::uint32_t lump) const {
    const std::string lp = to_lower(pack);
    for (const ResEntry& e : entries_) if (e.lump == lump && to_lower(packs_[e.pack]) == lp) return &e;
    return nullptr;
}

std::vector<const ResEntry*> Vfs::list(std::uint32_t type) const {
    std::vector<const ResEntry*> v;
    for (const ResEntry& e : entries_) if (type == 0 || e.type == type) v.push_back(&e);
    return v;
}

bool Vfs::read(const ResEntry& e, std::vector<std::uint8_t>& out, std::string& err) const {
    out.clear();
    if (!demo_) {
        std::string rel = e.path;
#ifdef _WIN32
        for (char& c : rel) if (c == '/') c = '\\';
#endif
        if (!read_whole_file(path_join(path_join(root_, "RETAIL"), rel), out)) { err = "nao consegui abrir RETAIL/" + e.path; return false; }
        return true;
    }
    if (e.pack >= files_.size()) { err = "pack invalido"; return false; }
    std::FILE*& f = files_[e.pack];
    if (!f) {
        std::string gpath = path_join(path_join(root_, "data"), packs_[e.pack] + ".grp");
        if (!file_exists(gpath)) {
            std::error_code ec;
            for (const auto& de : std::filesystem::directory_iterator(fs_path(path_join(root_, "data")), ec)) {
                const std::u8string u = de.path().filename().u8string();
                const std::string fn(reinterpret_cast<const char*>(u.data()), u.size());
                if (to_lower(fn) == to_lower(packs_[e.pack] + ".grp")) { gpath = path_join(path_join(root_, "data"), fn); break; }
            }
        }
        f = open_file(gpath, "rb");
        if (!f) { err = "nao consegui abrir " + gpath; return false; }
    }
    if (!seek64(f, e.offset)) { err = "seek falhou em " + packs_[e.pack] + ".grp"; return false; }
    out.resize(e.size);
    if (e.size && std::fread(out.data(), 1, e.size, f) != e.size) { err = "leitura curta em " + packs_[e.pack] + ".grp"; out.clear(); return false; }
    return true;
}

bool Vfs::read(const std::string& name, std::uint32_t type, std::vector<std::uint8_t>& out) const {
    const ResEntry* e = find(name, type);
    std::string err;
    return e && read(*e, out, err);
}

bool Vfs::read_file(const std::string& filename, std::vector<std::uint8_t>& out) const {
    const ResEntry* e = find_file(filename);
    std::string err;
    return e && read(*e, out, err);
}

std::string Vfs::where(const ResEntry& e) const {
    if (demo_) return (e.pack < packs_.size() ? packs_[e.pack] : std::string("?")) + ".grp#" + std::to_string(e.lump);
    return "RETAIL/" + e.path;
}

bool is_demo_dir(const std::string& dir, std::string& why) {
    if (dir.empty()) { why = "nenhuma pasta escolhida"; return false; }
    if (!dir_exists(dir)) { why = "a pasta nao existe: " + dir; return false; }
    if (!file_exists(path_join(dir, "resource.rht"))) { why = "nao tem resource.rht (escolha a pasta da demo, a que tem F3.exe, resource.rht e a pasta data)"; return false; }
    const std::string data = path_join(dir, "data");
    if (!dir_exists(data)) { why = "nao tem a pasta data (com os arquivos .grp)"; return false; }
    int grp = 0;
    std::error_code ec;
    for (const auto& de : std::filesystem::directory_iterator(fs_path(data), ec)) {
        const std::u8string u = de.path().extension().u8string();
        if (to_lower(std::string(reinterpret_cast<const char*>(u.data()), u.size())) == ".grp") ++grp;
    }
    if (grp == 0) { why = "a pasta data nao tem nenhum .grp"; return false; }
    why.clear();
    return true;
}

bool is_assets_dir(const std::string& dir, std::string& why) {
    if (dir.empty()) { why = "nenhuma pasta"; return false; }
    if (!dir_exists(dir)) { why = "a pasta nao existe: " + dir; return false; }
    if (!file_exists(path_join(dir, "index.csv"))) { why = "nao tem index.csv (nao e uma copia extraida pelo kit: rode extract-all)"; return false; }
    if (!dir_exists(path_join(path_join(path_join(dir, "RETAIL"), "Interface"), "INT_Interface"))) { why = "nao tem RETAIL/Interface/INT_Interface (extracao incompleta)"; return false; }
    why.clear();
    return true;
}

}  // namespace f3v
