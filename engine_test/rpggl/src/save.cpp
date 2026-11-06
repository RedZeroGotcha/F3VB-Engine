// =====================================================================
//  save.cpp - sistema de saves por arquivo .sav (estilo dos grandes RPGs de mundo aberto)
//
//  * Pasta "saves" ao lado do .exe.  Nomes: SaveNNN_Nome_Local_LvX_HHHhMMm.sav,
//    Quicksave1..3_*, Autosave1..3_* (rotativos, como nos grandes RPGs de mundo aberto).
//  * Cabecalho legivel sem carregar o jogo: nome, nivel, local, tempo de
//    jogo, data e MINIATURA (256x128) do momento do save.
//  * Corpo em SECOES etiquetadas (PLYR, INVT, QSTS, TIME, WRLD, NPCS, CAMR, VEHS):
//    versoes futuras podem acrescentar secoes e as antigas as ignoram.
//  * CRC32 do corpo; gravacao ATOMICA (.tmp -> rename) feita numa thread
//    do pool: o jogo nao engasga ao salvar.
//  * O mundo em si NAO e salvo: ele e funcao da semente.  Salva-se apenas
//    o "delta": mortos, saqueados, NPCs feridos, veiculos, locais descobertos.
// =====================================================================
#include "game.h"
#include <ctime>
#include <sys/stat.h>
#ifndef _WIN32
  #include <dirent.h>
  #include <unistd.h>
#endif

std::vector<SaveInfo> g_saveList;

static const char MAGIC[8] = { 'R', 'P', 'G', 'G', 'L', 'S', 'A', 'V' };
static const u32 SAVE_VERSION = 1;
#define TAG(a, b, c, d) ((u32)(a) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

struct BW {
    std::vector<u8> d;
    template<class T> void put(const T& v) { const u8* p = (const u8*)&v; d.insert(d.end(), p, p + sizeof(T)); }
    void bytes(const void* p, size_t n) { const u8* q = (const u8*)p; d.insert(d.end(), q, q + n); }
    void fixedStr(const char* s, size_t n) { std::vector<u8> t(n, 0); size_t l = strlen(s); memcpy(t.data(), s, l < n ? l : n - 1); bytes(t.data(), n); }
};
struct BR {
    const u8* p; size_t n, pos; bool ok;
    BR(const u8* pp, size_t nn) : p(pp), n(nn), pos(0), ok(true) {}
    template<class T> T get() { T v = T(); if (pos + sizeof(T) > n) { ok = false; return v; } memcpy(&v, p + pos, sizeof(T)); pos += sizeof(T); return v; }
    bool bytes(void* out, size_t k) { if (pos + k > n) { ok = false; return false; } memcpy(out, p + pos, k); pos += k; return true; }
    bool skip(size_t k) { if (pos + k > n) { ok = false; return false; } pos += k; return true; }
};

static u32 Crc32(const u8* d, size_t n) {
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) { c ^= d[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (u32)(-(int)(c & 1))); }
    return ~c;
}

// ---------------------------------------------------------------------
//  Sistema de arquivos
// ---------------------------------------------------------------------
static std::string ExeDir() {
    static std::string dir;
    if (!dir.empty()) return dir;
#ifdef _WIN32
    char buf[MAX_PATH + 1]; DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH); buf[n] = 0; std::string s(buf);
    size_t k = s.find_last_of("\\/"); dir = (k == std::string::npos) ? "." : s.substr(0, k);
#else
    const char* e = getenv("RPGGL_HOME"); dir = e ? e : ".";
#endif
    return dir;
}
static void MakeDir(const std::string& p) {
#ifdef _WIN32
    CreateDirectoryA(p.c_str(), NULL);
#else
    mkdir(p.c_str(), 0755);
#endif
}
std::string SaveDir() { std::string d = ExeDir() + "/saves"; MakeDir(d); return d; }

static bool ReadFile(const std::string& path, std::vector<u8>& out) {
    FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 64*1024*1024) { fclose(f); return false; }
    out.resize((size_t)n); size_t r = fread(out.data(), 1, (size_t)n, f); fclose(f); return r == (size_t)n;
}
static bool RemoveFile(const std::string& p) { return remove(p.c_str()) == 0; }
static bool WriteAtomic(const std::string& path, const std::vector<u8>& data) {
    std::string tmp = path + ".tmp"; FILE* f = fopen(tmp.c_str(), "wb"); if (!f) return false;
    size_t w = fwrite(data.data(), 1, data.size(), f); bool ok = (w == data.size()); ok = (fflush(f) == 0) && ok; fclose(f);
    if (!ok) { remove(tmp.c_str()); return false; }
#ifdef _WIN32
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { remove(tmp.c_str()); return false; }
    return true;
#else
    return rename(tmp.c_str(), path.c_str()) == 0;
#endif
}
static void ListDir(const std::string& dir, std::vector<std::string>& names) {
#ifdef _WIN32
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((dir + "/*.sav").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(fd.cFileName); } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir.c_str()); if (!d) return;
    while (struct dirent* e = readdir(d)) { std::string n = e->d_name; if (n.size() > 4 && n.compare(n.size() - 4, 4, ".sav") == 0) names.push_back(n); }
    closedir(d);
#endif
}

// ---------------------------------------------------------------------
//  Cabecalho
// ---------------------------------------------------------------------
static bool ParseHeader(const std::vector<u8>& f, SaveInfo& si, size_t& bodyOff, u32& bodyLen, u32& crc, bool thumbs) {
    BR r(f.data(), f.size()); char mg[8]; if (!r.bytes(mg, 8) || memcmp(mg, MAGIC, 8) != 0) return false;
    si.version = r.get<u32>(); if (si.version != SAVE_VERSION) return false;
    si.stamp = r.get<u64>(); si.number = r.get<u32>(); si.playSeconds = r.get<float>(); si.level = r.get<int>();
    r.bytes(si.name, 32); r.bytes(si.location, 48); si.name[31] = 0; si.location[47] = 0;
    si.gameHours = r.get<float>(); si.kind = (char)r.get<u8>(); r.skip(3);
    si.tw = r.get<u16>(); si.th = r.get<u16>(); u32 tl = r.get<u32>();
    if (!r.ok || tl > 4*1024*1024 || (size_t)tl != (size_t)si.tw*(size_t)si.th*3 + 0) { if (tl != 0) return false; }
    si.thumb.clear();
    if (thumbs && tl) { si.thumb.resize(tl); r.bytes(si.thumb.data(), tl); } else r.skip(tl);
    bodyLen = r.get<u32>(); crc = r.get<u32>(); bodyOff = r.pos;
    if (!r.ok || bodyOff + bodyLen > f.size()) return false;
    return true;
}

std::vector<SaveInfo> ListSaves(bool withThumbs) {
    std::vector<SaveInfo> out; std::string dir = SaveDir(); std::vector<std::string> names; ListDir(dir, names);
    for (size_t i = 0; i < names.size(); i++) {
        std::vector<u8> f; if (!ReadFile(dir + "/" + names[i], f)) continue;
        SaveInfo si; si.valid = false; size_t off; u32 bl, crc;
        if (!ParseHeader(f, si, off, bl, crc, withThumbs)) continue;
        si.valid = true; si.path = dir + "/" + names[i]; si.file = names[i]; out.push_back(si);
    }
    std::sort(out.begin(), out.end(), [](const SaveInfo& a, const SaveInfo& b) { return a.stamp != b.stamp ? a.stamp > b.stamp : a.number > b.number; });
    return out;
}
void RefreshSaveList() { g_saveList = ListSaves(true); G.hasSaves = !g_saveList.empty(); }
bool DeleteSave(const std::string& path) { return RemoveFile(path); }

// ---------------------------------------------------------------------
//  Serializacao
// ---------------------------------------------------------------------
static void Section(BW& body, u32 tag, const BW& s) { body.put(tag); body.put((u32)s.d.size()); body.bytes(s.d.data(), s.d.size()); }

static void BuildBody(BW& body) {
    Player pcopy = G.p; if (G.drive) { Vec3 l(-cosf(G.drive->yaw), 0, sinf(G.drive->yaw)); pcopy.pos = Vec3(G.drive->pos.x + l.x*2.6f, G.drive->pos.y, G.drive->pos.z + l.z*2.6f); }
    const Player& p = pcopy;
    { BW s;
      s.put(p.pos.x); s.put(p.pos.y); s.put(p.pos.z); s.put(p.yaw); s.put(p.pitch); s.put(p.hp); s.put(p.maxhp); s.put(p.stam); s.put(p.rad);
      s.put(p.level); s.put(p.xp); s.put(p.points); s.put(p.money); for (int i = 0; i < AT_COUNT; i++) s.put(p.attr[i]);
      s.put(p.weapon); s.put(p.armor); for (int i = 0; i < IT_COUNT; i++) s.put(p.mag[i]); s.put((u8)p.sneak); s.fixedStr(p.name, 32); s.put(p.bodyYaw); s.put(p.artifact); s.put(p.hunger); s.put(p.thirst);
      Section(body, TAG('P', 'L', 'Y', 'R'), s); }
    { BW s; s.put((u32)p.inv.size()); for (size_t i = 0; i < p.inv.size(); i++) { s.put(p.inv[i].id); s.put(p.inv[i].count); } Section(body, TAG('I', 'N', 'V', 'T'), s); }
    { BW s; for (int i = 0; i < Q_COUNT; i++) { s.put(G.quests[i].state); s.put(G.quests[i].stage); s.put(G.quests[i].count); } s.put(G.karma); s.put(G.kills); Section(body, TAG('Q', 'S', 'T', 'S'), s); }
    { BW s; s.put(G.gameHours); s.put(G.playSeconds); Section(body, TAG('T', 'I', 'M', 'E'), s); }
    { BW s; s.put(WorldSeed());
      s.put((u32)G.killed.size()); for (std::unordered_map<u64, float>::const_iterator it = G.killed.begin(); it != G.killed.end(); ++it) { s.put(it->first); s.put(it->second); }
      s.put((u32)G.looted.size()); for (std::unordered_map<u64, float>::const_iterator it = G.looted.begin(); it != G.looted.end(); ++it) { s.put(it->first); s.put(it->second); }
      s.put((u32)G.discovered.size()); for (std::unordered_set<u64>::const_iterator it = G.discovered.begin(); it != G.discovered.end(); ++it) s.put(*it);
      Section(body, TAG('W', 'R', 'L', 'D'), s); }
    { BW s; std::unordered_map<u64, NpcOvr> ov = G.npcOvr;                      // NPCs descarregados + os vivos feridos/em combate
      for (size_t i = 0; i < G.npcs.size(); i++) { const Npc* n = G.npcs[i]; if (n->alive && (n->hp < n->maxhp - 0.5f || n->state == NS_COMBAT)) { NpcOvr o; o.x = n->pos.x; o.z = n->pos.z; o.hp = n->hp; o.state = (u8)(n->state == NS_COMBAT ? NS_COMBAT : NS_IDLE); ov[n->id] = o; } }
      s.put((u32)ov.size()); for (std::unordered_map<u64, NpcOvr>::const_iterator it = ov.begin(); it != ov.end(); ++it) { s.put(it->first); s.put(it->second.x); s.put(it->second.z); s.put(it->second.hp); s.put(it->second.state); }
      Section(body, TAG('N', 'P', 'C', 'S'), s); }
    { BW s; s.put((u8)G.cam.first); Section(body, TAG('C', 'A', 'M', 'R'), s); }
    { BW s; std::unordered_map<u64, VehOvr> vo = G.vehOvr;                      // veiculos: descarregados + vivos
      for (size_t i = 0; i < G.vehicles.size(); i++) { const Vehicle* v = G.vehicles[i]; VehOvr o; o.type = v->type; o.x = v->pos.x; o.z = v->pos.z; o.yaw = v->yaw; o.fuel = v->fuel; o.hp = v->hp; o.state = (u8)v->state; vo[v->id] = o; }
      s.put((u32)vo.size()); for (std::unordered_map<u64, VehOvr>::const_iterator it = vo.begin(); it != vo.end(); ++it) { const VehOvr& o = it->second; s.put(it->first); s.put(o.type); s.put(o.x); s.put(o.z); s.put(o.yaw); s.put(o.fuel); s.put(o.hp); s.put(o.state); }
      Section(body, TAG('V', 'E', 'H', 'S'), s); }
}

static std::string Sanitize(const char* s) {
    std::string o;
    for (const unsigned char* c = (const unsigned char*)s; *c; c++) {
        if ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')) o += (char)*c;
        else if (*c == ' ' || *c == '-' || *c == '_') o += '_';
        else if (*c == 0xC3 && c[1]) {
            unsigned char b = c[1]; c++; char r = 0; unsigned char l = (unsigned char)(b | 0x20);
            if (l >= 0xA0 && l <= 0xA5) r = 'a'; else if (l == 0xA7) r = 'c'; else if (l >= 0xA8 && l <= 0xAB) r = 'e'; else if (l >= 0xAC && l <= 0xAF) r = 'i';
            else if (l >= 0xB2 && l <= 0xB6) r = 'o'; else if (l >= 0xB9 && l <= 0xBC) r = 'u';
            if (r) o += r;
        }
    }
    if (o.empty()) o = "X"; if (o.size() > 24) o.resize(24); return o;
}

void RequestSave(int kind, const std::string& overwritePath) {
    if (G.state != GS_PLAYING || G.p.dead) return;
    G.saveReq = 1; G.saveReqKind = kind; G.saveReqName = overwritePath;
}

void GameCommitSave(const std::vector<u8>& thumb, int tw, int th) {
    if (!G.saveReq) return;
    G.saveReq = 0; int kind = G.saveReqKind; std::string over = G.saveReqName;
    std::vector<SaveInfo> list = ListSaves(false);
    u32 number = 1;
    if (kind == SK_MANUAL) {
        bool found = false; u32 mx = 0;
        for (size_t i = 0; i < list.size(); i++) { if (list[i].kind == SK_MANUAL && list[i].number > mx) mx = list[i].number; if (!over.empty() && list[i].path == over) { number = list[i].number; found = true; } }
        if (!found) number = mx + 1;
    } else {
        for (size_t i = 0; i < list.size(); i++) if (list[i].kind == kind) { number = list[i].number % 3 + 1; break; }     // lista ordenada: o primeiro e o mais novo
    }
    std::vector<u8> thumbData = thumb; if ((int)thumbData.size() != tw*th*3) { thumbData.clear(); tw = th = 0; }
    BW body; BuildBody(body);
    BW f; f.bytes(MAGIC, 8); f.put(SAVE_VERSION); f.put((u64)time(NULL)); f.put(number); f.put(G.playSeconds); f.put(G.p.level);
    f.fixedStr(G.p.name, 32); f.fixedStr(AreaName(G.p.pos.x, G.p.pos.z), 48);
    f.put(G.gameHours); f.put((u8)kind); f.put((u8)0); f.put((u8)0); f.put((u8)0);
    f.put((u16)tw); f.put((u16)th); f.put((u32)thumbData.size()); f.bytes(thumbData.data(), thumbData.size());
    f.put((u32)body.d.size()); f.put(Crc32(body.d.data(), body.d.size())); f.bytes(body.d.data(), body.d.size());

    int hrs = (int)(G.playSeconds/3600.0f), mins = (int)(G.playSeconds/60.0f) % 60; char fn[256];
    std::string nm = Sanitize(G.p.name), loc = Sanitize(AreaName(G.p.pos.x, G.p.pos.z));
    const char* pre = kind == SK_QUICK ? "Quicksave" : (kind == SK_AUTO ? "Autosave" : "Save");
    if (kind == SK_MANUAL) snprintf(fn, sizeof(fn), "%s%03u_%s_%s_Lv%d_%03dh%02dm.sav", pre, number, nm.c_str(), loc.c_str(), G.p.level, hrs, mins);
    else snprintf(fn, sizeof(fn), "%s%u_%s_%s_Lv%d_%03dh%02dm.sav", pre, number, nm.c_str(), loc.c_str(), G.p.level, hrs, mins);
    std::string dir = SaveDir(), path = dir + "/" + fn; std::vector<u8> data = f.d;
    std::string slotPrefix = (kind == SK_MANUAL) ? "" : (std::string(pre) + std::to_string(number) + "_");
    G.saveBusy++;
    G.pool.Submit([path, data, over, slotPrefix, dir]() {
        bool ok = WriteAtomic(path, data);
        if (ok) {
            if (!over.empty() && over != path) RemoveFile(over);
            if (!slotPrefix.empty()) {                                          // rotacao: remove o save antigo do mesmo slot
                std::vector<std::string> names; ListDir(dir, names);
                for (size_t i = 0; i < names.size(); i++) if (names[i].compare(0, slotPrefix.size(), slotPrefix) == 0 && dir + "/" + names[i] != path) RemoveFile(dir + "/" + names[i]);
            }
            G.saveOk++;
        } else G.saveFail++;
        G.saveBusy--;
    });
}

// ---------------------------------------------------------------------
//  Carregar
// ---------------------------------------------------------------------
bool GameLoadFile(const std::string& path) {
    std::vector<u8> f; SaveInfo si; size_t off; u32 bl, crc;
    if (!ReadFile(path, f) || !ParseHeader(f, si, off, bl, crc, false)) { Notify("Save inválido ou de outra versão.", 3); return false; }
    if (Crc32(f.data() + off, bl) != crc) { Notify("Save corrompido (CRC não confere).", 3); return false; }
    Player np; std::vector<Slot> inv; QuestState nq[Q_COUNT]; memset(nq, 0, sizeof(nq)); int karma = 0, kills = 0; float hours = 8.5f, play = 0; bool first = false, havePlayer = false;
    std::unordered_map<u64, float> killed, looted; std::unordered_set<u64> disc; std::unordered_map<u64, NpcOvr> ovr; std::unordered_map<u64, VehOvr> vovr; u32 seed = WorldSeed();
    BR body(f.data() + off, bl);
    while (body.ok && body.pos + 8 <= body.n) {
        u32 tag = body.get<u32>(), len = body.get<u32>(); if (body.pos + len > body.n) { body.ok = false; break; }
        BR s(body.p + body.pos, len); body.pos += len;
        switch (tag) {
        case TAG('P', 'L', 'Y', 'R'): {
            np.pos.x = s.get<float>(); np.pos.y = s.get<float>(); np.pos.z = s.get<float>(); np.yaw = s.get<float>(); np.pitch = s.get<float>();
            np.hp = s.get<float>(); np.maxhp = s.get<float>(); np.stam = s.get<float>(); np.rad = s.get<float>();
            np.level = s.get<int>(); np.xp = s.get<int>(); np.points = s.get<int>(); np.money = s.get<int>();
            for (int i = 0; i < AT_COUNT; i++) np.attr[i] = s.get<int>();
            np.weapon = s.get<int>(); np.armor = s.get<int>(); for (int i = 0; i < IT_COUNT; i++) np.mag[i] = s.get<int>();
            np.sneak = s.get<u8>() != 0; s.bytes(np.name, 32); np.name[31] = 0; np.bodyYaw = s.get<float>(); np.artifact = s.get<int>(); np.hunger = s.get<float>(); np.thirst = s.get<float>(); havePlayer = s.ok; break; }
        case TAG('I', 'N', 'V', 'T'): { u32 n = s.get<u32>(); for (u32 i = 0; i < n && s.ok && i < 4096; i++) { Slot sl; sl.id = s.get<int>(); sl.count = s.get<int>(); if (sl.id >= 0 && sl.id < IT_COUNT && sl.count > 0) inv.push_back(sl); } break; }
        case TAG('Q', 'S', 'T', 'S'): { for (int i = 0; i < Q_COUNT; i++) { nq[i].state = s.get<int>(); nq[i].stage = s.get<int>(); nq[i].count = s.get<int>(); } karma = s.get<int>(); kills = s.get<int>(); break; }
        case TAG('T', 'I', 'M', 'E'): hours = s.get<float>(); play = s.get<float>(); break;
        case TAG('W', 'R', 'L', 'D'): {
            seed = s.get<u32>(); u32 n = s.get<u32>(); for (u32 i = 0; i < n && s.ok; i++) { u64 k = s.get<u64>(); killed[k] = s.get<float>(); }
            n = s.get<u32>(); for (u32 i = 0; i < n && s.ok; i++) { u64 k = s.get<u64>(); looted[k] = s.get<float>(); }
            n = s.get<u32>(); for (u32 i = 0; i < n && s.ok; i++) disc.insert(s.get<u64>()); break; }
        case TAG('N', 'P', 'C', 'S'): { u32 n = s.get<u32>(); for (u32 i = 0; i < n && s.ok; i++) { u64 k = s.get<u64>(); NpcOvr o; o.x = s.get<float>(); o.z = s.get<float>(); o.hp = s.get<float>(); o.state = s.get<u8>(); ovr[k] = o; } break; }
        case TAG('C', 'A', 'M', 'R'): first = s.get<u8>() != 0; break;
        case TAG('V', 'E', 'H', 'S'): { u32 n = s.get<u32>(); for (u32 i = 0; i < n && s.ok; i++) { u64 k = s.get<u64>(); VehOvr o; o.type = s.get<int>(); o.x = s.get<float>(); o.z = s.get<float>(); o.yaw = s.get<float>(); o.fuel = s.get<float>(); o.hp = s.get<float>(); o.state = s.get<u8>(); if (o.type >= 0 && o.type < VT_COUNT) vovr[k] = o; } break; }
        default: break;                                                         // secao desconhecida (versao futura): ignora
        }
        if (!s.ok) { body.ok = false; }
    }
    if (!body.ok || !havePlayer) { Notify("Save incompleto.", 3); return false; }

    GameClearWorld(); WorldSetSeed(seed);
    G.p = np; G.p.inv = inv; G.p.vel = Vec3(); G.p.dead = false; G.p.onGround = true; if (G.p.hp <= 0) G.p.hp = 1;
    if (G.p.weapon < 0 || G.p.weapon >= IT_COUNT) G.p.weapon = IT_PIPE;
    memcpy(G.quests, nq, sizeof(nq)); G.karma = karma; G.kills = kills; G.gameHours = hours; G.playSeconds = play;
    G.killed = killed; G.looted = looted; G.discovered = disc; G.npcOvr = ovr; G.vehOvr = vovr; G.cam.first = first;
    if (G.p.artifact < 0 || G.p.artifact >= IT_COUNT) G.p.artifact = IT_NONE;
    G.simTime = 0; G.shotT = -100; G.autoT = 0; G.lastLocation = ""; G.locT = 0; G.shopNpc = 0;
    GameFinishLoadSetup();
    Notify(std::string("Jogo carregado: ") + si.name, 2);
    return true;
}

// ---------------------------------------------------------------------
//  Opcoes (settings.cfg ao lado do .exe)
// ---------------------------------------------------------------------
void LoadSettings() {
    FILE* f = fopen((ExeDir() + "/settings.cfg").c_str(), "r"); if (!f) return;
    char line[128]; Settings& s = G.set;
    while (fgets(line, sizeof(line), f)) {
        char k[64]; float v; if (sscanf(line, "%63[^=]=%f", k, &v) != 2) continue;
        if (!strcmp(k, "sens")) s.sens = Clampf(v, 0.2f, 4.0f); else if (!strcmp(k, "fov")) s.fov = Clampf(v, 60, 110);
        else if (!strcmp(k, "volume")) s.volume = Clampf(v, 0, 1); else if (!strcmp(k, "invertY")) s.invertY = v > 0.5f;
        else if (!strcmp(k, "showFps")) s.showFps = v > 0.5f; else if (!strcmp(k, "viewR")) s.viewR = Clampi((int)v, 3, 8);
    }
    fclose(f);
}
void SaveSettings() {
    FILE* f = fopen((ExeDir() + "/settings.cfg").c_str(), "w"); if (!f) return;
    const Settings& s = G.set;
    fprintf(f, "sens=%.2f\nfov=%.0f\nvolume=%.2f\ninvertY=%d\nshowFps=%d\nviewR=%d\n", s.sens, s.fov, s.volume, s.invertY ? 1 : 0, s.showFps ? 1 : 0, s.viewR);
    fclose(f);
}
