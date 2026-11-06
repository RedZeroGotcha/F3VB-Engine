// F3VB Map Viewer (v19): abre mapas .f3map (gerados por `convert-retail --maps`) e mostra a geometria 3D (.8tre), a planta (.rle),
// entidades, navegacao, gatilhos, caminhos, sons, marcadores, efeitos e agua. Win32 + OpenGL 1.x, sem bibliotecas externas, 64 bits.
// Escolha o mapa pelo dialogo (O), ou PgDn/PgUp para folhear os mapas da mesma pasta. Tudo e registrado em logs\f3mapviewer_log_*.txt.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <GL/gl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "f3map.h"
#include "f3tex.h"
#include "script_world.h"
#include "stf.h"
#include "log.h"

using namespace f3v;

namespace {

constexpr const char* kVersion = "F3VB Map Viewer v19";
enum { ID_OPEN = 100, ID_NEXT, ID_PREV, ID_QUIT, ID_GEOM, ID_FLOOR, ID_ENTS, ID_NAV, ID_TRIG, ID_PATHS, ID_MISC, ID_WATER, ID_WIRE, ID_LIGHT, ID_VCOL,
       ID_SELNEXT, ID_SELPREV, ID_FOCUS, ID_HOME, ID_TOP, ID_HELP, ID_REPORT, ID_CMODE, ID_DIALOG, ID_SURVEY, ID_DIALOG_NEXT, ID_TEXMODE };

struct App {
    HWND hwnd = nullptr; HDC hdc = nullptr; HGLRC rc = nullptr;
    int w = 1280, h = 800;
    GLuint font_base = 0, grid_tex = 0;
    std::uint32_t tex_w = 1, tex_h = 1;
    MapData map; bool loaded = false;
    std::string path, status;
    std::vector<std::string> siblings; int sib = -1;
    // camera
    float yaw = 35.f, pitch = 35.f, dist = 100.f, cx = 0, cy = 0, cz = 0, radius = 50.f;
    int mx = 0, my = 0; bool ldown = false, rdown = false;
    // camadas
    bool geom = true, floor_ = true, ents = true, nav = true, trig = true, paths = true, misc = true, water = true, wire = false, light = true, vcol = true, help = true;
    int sel = -1;
    // v16: modo de cor da geometria (M): 0 cores de vertice, 1 material, 2 pagina de lightmap, 3 textura difusa
    int cmode = 0;
    // v17: scripts (VM) ligados ao mapa: biblioteca de .amx, tabela de strings, instancia do script da entidade e sessao de dialogo
    // v18: texturas (X): 0 sem textura, 1 difusa, 2 difusa + lightmap (multitextura). Cache por nome (minusculo, sem extensao): 0 = ausente.
    int tmode = 0;
    bool tex_auto = false, mtex_ok = false;
    std::string tex_dir;
    std::map<std::string, GLuint> tex_cache;
    std::vector<GLuint> diffuse_ids, lightmap_ids;      // indice = (1-based - 1), alinhado com map.tga_names / map.ctx_names
    std::size_t tex_found = 0, tex_missing = 0;
    ScriptLibrary scripts;
    Stf stf;
    std::string stf_path;
    std::vector<ScriptRef> map_scr;
    std::unique_ptr<ScriptInstance> inst;
    std::unique_ptr<DialogSession> dlg;
    int dlg_ent = -1, dlg_browse = -1;
    std::string dlg_script;
    bool dlg_open = false;
    std::size_t dlg_logged_lines = 0, dlg_logged_trace = 0;
    std::map<std::uint32_t, std::size_t> cnt_mat, cnt_page, cnt_tex;     // triangulos por id
    double fps = 0, fps_acc = 0, uptime = 0, heartbeat = 0; int frames = 0; std::uint64_t total_frames = 0;
    LARGE_INTEGER freq, last;
    Log log;
} A;

#define LOGF(...) A.log.linef(__VA_ARGS__)

// ------------------------------------------------------------------------------------------------ texturas (v18)
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE0_ALPHA 0x8588
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND0_ALPHA 0x8598
#define GL_RGB_SCALE 0x8573
#define GL_PREVIOUS 0x8578
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif
#ifndef GL_REPLACE
#define GL_REPLACE 0x1E01
#endif
#ifndef GL_SRC_COLOR
#define GL_SRC_COLOR 0x0300
#endif
#ifndef GL_PRIMARY_COLOR
#define GL_PRIMARY_COLOR 0x8577
#endif
#ifndef GL_SOURCE1_ALPHA
#define GL_SOURCE1_ALPHA 0x8589
#define GL_OPERAND1_ALPHA 0x8599
#endif
typedef void(APIENTRY* PFN_ActiveTexture)(GLenum);
PFN_ActiveTexture p_glActiveTexture = nullptr, p_glClientActiveTexture = nullptr;

void compute_stats();
void log_tables(bool full);
std::size_t count_long_edges();

// ------------------------------------------------------------------------------------------------ utilidades
std::string exe_dir() {
    char b[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, b, MAX_PATH);
    const std::string s = b;
    const std::size_t p = s.find_last_of("\\/");
    return p == std::string::npos ? std::string(".") : s.substr(0, p);
}

std::string default_maps_dir() {
    char env[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("F3VB_ASSETS_DIR", env, MAX_PATH) > 0) return std::string(env) + "\\CONVERTED\\maps";
    return exe_dir() + "\\..\\..\\original_assets\\CONVERTED\\maps";      // exe em f3vb\\viewer\\build
}

std::string dir_of(const std::string& p) { const std::size_t k = p.find_last_of("\\/"); return k == std::string::npos ? std::string(".") : p.substr(0, k); }

std::vector<std::string> list_maps(const std::string& dir) {
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*.f3map").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(dir + "\\" + fd.cFileName); } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

void build_font() {
    HFONT f = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HGDIOBJ old = SelectObject(A.hdc, f);
    A.font_base = glGenLists(96);
    wglUseFontBitmapsA(A.hdc, 32, 96, A.font_base);
    SelectObject(A.hdc, old);
    DeleteObject(f);
}

void text(float x, float y, const std::string& s) {
    glRasterPos2f(x, y);
    glListBase(A.font_base - 32);
    glCallLists(static_cast<GLsizei>(s.size()), GL_UNSIGNED_BYTE, s.c_str());
}

std::uint32_t pow2_at_least(std::uint32_t v) { std::uint32_t p = 1; while (p < v && p < 8192) p <<= 1; return p; }

// cor de depuracao por id de tile (igual a rle.py: matiz = id % 11, brilho = id / 11)
void tile_rgb(int v, std::uint8_t out[3]) {
    if (v == 0x2F) { out[0] = 12; out[1] = 14; out[2] = 20; return; }
    const float hue = static_cast<float>(v % 11) / 11.f, l = 0.30f + 0.11f * static_cast<float>(std::min(v / 11, 4)), s = 0.55f;
    const float q = l < 0.5f ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
    auto f = [&](float t) {
        t -= std::floor(t);
        if (t < 1.f / 6) return p + (q - p) * 6 * t;
        if (t < 0.5f) return q;
        if (t < 2.f / 3) return p + (q - p) * (2.f / 3 - t) * 6;
        return p;
    };
    const float d[3] = {1.f / 3, 0.f, -1.f / 3};
    for (int i = 0; i < 3; ++i) out[i] = static_cast<std::uint8_t>(255.f * f(hue + d[i]));
}

void upload_grid_texture() {
    if (A.grid_tex) { glDeleteTextures(1, &A.grid_tex); A.grid_tex = 0; }
    if (A.map.grid_w == 0 || A.map.grid_rows == 0) return;
    A.tex_w = pow2_at_least(A.map.grid_w);
    A.tex_h = pow2_at_least(A.map.grid_rows);
    std::vector<std::uint8_t> px(static_cast<std::size_t>(A.tex_w) * A.tex_h * 4, 0);
    std::uint8_t pal[256][3];
    for (int v = 0; v < 256; ++v) tile_rgb(v, pal[v]);
    for (std::uint32_t r = 0; r < std::min(A.map.grid_rows, A.tex_h); ++r)
        for (std::uint32_t c = 0; c < std::min(A.map.grid_w, A.tex_w); ++c) {
            const std::uint8_t v = A.map.grid[static_cast<std::size_t>(r) * A.map.grid_w + c];
            std::uint8_t* d = &px[(static_cast<std::size_t>(r) * A.tex_w + c) * 4];
            d[0] = pal[v][0]; d[1] = pal[v][1]; d[2] = pal[v][2]; d[3] = v == 0x2F ? 40 : 255;
        }
    glGenTextures(1, &A.grid_tex);
    glBindTexture(GL_TEXTURE_2D, A.grid_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(A.tex_w), static_cast<GLsizei>(A.tex_h), 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
}

void fit_camera() {
    float mn[3], mx[3];
    if (!A.map.bounds(mn, mx)) { A.cx = A.cy = A.cz = 0; A.radius = 50; A.dist = 100; return; }
    A.cx = 0.5f * (mn[0] + mx[0]); A.cy = 0.5f * (mn[1] + mx[1]); A.cz = 0.5f * (mn[2] + mx[2]);
    const float dx = mx[0] - mn[0], dy = mx[1] - mn[1], dz = mx[2] - mn[2];
    A.radius = std::max(1.f, 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz));
    A.dist = A.radius * 2.2f;
}

void set_title() {
    std::string t = kVersion;
    if (A.loaded) t += " - " + (A.map.name.empty() ? std::string("(sem nome)") : A.map.name);
    SetWindowTextA(A.hwnd, t.c_str());
}

// ------------------------------------------------------------------------------------------------ carga
std::string tex_key(const std::string& name) {
    std::string k;
    for (char c : name) k.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    return k;
}

GLuint upload_texture(const Image& img, bool clamp) {
    const std::vector<Image> mips = build_mips(img);
    if (mips.empty()) return 0;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    for (std::size_t l = 0; l < mips.size(); ++l)
        glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(l), GL_RGBA, static_cast<GLsizei>(mips[l].w), static_cast<GLsizei>(mips[l].h), 0, GL_RGBA, GL_UNSIGNED_BYTE, mips[l].rgba.data());
    return id;
}

// carrega (com cache) CONVERTED\textures\<nome>.f3tex; 0 = nao existe / nao decodificou
GLuint get_texture(const std::string& name, bool clamp, std::string& problem) {
    const std::string key = tex_key(name);
    const auto it = A.tex_cache.find(key);
    if (it != A.tex_cache.end()) return it->second;
    std::string file;
    for (char c : name) file.push_back((c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') ? '_' : c);
    Image img;
    std::string err;
    GLuint id = 0;
    if (!A.tex_dir.empty() && load_f3tex_file(A.tex_dir + "\\" + file + ".f3tex", img, err)) id = upload_texture(img, clamp);
    else problem = err;
    A.tex_cache[key] = id;
    return id;
}

void init_gl_extensions() {
    p_glActiveTexture = reinterpret_cast<PFN_ActiveTexture>(reinterpret_cast<void*>(wglGetProcAddress("glActiveTexture")));
    p_glClientActiveTexture = reinterpret_cast<PFN_ActiveTexture>(reinterpret_cast<void*>(wglGetProcAddress("glClientActiveTexture")));
    A.mtex_ok = p_glActiveTexture && p_glClientActiveTexture;
    LOGF("multitextura (glActiveTexture): %s", A.mtex_ok ? "sim (lightmaps disponiveis)" : "NAO (so textura difusa)");
}

void load_textures_for_map() {
    A.diffuse_ids.clear(); A.lightmap_ids.clear();
    A.tex_found = A.tex_missing = 0;
    A.tex_dir = dir_of(dir_of(A.path)) + "\\textures";
    const DWORD t0 = GetTickCount();
    std::string missing;
    std::size_t nmiss = 0, found_d = 0, found_l = 0;
    auto one = [&](const std::string& name, bool lightmap) -> GLuint {
        std::string problem;
        const GLuint id = get_texture(name, lightmap, problem);
        if (id) { ++A.tex_found; (lightmap ? found_l : found_d)++; }
        else { ++A.tex_missing; ++nmiss; if (nmiss <= 8) missing += (missing.empty() ? "" : "; ") + name + (lightmap ? " (lightmap)" : ""); }
        return id;
    };
    for (const std::string& n : A.map.tga_names) A.diffuse_ids.push_back(one(n, false));
    for (const std::string& n : A.map.ctx_names) A.lightmap_ids.push_back(one(n, true));
    const bool has_uv = !A.map.blocks.empty() && !A.map.blocks[0].uv.empty();
    LOGF("texturas (v18): pasta %s | difusas %zu de %zu | lightmaps %zu de %zu | faltando %zu%s%s | UVs no .f3map: %s | %lu ms", A.tex_dir.c_str(), found_d, A.map.tga_names.size(), found_l,
         A.map.ctx_names.size(), nmiss, nmiss ? ": " : "", missing.c_str(), has_uv ? "sim (v3)" : "NAO (reconverta: py -3 -m f3vb convert-retail --maps)", static_cast<unsigned long>(GetTickCount() - t0));
    if (A.tex_found == 0 && (!A.map.tga_names.empty() || !A.map.ctx_names.empty())) LOGF("AVISO: nenhuma textura carregada. Rode: py -3 -m f3vb export-textures");
    if (A.tex_found > 0 && has_uv && A.tmode == 0 && !A.tex_auto) {
        A.tmode = A.mtex_ok ? 2 : 1; A.tex_auto = true;
        A.floor_ = false;                         // v19: a "planta" (.rle) por cima do chao texturizado era parte das manchas claras: desligada (R liga)
        LOGF("texturas ligadas (modo %s); X alterna: sem textura / difusa / difusa + lightmap | planta (R) desligada", A.tmode == 2 ? "difusa + lightmap" : "difusa");
    }
    {
        std::size_t no_state = 0;
        for (const MapTriList& tl : A.map.tris) if (tl.material == 0 && tl.page == 0 && tl.tex == 0) no_state += tl.idx.size() / 3;
        if (no_state) LOGF("AVISO: %zu triangulos sem material/pagina/textura (o 'lote inicial' do v16-v18). Reconverta: py -3 -m f3vb convert-retail --maps", no_state);
    }
}

// ------------------------------------------------------------------------------------------------ scripts / VM (v17)
std::string first_file(const std::string& dir, const char* pattern) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\" + pattern).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return std::string();
    std::string r;
    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { r = dir + "\\" + fd.cFileName; break; } } while (FindNextFileA(h, &fd));
    FindClose(h);
    return r;
}

void load_scripts_for_map() {
    A.dlg.reset(); A.inst.reset(); A.dlg_open = false; A.dlg_ent = -1;
    // pasta dos scripts: irma da pasta dos mapas (CONVERTED\maps -> CONVERTED\scripts); senao a padrao
    const std::string maps_dir = dir_of(A.path);
    const std::string cand1 = dir_of(maps_dir) + "\\scripts";
    const std::string cand2 = default_maps_dir().substr(0, default_maps_dir().find_last_of("\\/")) + "\\scripts";
    std::size_t n = A.scripts.scan(cand1);
    if (n == 0) n = A.scripts.scan(cand2);
    // tabela de strings: F3VB_STF, ou o primeiro .stf na pasta de scripts, ou na pasta original_assets
    A.stf = Stf();
    A.stf_path.clear();
    char env[MAX_PATH] = {0};
    std::string stf_file;
    if (GetEnvironmentVariableA("F3VB_STF", env, MAX_PATH) > 0) stf_file = env;
    if (stf_file.empty()) stf_file = first_file(A.scripts.dir(), "*.stf");
    if (stf_file.empty()) stf_file = first_file(dir_of(dir_of(A.scripts.dir())), "*.stf");
    std::string err;
    if (!stf_file.empty()) {
        if (A.stf.load_file(stf_file, err)) A.stf_path = stf_file;
        else LOGF("AVISO: .stf %s nao carregou: %s", stf_file.c_str(), err.c_str());
    }
    A.map_scr = map_scripts(A.map, A.scripts);
    std::size_t ents = 0, found = 0;
    std::string missing;
    std::size_t nmiss = 0;
    for (const ScriptRef& r : A.map_scr) {
        ents += r.entities;
        if (!r.path.empty()) ++found;
        else { ++nmiss; if (nmiss <= 12) missing += (missing.empty() ? "" : "; ") + r.name; }
    }
    LOGF("scripts (v17): pasta %s | %zu .amx | tabela de strings: %s (%zu textos) | o mapa cita %zu scripts distintos em %zu entidades: %zu encontrados, %zu faltando%s%s",
         A.scripts.dir().c_str(), n, A.stf_path.empty() ? "AUSENTE (as falas aparecem como [texto #id])" : A.stf_path.c_str(), A.stf.count(), A.map_scr.size(), ents, found, nmiss,
         nmiss ? ": " : "", missing.c_str());
    if (n == 0) LOGF("AVISO: nenhum .amx em %s nem em %s. Rode: py -3 -m f3vb export-scripts", cand1.c_str(), cand2.c_str());
}

std::string speech_text(const DialogLine& l) {
    if (!l.text.empty()) return l.text;
    return "[texto #" + std::to_string(l.text_id) + (A.stf.loaded() ? " fora da tabela]" : " - sem .stf]");
}

void log_dialog_progress() {
    if (!A.dlg || !A.inst) return;
    for (; A.dlg_logged_trace < A.dlg->trace.size(); ++A.dlg_logged_trace) LOGF("  [VM] %s", A.dlg->trace[A.dlg_logged_trace].c_str());
    for (; A.dlg_logged_lines < A.dlg->lines().size(); ++A.dlg_logged_lines) {
        const DialogLine& l = A.dlg->lines()[A.dlg_logged_lines];
        LOGF("  [fala] %s: %s", l.who.c_str(), speech_text(l).c_str());
    }
    if (!A.dlg->error().empty()) LOGF("  [VM] ERRO: %s", A.dlg->error().c_str());
    if (A.dlg->finished()) LOGF("  [dialogo] terminou");
    else for (std::size_t i = 0; i < A.dlg->choices().size(); ++i) {
        const DialogChoice& c = A.dlg->choices()[i];
        LOGF("  [resposta %zu] #%d texto %d '%s' acao='%s' destino='%s'", i + 1, c.reply_id, c.text_id, c.text.c_str(), c.action_fn.c_str(), c.link_fn.c_str());
    }
}

void close_dialog() {
    if (A.dlg_open) LOGF("dialogo fechado (entidade #%d)", A.dlg_ent);
    A.dlg_open = false;
    A.dlg.reset();
    A.inst.reset();
}

// browse = false (V): usa o script da entidade; se ele nao tem OnStartDialogue, procura por nome um script de dialogo parecido.
// browse = true  (K): passa para o PROXIMO script de dialogo da biblioteca (para experimentar qualquer dialogo na entidade selecionada).
void open_dialog(bool browse) {
    if (!A.loaded || A.sel < 0 || A.sel >= static_cast<int>(A.map.entities.size())) { A.status = "Selecione uma entidade (Tab) antes de abrir o dialogo (V)"; return; }
    const MapEntity& e = A.map.entities[static_cast<std::size_t>(A.sel)];
    std::string path, name, why;
    if (browse) {
        const auto& all = A.scripts.dialogue_scripts();
        if (all.empty()) { A.status = "Nenhum script com OnStartDialogue em " + A.scripts.dir(); LOGF("%s", A.status.c_str()); return; }
        A.dlg_browse = (A.dlg_browse + 1) % static_cast<int>(all.size());
        const auto& pick = all[static_cast<std::size_t>(A.dlg_browse)];
        path = pick.path; name = pick.name;
        why = "escolhido pela tecla K (" + std::to_string(A.dlg_browse + 1) + "/" + std::to_string(all.size()) + ")";
    } else {
        path = e.script.empty() ? std::string() : A.scripts.find(e.script);
        name = e.script;
        why = "script da entidade";
    }
    std::string err;
    A.dlg.reset();
    if (!path.empty()) A.inst = ScriptInstance::from_file(path, name, 1000 + A.sel, 1, err);
    else A.inst.reset();
    if (!browse && (!A.inst || A.inst->prog->find_public("OnStartDialogue") < 0)) {
        // O script de comportamento (CaptainArmstrong.amx, Template_*) normalmente nao tem dialogo: procura o script de dialogo por nome
        const std::string hay = e.prototype + " " + e.instance + " " + e.name;
        const auto cand = A.scripts.match_dialogue(hay);
        if (!cand.empty()) {
            LOGF("DIALOGO: o script da entidade ('%s') nao tem OnStartDialogue; candidatos por nome: %zu (usando '%s')", e.script.c_str(), cand.size(), cand[0].name.c_str());
            for (std::size_t k = 1; k < cand.size() && k < 6; ++k) LOGF("  outro candidato: %s", cand[k].name.c_str());
            path = cand[0].path; name = cand[0].name; why = "dialogo encontrado por nome (heuristica)";
            A.inst = ScriptInstance::from_file(path, name, 1000 + A.sel, 1, err);
        } else {
            std::string lista;
            const auto& all = A.scripts.dialogue_scripts();
            for (std::size_t k = 0; k < all.size() && k < 40; ++k) lista += (lista.empty() ? "" : "; ") + all[k].name;
            A.status = "Sem script de dialogo para esta entidade (o dela nao tem OnStartDialogue). K experimenta qualquer dialogo (" + std::to_string(all.size()) + " scripts)";
            LOGF("DIALOGO: '%s' nao tem OnStartDialogue e nenhum nome parece combinar com '%s'. Scripts com dialogo (%zu): %s", e.script.c_str(), hay.c_str(), all.size(), lista.c_str());
            A.inst.reset();
            return;
        }
    }
    if (!A.inst) {
        A.status = path.empty() ? "A entidade selecionada nao tem script (use K para experimentar um dialogo)" : "ERRO ao carregar o script: " + err;
        LOGF("%s %s", A.status.c_str(), path.c_str());
        return;
    }
    LOGF("DIALOGO: entidade #%d %s inst='%s' | script '%s' (%s) | %s: %zu publics, %zu natives", A.sel, e.prototype.c_str(), e.instance.c_str(), name.c_str(), path.c_str(), why.c_str(),
         A.inst->prog->publics().size(), A.inst->prog->natives().size());
    A.dlg.reset(new DialogSession(*A.inst, &A.stf));
    A.dlg_ent = A.sel;
    A.dlg_script = name;
    A.dlg_logged_lines = A.dlg_logged_trace = 0;
    A.dlg_open = true;
    A.dlg->start();
    log_dialog_progress();
    for (const auto& kv : A.inst->host->unimplemented_calls()) LOGF("  [natives sem implementar] %s x%d", kv.first.c_str(), kv.second);
    A.status = A.dlg->finished() ? "Dialogo terminou (veja o log; K tenta outro script)" : "Dialogo aberto: 1-9 escolhe a resposta, V fecha, K troca de script";
}

void dialog_choose(std::size_t i) {
    if (!A.dlg_open || !A.dlg || !A.inst) return;
    if (!A.dlg->choose(i)) { A.status = A.dlg->finished() ? "Dialogo terminado: V fecha" : "Resposta inexistente"; return; }
    LOGF("resposta escolhida: %zu", i + 1);
    log_dialog_progress();
    for (const auto& kv : A.inst->host->unimplemented_calls()) LOGF("  [natives sem implementar] %s x%d", kv.first.c_str(), kv.second);
    A.status = A.dlg->finished() ? "Dialogo terminou: V fecha" : "Dialogo: 1-9 escolhe a resposta, V fecha";
}

// Roda todas as publics de cada script citado pelo mapa e resume (erros da VM e natives que faltam implementar)
void survey_map_scripts() {
    if (!A.loaded) return;
    std::size_t ok = 0, bad = 0, missing = 0;
    std::map<std::string, int> all_unimpl;
    LOGF("LEVANTAMENTO DE SCRIPTS do mapa '%s' (%zu scripts distintos)", A.map.name.c_str(), A.map_scr.size());
    for (const ScriptRef& r : A.map_scr) {
        if (r.path.empty()) { ++missing; LOGF("  %-34s %zu entidades | .amx NAO ENCONTRADO", r.name.c_str(), r.entities); continue; }
        std::string err;
        std::unique_ptr<ScriptInstance> si = ScriptInstance::from_file(r.path, r.name, 1000, 1, err);
        if (!si) { ++bad; LOGF("  %-34s %zu entidades | NAO CARREGOU: %s", r.name.c_str(), r.entities, err.c_str()); continue; }
        const ScriptSurvey sv = survey_script(*si);
        (sv.ran_fail || !sv.unresolved.empty() ? bad : ok)++;
        std::string un;
        for (const auto& kv : sv.unimplemented) { un += (un.empty() ? "" : ", ") + kv.first + "x" + std::to_string(kv.second); all_unimpl[kv.first] += kv.second; }
        LOGF("  %-34s %zu entidades | %zu publics (%zu ok, %zu com erro) | dialogo=%s | natives sem implementar: %s", r.name.c_str(), r.entities, sv.publics, sv.ran_ok, sv.ran_fail,
             sv.has_dialogue ? "sim" : "nao", un.empty() ? "(nenhuma)" : un.c_str());
        for (const auto& kv : sv.unimplemented) {
            const auto it = sv.samples.find(kv.first);
            if (it != sv.samples.end() && !it->second.empty()) LOGF("      %s x%d ex.: %s", kv.first.c_str(), kv.second, it->second[0].c_str());
        }
        for (const std::string& f : sv.failures) LOGF("      erro: %s", f.c_str());
        for (const std::string& f : sv.unresolved) LOGF("      native importada e nao registrada: %s", f.c_str());
    }
    std::string tot;
    for (const auto& kv : all_unimpl) tot += (tot.empty() ? "" : ", ") + kv.first + "x" + std::to_string(kv.second);
    LOGF("  resumo: %zu scripts ok, %zu com problema, %zu sem .amx | natives sem implementar (soma): %s", ok, bad, missing, tot.empty() ? "(nenhuma)" : tot.c_str());
    A.status = "Levantamento de scripts gravado no log (" + std::to_string(ok) + " ok, " + std::to_string(bad) + " com problema, " + std::to_string(missing) + " sem .amx)";
}

bool load_map(const std::string& path) {
    const auto t0 = std::chrono::steady_clock::now();
    LOGF("ABRIR: %s", path.c_str());
    MapData m;
    std::string err;
    if (!load_f3map_file(path, m, err)) { A.status = "ERRO: " + err; LOGF("ERRO ao abrir: %s", err.c_str()); return false; }
    A.map = std::move(m);
    A.loaded = true;
    A.path = path;
    A.sel = -1;
    upload_grid_texture();
    fit_camera();
    set_title();
    A.siblings = list_maps(dir_of(path));
    A.sib = -1;
    for (std::size_t i = 0; i < A.siblings.size(); ++i) if (_stricmp(A.siblings[i].c_str(), path.c_str()) == 0) A.sib = static_cast<int>(i);
    std::size_t hits = 0, total = 0;
    A.map.floor_hits(hits, total);
    float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
    const bool has = A.map.bounds(mn, mx);
    LOGF("mapa '%s': tre='%s' rle='%s' origem (%.2f, %.2f) celula %.2f | grade %ux%u | blocos %zu, triangulos %zu | entidades %zu, nos de navegacao %zu, entradas %zu, gatilhos %zu, caminhos %zu, sons %zu, marcadores %zu, efeitos %zu, agua %zu, luzes %zu",
         A.map.name.c_str(), A.map.tre.c_str(), A.map.rle.c_str(), static_cast<double>(A.map.origin[0]), static_cast<double>(A.map.origin[1]), static_cast<double>(A.map.cell), A.map.grid_w, A.map.grid_rows,
         A.map.blocks.size(), A.map.triangle_count(), A.map.entities.size(), A.map.nav.size(), A.map.entries.size(), A.map.triggers.size(), A.map.paths.size(), A.map.sounds.size(),
         A.map.markers.size(), A.map.effects.size(), A.map.water.size(), A.map.lights.size());
    if (has) LOGF("caixa: x[%.1f, %.1f] y[%.1f, %.1f] z[%.1f, %.1f]", static_cast<double>(mn[0]), static_cast<double>(mx[0]), static_cast<double>(mn[1]), static_cast<double>(mx[1]), static_cast<double>(mn[2]), static_cast<double>(mx[2]));
    LOGF("regra mundo->celula (celula = (pos - origem) / %.2f): %zu de %zu pontos (entidades, navegacao, entradas) em celulas nao vazias", static_cast<double>(A.map.cell), hits, total);
    load_scripts_for_map();
    load_textures_for_map();
    compute_stats();
    if (!A.map.tris.empty()) {
        const std::size_t le = count_long_edges();
        LOGF("espinhos: %zu de %zu triangulos (%.3f%%) tem uma aresta maior que 20%% da diagonal do mapa (no v15 isso era o sintoma dos indices lidos errado)", le, A.map.triangle_count(),
             100.0 * static_cast<double>(le) / static_cast<double>(std::max<std::size_t>(1, A.map.triangle_count())));
        log_tables(false);
    }
    if (A.map.blocks.empty()) LOGF("AVISO: sem geometria 3D (.8tre nao encontrado ao converter); so a planta e os objetos serao mostrados");
    if (A.map.grid.empty()) LOGF("AVISO: sem grade (.rle nao encontrado ao converter)");
    for (std::size_t i = 0; i < A.map.triggers.size(); ++i)
        if (A.map.triggers[i].kind == 2) LOGF("gatilho de viagem #%zu -> mapa '%s', ponto de entrada %d", i, A.map.triggers[i].target.c_str(), A.map.triggers[i].entry);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    char b[256];
    std::snprintf(b, sizeof b, "Mapa '%s' aberto em %.0f ms: %zu triangulos, %zu entidades", A.map.name.c_str(), ms, A.map.triangle_count(), A.map.entities.size());
    A.status = b;
    LOGF("%s", b);
    return true;
}

void cmd_open() {
    char file[MAX_PATH] = {0};
    const std::string init = A.path.empty() ? default_maps_dir() : dir_of(A.path);
    OPENFILENAMEA o;
    std::memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = A.hwnd;
    o.lpstrFilter = "Mapas F3VB (*.f3map)\0*.f3map\0Todos os arquivos\0*.*\0\0";
    o.lpstrFile = file;
    o.nMaxFile = MAX_PATH;
    o.lpstrInitialDir = init.c_str();
    o.lpstrTitle = "Escolha um mapa";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&o)) load_map(file);
    else { A.status = "Nenhum mapa escolhido. Aperte O para abrir."; LOGF("dialogo de abrir cancelado"); }
}

void step_map(int delta) {
    if (A.siblings.empty()) { A.status = "Nao ha outros mapas nesta pasta"; return; }
    const int n = static_cast<int>(A.siblings.size());
    const int k = ((A.sib < 0 ? 0 : A.sib) + delta + n) % n;
    load_map(A.siblings[static_cast<std::size_t>(k)]);
}

// ------------------------------------------------------------------------------------------------ desenho
void box(float x, float y, float z, float w, float h, const GLfloat c[3]) {
    const float a = w * 0.5f;
    glColor3fv(c);
    glBegin(GL_QUADS);
    glVertex3f(x - a, y, z - a); glVertex3f(x + a, y, z - a); glVertex3f(x + a, y + h, z - a); glVertex3f(x - a, y + h, z - a);
    glVertex3f(x - a, y, z + a); glVertex3f(x + a, y, z + a); glVertex3f(x + a, y + h, z + a); glVertex3f(x - a, y + h, z + a);
    glVertex3f(x - a, y, z - a); glVertex3f(x - a, y, z + a); glVertex3f(x - a, y + h, z + a); glVertex3f(x - a, y + h, z - a);
    glVertex3f(x + a, y, z - a); glVertex3f(x + a, y, z + a); glVertex3f(x + a, y + h, z + a); glVertex3f(x + a, y + h, z - a);
    glVertex3f(x - a, y + h, z - a); glVertex3f(x + a, y + h, z - a); glVertex3f(x + a, y + h, z + a); glVertex3f(x - a, y + h, z + a);
    glEnd();
}

void kind_color(const std::string& k, GLfloat c[3]) {
    c[0] = 1; c[1] = 1; c[2] = 1;
    if (k == "CRT") { c[0] = 1.f; c[1] = 0.27f; c[2] = 0.27f; }
    else if (k == "WEA") { c[0] = 1.f; c[1] = 0.63f; c[2] = 0.16f; }
    else if (k == "AMO") { c[0] = 1.f; c[1] = 0.78f; c[2] = 0.31f; }
    else if (k == "ARM") { c[0] = 0.9f; c[1] = 0.55f; c[2] = 0.24f; }
    else if (k == "ITM") { c[0] = 1.f; c[1] = 0.47f; c[2] = 0.78f; }
    else if (k == "DOR") { c[0] = 0.31f; c[1] = 0.55f; c[2] = 1.f; }
    else if (k == "CON") { c[0] = 0.94f; c[1] = 0.9f; c[2] = 0.27f; }
    else if (k == "USE") { c[0] = 0.78f; c[1] = 1.f; c[2] = 0.31f; }
}

float floor_y() {
    float mn[3], mx[3];
    return A.map.bounds(mn, mx) ? mn[1] : 0.f;
}

// ------------------------------------------------------------------------------------------------ modos de cor (v16)
const char* cmode_name(int m) {
    switch (m) { case 1: return "material"; case 2: return "pagina de lightmap"; case 3: return "textura difusa"; default: return "cor de vertice"; }
}

// id do lote conforme o modo (1 material, 2 pagina de lightmap, 3 textura difusa)
std::uint32_t list_id(const MapTriList& tl, int mode) { return mode == 1 ? tl.material : (mode == 2 ? tl.page : tl.tex); }

// cor distinta por id (matiz pelo numero aureo); 0 de pagina/textura = "nenhuma" = cinza
void id_color(std::uint32_t id, int mode, GLfloat c[3]) {
    if (mode != 1 && id == 0) { c[0] = c[1] = c[2] = 0.55f; return; }
    const float h = std::fmod(static_cast<float>(id) * 0.61803399f + 0.08f, 1.f), s = 0.62f, v = 0.95f;
    const float hh = h * 6.f;
    const int i = static_cast<int>(hh);
    const float f = hh - static_cast<float>(i), p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i % 6) {
        case 0: c[0] = v; c[1] = t; c[2] = p; break;
        case 1: c[0] = q; c[1] = v; c[2] = p; break;
        case 2: c[0] = p; c[1] = v; c[2] = t; break;
        case 3: c[0] = p; c[1] = q; c[2] = v; break;
        case 4: c[0] = t; c[1] = p; c[2] = v; break;
        default: c[0] = v; c[1] = p; c[2] = q; break;
    }
}

void list_color(const MapTriList& tl, GLfloat c[3]) { id_color(list_id(tl, A.cmode), A.cmode, c); }

const std::map<std::uint32_t, std::size_t>& counts_for(int mode) { return mode == 1 ? A.cnt_mat : (mode == 2 ? A.cnt_page : A.cnt_tex); }

std::string id_label(std::uint32_t id, int mode) {
    char b[320];
    if (mode == 1) {
        if (id < A.map.materials.size()) {
            const MapMaterial& m = A.map.materials[id];
            std::snprintf(b, sizeof b, "#%u %s (%s, %s)", id, m.name.c_str(), m.blend.c_str(), m.surface.c_str());
        } else std::snprintf(b, sizeof b, "#%u (fora da tabela de materiais)", id);
    } else if (mode == 2) {
        if (id == 0) return "sem lightmap";
        const std::string n = A.map.lightmap_name(id);
        std::snprintf(b, sizeof b, "pagina %u: %s", id, n.empty() ? "(fora da lista .ctx)" : n.c_str());
    } else {
        if (id == 0) return "sem textura difusa";
        const std::string n = A.map.diffuse_name(id);
        std::snprintf(b, sizeof b, "textura %u: %s", id, n.empty() ? "(fora da lista .tga)" : n.c_str());
    }
    return b;
}

void compute_stats() {
    A.cnt_mat.clear(); A.cnt_page.clear(); A.cnt_tex.clear();
    for (const MapTriList& tl : A.map.tris) {
        const std::size_t nt = tl.idx.size() / 3;
        A.cnt_mat[tl.material] += nt; A.cnt_page[tl.page] += nt; A.cnt_tex[tl.tex] += nt;
    }
}

// tabelas de materiais / paginas / texturas com o numero de triangulos (log ao abrir e no F2)
void log_tables(bool full) {
    if (A.map.tris.empty()) return;
    LOGF("lotes (v%u do .f3map): %zu listas de triangulos | %zu materiais na tabela, %zu usados | %zu paginas de lightmap usadas (de %zu .ctx) | %zu texturas difusas usadas (de %zu .tga)",
         A.map.version, A.map.tris.size(), A.map.materials.size(), A.cnt_mat.size(), A.cnt_page.size() - (A.cnt_page.count(0) ? 1u : 0u), A.map.ctx_names.size(),
         A.cnt_tex.size() - (A.cnt_tex.count(0) ? 1u : 0u), A.map.tga_names.size());
    if (A.map.version < 2) { LOGF("AVISO: .f3map v1 (sem material/pagina/textura); reconverta com: py -3 -m f3vb convert-retail --maps"); return; }
    for (const auto& kv : A.cnt_mat) LOGF("  material %s: %zu triangulos", id_label(kv.first, 1).c_str(), kv.second);
    for (const auto& kv : A.cnt_tex) LOGF("  %s: %zu triangulos", id_label(kv.first, 3).c_str(), kv.second);
    std::size_t shown = 0;
    for (const auto& kv : A.cnt_page) {
        if (!full && shown >= 12) { LOGF("  (... mais %zu paginas de lightmap; F2 lista todas)", A.cnt_page.size() - shown); break; }
        LOGF("  %s: %zu triangulos", id_label(kv.first, 2).c_str(), kv.second);
        ++shown;
    }
    for (std::size_t i = 0; i < A.map.materials.size(); ++i)
        if (!A.cnt_mat.count(static_cast<std::uint32_t>(i))) LOGF("  material #%zu %s: nenhum triangulo", i, A.map.materials[i].name.c_str());
}

// triangulos com uma aresta maior que 20% da diagonal do mapa: sintoma dos "espinhos" (indices lidos errado)
std::size_t count_long_edges() {
    const float lim = 0.4f * A.radius, lim2 = lim * lim;        // radius = meia diagonal
    std::size_t n = 0;
    for (const MapTriList& tl : A.map.tris) {
        const MapBlock& b = A.map.blocks[tl.block];
        for (std::size_t i = 0; i + 2 < tl.idx.size(); i += 3) {
            const float* p[3] = {&b.xyz[tl.idx[i] * 3], &b.xyz[tl.idx[i + 1] * 3], &b.xyz[tl.idx[i + 2] * 3]};
            bool longe = false;
            for (int e = 0; e < 3 && !longe; ++e) {
                const float* a = p[e]; const float* c = p[(e + 1) % 3];
                const float dx = a[0] - c[0], dy = a[1] - c[1], dz = a[2] - c[2];
                longe = dx * dx + dy * dy + dz * dz > lim2;
            }
            if (longe) ++n;
        }
    }
    return n;
}

// Geometria com texturas (X) - v19: MESMA formula do jogo (map_render.cpp). A luz da demo e "assada": nos materiais com lightmap a cor do vertice e
// sempre 127 (neutro) e a luz vem do lightmap; nos outros (BASE_2X, NO_TEXTURE) a luz esta na cor do vertice. Por isso: unidade 0 = (textura ou branco)
// x cor do vertice x 2; unidade 1 = x lightmap x 2; e SEM a luz do OpenGL (a tecla L so vale nos modos sem textura).
// MISTURA E CORTE SAO SUPOSICOES: ALPHABLEND por ultimo com mistura por alfa; IMAGE_ALPHA com corte de alfa 0,5.
GLuint white_texture() {
    static GLuint id = 0;
    if (!id) { const Image w{1, 1, {255, 255, 255, 255}}; id = upload_texture(w, false); }
    return id;
}
void combine_modulate(GLenum src0, GLenum src1, int scale) {
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, static_cast<GLint>(src0));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, static_cast<GLint>(src1));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, static_cast<GLint>(src0));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, static_cast<GLint>(src1));
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, scale);
}
void draw_geometry_textured() {
    glPolygonMode(GL_FRONT_AND_BACK, A.wire ? GL_LINE : GL_FILL);
    const bool lm = A.tmode == 2 && A.mtex_ok;
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    auto unit = [&](int u) { if (A.mtex_ok) { p_glActiveTexture(u ? GL_TEXTURE1 : GL_TEXTURE0); p_glClientActiveTexture(u ? GL_TEXTURE1 : GL_TEXTURE0); } };
    const GLuint white = white_texture();
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
        for (const MapTriList& tl : A.map.tris) {
            const MapMaterial* mat = tl.material < A.map.materials.size() ? &A.map.materials[tl.material] : nullptr;
            const bool blended = mat && mat->blend == "ALPHABLEND";
            if (blended != (pass == 1)) continue;
            const MapBlock& b = A.map.blocks[tl.block];
            if (b.uv.empty()) continue;
            const bool cutout = mat && mat->name.find("IMAGE_ALPHA") != std::string::npos;
            const GLuint dtex = tl.tex >= 1 && tl.tex <= A.diffuse_ids.size() ? A.diffuse_ids[tl.tex - 1] : 0;
            const GLuint ltex = lm && tl.page >= 1 && tl.page <= A.lightmap_ids.size() ? A.lightmap_ids[tl.page - 1] : 0;
            glVertexPointer(3, GL_FLOAT, 0, b.xyz.data());
            if (A.vcol) glColorPointer(4, GL_UNSIGNED_BYTE, 0, b.rgba.data());
            else { glDisableClientState(GL_COLOR_ARRAY); glColor4ub(127, 127, 127, 255); }
            unit(0);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, dtex ? dtex : white);
            combine_modulate(0x1702 /* GL_TEXTURE */, GL_PRIMARY_COLOR, 2);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, b.uv.data());
            if (A.mtex_ok) {
                unit(1);
                if (ltex && !b.lmuv.empty()) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, ltex);
                    combine_modulate(GL_PREVIOUS, 0x1702, 2);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glTexCoordPointer(2, GL_FLOAT, 0, b.lmuv.data());
                } else { glDisable(GL_TEXTURE_2D); glDisableClientState(GL_TEXTURE_COORD_ARRAY); }
                unit(0);
            }
            if (cutout) { glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f); } else glDisable(GL_ALPHA_TEST);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(tl.idx.size()), GL_UNSIGNED_INT, tl.idx.data());
            if (!A.vcol) glEnableClientState(GL_COLOR_ARRAY);
        }
    }
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    if (A.mtex_ok) { unit(1); glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1); glDisable(GL_TEXTURE_2D); glDisableClientState(GL_TEXTURE_COORD_ARRAY); }
    unit(0);
    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void draw_geometry() {
    if (!A.geom || A.map.tris.empty()) return;
    if (A.tmode > 0 && !A.map.blocks.empty() && !A.map.blocks[0].uv.empty()) { draw_geometry_textured(); return; }
    glPolygonMode(GL_FRONT_AND_BACK, A.wire ? GL_LINE : GL_FILL);
    if (A.light) { glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_COLOR_MATERIAL); glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1); }
    else glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    if (A.cmode == 0 && A.vcol) glEnableClientState(GL_COLOR_ARRAY);
    for (const MapTriList& tl : A.map.tris) {
        const MapBlock& b = A.map.blocks[tl.block];
        glVertexPointer(3, GL_FLOAT, 0, b.xyz.data());
        glNormalPointer(GL_FLOAT, 0, b.nrm.data());
        if (A.cmode != 0) { GLfloat c[3]; list_color(tl, c); glColor3fv(c); }
        else if (A.vcol) glColorPointer(4, GL_UNSIGNED_BYTE, 0, b.rgba.data());
        else glColor3f(0.65f, 0.65f, 0.68f);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(tl.idx.size()), GL_UNSIGNED_INT, tl.idx.data());
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void draw_floor() {
    if (!A.floor_ || !A.grid_tex) return;
    const float y = floor_y() - 0.03f;
    const float x0 = A.map.origin[0], z0 = A.map.origin[1];
    const float x1 = x0 + static_cast<float>(A.map.grid_w) * A.map.cell, z1 = z0 + static_cast<float>(A.map.grid_rows) * A.map.cell;
    const float u = static_cast<float>(A.map.grid_w) / static_cast<float>(A.tex_w), v = static_cast<float>(A.map.grid_rows) / static_cast<float>(A.tex_h);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, A.grid_tex);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(1.f, 1.f, 1.f, 0.85f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x0, y, z0);
    glTexCoord2f(u, 0); glVertex3f(x1, y, z0);
    glTexCoord2f(u, v); glVertex3f(x1, y, z1);
    glTexCoord2f(0, v); glVertex3f(x0, y, z1);
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
}

void draw_overlays() {
    glDisable(GL_LIGHTING);
    if (A.nav && !A.map.nav.empty()) {
        glColor3f(0.24f, 0.78f, 0.35f);
        glBegin(GL_LINES);
        for (const MapNavNode& n : A.map.nav)
            for (std::uint8_t l : n.links)
                if (l < A.map.nav.size()) { glVertex3fv(n.pos.data()); glVertex3fv(A.map.nav[l].pos.data()); }
        glEnd();
        glPointSize(5.f);
        glColor3f(0.35f, 1.f, 0.47f);
        glBegin(GL_POINTS);
        for (const MapNavNode& n : A.map.nav) glVertex3fv(n.pos.data());
        glEnd();
    }
    if (A.trig) {
        for (const MapTrigger& t : A.map.triggers) {
            if (t.kind == 2) glColor3f(0.f, 0.86f, 1.f); else if (t.kind == 1) glColor3f(1.f, 0.f, 1.f); else if (t.kind == 3) glColor3f(0.6f, 0.6f, 0.6f); else glColor3f(0.f, 0.6f, 0.78f);
            glBegin(GL_LINE_LOOP);
            for (const Vec3f& p : t.pts) glVertex3fv(p.data());
            glEnd();
        }
    }
    if (A.paths) {
        glColor3f(0.47f, 0.67f, 1.f);
        for (const MapPath& p : A.map.paths) {
            glBegin(GL_LINE_STRIP);
            for (const Vec3f& q : p.pts) glVertex3fv(q.data());
            glEnd();
        }
    }
    if (A.misc) {
        const GLfloat snd[3] = {0.75f, 0.35f, 1.f}, mk[3] = {1.f, 1.f, 1.f}, fx[3] = {1.f, 0.47f, 0.f}, ep[3] = {1.f, 1.f, 0.f};
        for (const MapSound& s : A.map.sounds) box(s.pos[0], s.pos[1], s.pos[2], 0.5f, 0.5f, snd);
        for (const MapMarker& s : A.map.markers) box(s.pos[0], s.pos[1], s.pos[2], 0.6f, 1.2f, mk);
        for (const MapEffect& s : A.map.effects) box(s.pos[0], s.pos[1], s.pos[2], 0.5f, 0.5f, fx);
        for (const MapEntry& e : A.map.entries) for (const Vec3f& p : e.pos) if (p[0] != 0 || p[2] != 0) box(p[0], p[1], p[2], 0.6f, 2.2f, ep);
    }
    if (A.water && !A.map.water.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glColor4f(0.2f, 0.35f, 1.f, 0.35f);
        glBegin(GL_QUADS);
        for (const MapWater& w : A.map.water) { glVertex3f(w.pos[0], w.pos[1], w.pos[2]); glVertex3f(w.pos[0] + 10, w.pos[1], w.pos[2]); glVertex3f(w.pos[0] + 10, w.pos[1], w.pos[2] + 10); glVertex3f(w.pos[0], w.pos[1], w.pos[2] + 10); }
        glEnd();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    if (A.ents) {
        for (std::size_t i = 0; i < A.map.entities.size(); ++i) {
            const MapEntity& e = A.map.entities[i];
            GLfloat c[3];
            kind_color(e.kind, c);
            const bool sel = static_cast<int>(i) == A.sel;
            const float hgt = e.kind == "CRT" ? 1.8f : (e.kind == "DOR" ? 2.2f : 0.6f), wid = sel ? 1.1f : (e.kind == "CRT" ? 0.7f : 0.45f);
            if (sel) { c[0] = c[1] = c[2] = 1.f; }
            box(e.pos[0], e.pos[1], e.pos[2], wid, hgt, c);
            if (sel) { glColor3f(1, 1, 1); glBegin(GL_LINES); glVertex3f(e.pos[0], e.pos[1], e.pos[2]); glVertex3f(e.pos[0], e.pos[1] + 12.f, e.pos[2]); glEnd(); }
        }
    }
}

void draw_dialog() {
    if (!A.dlg_open || !A.dlg) return;
    std::vector<std::pair<std::string, GLfloat>> rows;          // texto + tom (0 = jogador, 1 = npc, 2 = sistema)
    const std::size_t maxc = static_cast<std::size_t>(std::max(40.f, A.w * 0.56f / 8.f));
    auto clip = [&](std::string t) { if (t.size() > maxc) t = t.substr(0, maxc - 3) + "..."; return t; };
    const MapEntity* e = A.dlg_ent >= 0 && A.dlg_ent < static_cast<int>(A.map.entities.size()) ? &A.map.entities[static_cast<std::size_t>(A.dlg_ent)] : nullptr;
    rows.push_back({clip(std::string("DIALOGO com ") + (e ? e->prototype : std::string("?")) + " | script: " + A.dlg_script + "   [1-9 responde | V fecha | K outro]"), 2.f});
    const std::vector<DialogLine>& ls = A.dlg->lines();
    const std::size_t from = ls.size() > 7 ? ls.size() - 7 : 0;
    for (std::size_t i = from; i < ls.size(); ++i) rows.push_back({clip(ls[i].who + ": " + speech_text(ls[i])), ls[i].who == "Jogador" ? 0.f : 1.f});
    if (!A.dlg->error().empty()) rows.push_back({clip("ERRO da VM: " + A.dlg->error()), 2.f});
    if (A.dlg->finished()) rows.push_back({"(fim do dialogo - V fecha, K tenta outro; veja o log)", 2.f});
    else {
        for (std::size_t i = 0; i < A.dlg->choices().size() && i < 9; ++i) {
            const DialogChoice& c = A.dlg->choices()[i];
            DialogLine tmp; tmp.text_id = c.text_id; tmp.text = c.text;
            rows.push_back({clip(std::to_string(i + 1) + ". " + speech_text(tmp)), 3.f});
        }
        if (A.dlg->hidden_choices()) rows.push_back({clip(std::to_string(A.dlg->hidden_choices()) + " resposta(s) escondida(s) pela condicao do script"), 2.f});
    }
    const float x0 = A.w * 0.40f, w = A.w * 0.58f, h = 16.f * static_cast<float>(rows.size()) + 10.f, y0 = 8.f;
    glColor3f(0.04f, 0.06f, 0.10f);
    glBegin(GL_QUADS);
    glVertex3f(x0 - 6, y0, 0); glVertex3f(x0 + w, y0, 0); glVertex3f(x0 + w, y0 + h, 0); glVertex3f(x0 - 6, y0 + h, 0);
    glEnd();
    float y = y0 + h - 18.f;
    for (const auto& r : rows) {
        if (r.second == 0.f) glColor3f(0.55f, 0.85f, 1.f);
        else if (r.second == 1.f) glColor3f(1.f, 0.92f, 0.55f);
        else if (r.second == 3.f) glColor3f(0.7f, 1.f, 0.7f);
        else glColor3f(0.75f, 0.75f, 0.78f);
        text(x0, y, r.first);
        y -= 16.f;
    }
}

void draw_legend() {
    if (!A.loaded || !A.geom || A.cmode == 0 || A.map.tris.empty()) return;
    std::vector<std::pair<std::size_t, std::uint32_t>> rows;
    for (const auto& kv : counts_for(A.cmode)) rows.push_back({kv.second, kv.first});
    std::sort(rows.begin(), rows.end(), [](const std::pair<std::size_t, std::uint32_t>& a, const std::pair<std::size_t, std::uint32_t>& b) { return a.first > b.first; });
    const std::size_t total = rows.size();
    if (rows.size() > 14) rows.resize(14);
    float y = 14.f + 16.f * static_cast<float>(rows.size());
    char b[400];
    std::snprintf(b, sizeof b, "Cor por %s (M muda) | %zu valores%s%s", cmode_name(A.cmode), total, A.map.version < 2 ? " | .f3map v1: sem dados de lote, reconverta" : "", total > rows.size() ? " | mostrando os 14 maiores" : "");
    glColor3f(1.f, 1.f, 1.f);
    text(8, y, b);
    for (const auto& r : rows) {
        y -= 16.f;
        GLfloat c[3];
        id_color(r.second, A.cmode, c);
        glColor3fv(c);
        glBegin(GL_QUADS);
        glVertex3f(8, y, 0); glVertex3f(20, y, 0); glVertex3f(20, y + 11, 0); glVertex3f(8, y + 11, 0);
        glEnd();
        std::snprintf(b, sizeof b, "%s - %zu triangulos", id_label(r.second, A.cmode).c_str(), r.first);
        glColor3f(0.92f, 0.95f, 1.f);
        text(26, y, b);
    }
}

void draw_hud() {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, A.w, 0, A.h, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glColor3f(0.92f, 0.95f, 1.f);
    float y = static_cast<float>(A.h) - 18.f;
    char b[400];
    if (A.loaded) {
        std::snprintf(b, sizeof b, "%s | mapa %d/%zu | %zu triangulos | %zu entidades | %zu nos de navegacao | %zu gatilhos | %.0f FPS", A.map.name.c_str(), A.sib + 1, A.siblings.size(),
                      A.map.triangle_count(), A.map.entities.size(), A.map.nav.size(), A.map.triggers.size(), A.fps);
    } else std::snprintf(b, sizeof b, "Nenhum mapa aberto. Aperte O para escolher um .f3map (gerado por: py -3 -m f3vb convert-retail --maps).");
    text(8, y, b); y -= 16;
    text(8, y, A.status); y -= 16;
    if (A.loaded && A.sel >= 0 && A.sel < static_cast<int>(A.map.entities.size())) {
        const MapEntity& e = A.map.entities[static_cast<std::size_t>(A.sel)];
        std::snprintf(b, sizeof b, "selecionada #%d: %s  inst='%s'  nome='%s'  script='%s'  pos (%.1f, %.1f, %.1f) yaw %.2f", A.sel, e.prototype.c_str(), e.instance.c_str(), e.name.c_str(), e.script.c_str(),
                      static_cast<double>(e.pos[0]), static_cast<double>(e.pos[1]), static_cast<double>(e.pos[2]), static_cast<double>(e.yaw));
        text(8, y, b); y -= 16;
        if (!e.script.empty()) {
            const bool ok = !A.scripts.find(e.script).empty();
            std::snprintf(b, sizeof b, ok ? "  script encontrado: V abre o dialogo (OnStartDialogue) | B levanta todos os scripts do mapa" : "  script NAO encontrado em %s (rode: py -3 -m f3vb export-scripts)", A.scripts.dir().c_str());
            text(8, y, b); y -= 16;
        }
    }
    if (A.help) {
        const char* lines[] = {"O abrir mapa | PgDn/PgUp proximo/anterior | G geometria | R planta | E entidades | N navegacao | T gatilhos | P caminhos | S sons/marcadores/efeitos | A agua",
                               "X texturas (difusa / +lightmap) | V dialogo da entidade (VM) | K outro script de dialogo | B levanta scripts do mapa | W wireframe | L luz | C cores de vertice | M modo de cor (vertice/material/lightmap/textura) | Tab/Shift+Tab seleciona entidade | Z foca nela | 1-9 escolhe resposta (com dialogo aberto) | Home camera | 2 vista de cima | 1 vista 3D | F2 relatorio no log | F1 esconde ajuda",
                               "Mouse: esquerdo gira, direito/meio move, roda zoom. Log: logs\\f3mapviewer_log_*.txt"};
        for (const char* l : lines) { text(8, y, l); y -= 16; }
    }
    draw_legend();
    draw_dialog();
    glEnable(GL_DEPTH_TEST);
}

void render() {
    glViewport(0, 0, A.w, A.h);
    glClearColor(0.08f, 0.09f, 0.12f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    const float aspect = A.h > 0 ? static_cast<float>(A.w) / static_cast<float>(A.h) : 1.f;
    const float zn = std::max(0.05f, A.dist * 0.02f), zf = std::max(500.f, A.dist * 200.f);
    const float top = zn * std::tan(0.5f * 45.f * 3.14159265f / 180.f);
    glFrustum(-top * aspect, top * aspect, -top, top, zn, zf);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glTranslatef(0, 0, -A.dist);
    glRotatef(A.pitch, 1, 0, 0);
    glRotatef(A.yaw, 0, 1, 0);
    glTranslatef(-A.cx, -A.cy, -A.cz);
    const GLfloat lpos[4] = {0.4f, 0.9f, 0.6f, 0.f}, amb[4] = {0.45f, 0.45f, 0.48f, 1.f}, dif[4] = {0.85f, 0.85f, 0.8f, 1.f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    if (A.loaded) {
        draw_geometry();
        draw_floor();
        draw_overlays();
    }
    draw_hud();
    SwapBuffers(A.hdc);
}

// ------------------------------------------------------------------------------------------------ comandos
const char* cmd_name(int id) {
    switch (id) {
        case ID_OPEN: return "abrir(O)"; case ID_NEXT: return "proximo-mapa(PgDn)"; case ID_PREV: return "mapa-anterior(PgUp)"; case ID_QUIT: return "sair";
        case ID_GEOM: return "geometria(G)"; case ID_FLOOR: return "planta(R)"; case ID_ENTS: return "entidades(E)"; case ID_NAV: return "navegacao(N)";
        case ID_TRIG: return "gatilhos(T)"; case ID_PATHS: return "caminhos(P)"; case ID_MISC: return "sons-marcadores-efeitos(S)"; case ID_WATER: return "agua(A)";
        case ID_WIRE: return "wireframe(W)"; case ID_LIGHT: return "luz(L)"; case ID_VCOL: return "cores-de-vertice(C)"; case ID_SELNEXT: return "seleciona-proxima(Tab)";
        case ID_SELPREV: return "seleciona-anterior(Shift+Tab)"; case ID_FOCUS: return "foca(Z)"; case ID_HOME: return "camera(Home)"; case ID_TOP: return "vista(1/2)";
        case ID_HELP: return "ajuda(F1)"; case ID_REPORT: return "relatorio(F2)"; case ID_CMODE: return "modo-de-cor(M)"; case ID_DIALOG: return "dialogo(V)"; case ID_DIALOG_NEXT: return "proximo-dialogo(K)"; case ID_TEXMODE: return "texturas(X)"; case ID_SURVEY: return "levantar-scripts(B)"; default: return "?";
    }
}

void toggle(bool& v, const char* nome) { v = !v; A.status = std::string(nome) + (v ? " ligado" : " desligado"); }

void log_report() {
    LOGF("RELATORIO (F2): mapa='%s' arquivo=%s", A.map.name.c_str(), A.path.c_str());
    for (std::size_t i = 0; i < A.map.entities.size(); ++i) {
        const MapEntity& e = A.map.entities[i];
        A.log.linef("  entidade #%zu %s %s inst='%s' nome='%s' script='%s' pele='%s' pos (%.2f, %.2f, %.2f) yaw %.2f %s", i, e.kind.c_str(), e.prototype.c_str(), e.instance.c_str(), e.name.c_str(),
                    e.script.c_str(), e.skin.c_str(), static_cast<double>(e.pos[0]), static_cast<double>(e.pos[1]), static_cast<double>(e.pos[2]), static_cast<double>(e.yaw), A.map.on_floor(e.pos) ? "(em celula ocupada)" : "(FORA da planta)");
    }
    for (std::size_t i = 0; i < A.map.nav.size(); ++i) A.log.linef("  no de navegacao #%zu pos (%.2f, %.2f, %.2f) vizinhos %zu", i, static_cast<double>(A.map.nav[i].pos[0]), static_cast<double>(A.map.nav[i].pos[1]), static_cast<double>(A.map.nav[i].pos[2]), A.map.nav[i].links.size());
    for (std::size_t i = 0; i < A.map.blocks.size(); ++i) A.log.linef("  bloco de vertices #%zu: %u vertices", i, A.map.blocks[i].nv);
    LOGF("  modo de cor atual: %s", cmode_name(A.cmode));
    log_tables(true);
    for (const ScriptRef& r : A.map_scr) LOGF("  script '%s': %zu entidades, %s", r.name.c_str(), r.entities, r.path.empty() ? ".amx NAO ENCONTRADO" : r.path.c_str());
}

void on_command(int id) {
    switch (id) {
        case ID_OPEN: cmd_open(); break;
        case ID_NEXT: step_map(+1); break;
        case ID_PREV: step_map(-1); break;
        case ID_QUIT: LOGF("comando sair"); DestroyWindow(A.hwnd); return;
        case ID_GEOM: toggle(A.geom, "Geometria 3D"); break;
        case ID_FLOOR: toggle(A.floor_, "Planta (grade .rle)"); break;
        case ID_ENTS: toggle(A.ents, "Entidades"); break;
        case ID_NAV: toggle(A.nav, "Navegacao"); break;
        case ID_TRIG: toggle(A.trig, "Gatilhos"); break;
        case ID_PATHS: toggle(A.paths, "Caminhos"); break;
        case ID_MISC: toggle(A.misc, "Sons, marcadores, efeitos e entradas"); break;
        case ID_WATER: toggle(A.water, "Agua"); break;
        case ID_WIRE: toggle(A.wire, "Wireframe"); break;
        case ID_LIGHT: toggle(A.light, "Iluminacao"); break;
        case ID_VCOL: toggle(A.vcol, "Cores de vertice"); break;
        case ID_HELP: toggle(A.help, "Ajuda"); break;
        case ID_DIALOG:
            if (A.dlg_open) { close_dialog(); A.status = "Dialogo fechado"; } else open_dialog(false);
            break;
        case ID_DIALOG_NEXT:
            if (A.dlg_open) close_dialog();
            open_dialog(true);
            break;
        case ID_SURVEY: survey_map_scripts(); break;
        case ID_TEXMODE:
            A.tmode = (A.tmode + 1) % (A.mtex_ok ? 3 : 2);
            A.status = std::string("Texturas: ") + (A.tmode == 0 ? "desligadas (modos de cor M)" : (A.tmode == 1 ? "difusa" : "difusa + lightmap")) +
                       (A.map.blocks.empty() || A.map.blocks[0].uv.empty() ? " | AVISO: .f3map sem UVs (reconverta com convert-retail --maps)" : "") +
                       (A.tmode > 0 && A.tex_found == 0 ? " | AVISO: nenhuma textura carregada (py -3 -m f3vb export-textures)" : "");
            break;
        case ID_CMODE:
            A.cmode = (A.cmode + 1) % 4;
            A.status = std::string("Modo de cor: ") + cmode_name(A.cmode) + (A.cmode != 0 && A.map.version < 2 ? " (arquivo .f3map v1: sem dados de lote)" : "");
            break;
        case ID_SELNEXT: case ID_SELPREV:
            if (A.map.entities.empty()) { A.status = "O mapa nao tem entidades"; break; }
            {
                const int n = static_cast<int>(A.map.entities.size());
                if (A.sel < 0) A.sel = id == ID_SELNEXT ? 0 : n - 1;
                else A.sel = (A.sel + (id == ID_SELNEXT ? 1 : n - 1)) % n;
            }
            A.status = "Entidade selecionada: " + A.map.entities[static_cast<std::size_t>(A.sel)].prototype;
            break;
        case ID_FOCUS:
            if (A.sel >= 0 && A.sel < static_cast<int>(A.map.entities.size())) {
                const MapEntity& e = A.map.entities[static_cast<std::size_t>(A.sel)];
                A.cx = e.pos[0]; A.cy = e.pos[1]; A.cz = e.pos[2]; A.dist = std::min(A.dist, std::max(8.f, A.radius * 0.15f));
                A.status = "Camera na entidade selecionada";
            } else A.status = "Selecione uma entidade primeiro (Tab)";
            break;
        case ID_HOME: A.yaw = 35.f; A.pitch = 35.f; fit_camera(); A.status = "Camera resetada"; break;
        case ID_TOP: A.pitch = 89.f; A.yaw = 0.f; fit_camera(); A.status = "Vista de cima"; break;
        case ID_REPORT: A.status = "Relatorio gravado no log"; break;
        default: break;
    }
    LOGF("comando %-28s | %s", cmd_name(id), A.status.c_str());
    if (id == ID_REPORT && A.loaded) log_report();
}

HMENU make_menu() {
    HMENU bar = CreateMenu(), file = CreatePopupMenu(), view = CreatePopupMenu(), nav = CreatePopupMenu();
    AppendMenuA(file, MF_STRING, ID_OPEN, "Abrir mapa...\tO");
    AppendMenuA(file, MF_STRING, ID_NEXT, "Proximo mapa da pasta\tPgDn");
    AppendMenuA(file, MF_STRING, ID_PREV, "Mapa anterior\tPgUp");
    AppendMenuA(file, MF_STRING, ID_REPORT, "Gravar relatorio no log\tF2");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, ID_QUIT, "Sair\tEsc");
    AppendMenuA(view, MF_STRING, ID_GEOM, "Geometria 3D\tG");
    AppendMenuA(view, MF_STRING, ID_FLOOR, "Planta (grade)\tR");
    AppendMenuA(view, MF_STRING, ID_ENTS, "Entidades\tE");
    AppendMenuA(view, MF_STRING, ID_NAV, "Navegacao\tN");
    AppendMenuA(view, MF_STRING, ID_TRIG, "Gatilhos\tT");
    AppendMenuA(view, MF_STRING, ID_PATHS, "Caminhos\tP");
    AppendMenuA(view, MF_STRING, ID_MISC, "Sons, marcadores, efeitos, entradas\tS");
    AppendMenuA(view, MF_STRING, ID_WATER, "Agua\tA");
    AppendMenuA(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(view, MF_STRING, ID_WIRE, "Wireframe\tW");
    AppendMenuA(view, MF_STRING, ID_LIGHT, "Iluminacao\tL");
    AppendMenuA(view, MF_STRING, ID_VCOL, "Cores de vertice\tC");
    AppendMenuA(view, MF_STRING, ID_CMODE, "Modo de cor (vertice/material/lightmap/textura)\tM");
    AppendMenuA(view, MF_STRING, ID_HELP, "Ajuda\tF1");
    AppendMenuA(view, MF_STRING, ID_TEXMODE, "Texturas (desligada/difusa/difusa+lightmap)\tX");
    AppendMenuA(nav, MF_STRING, ID_DIALOG, "Dialogo da entidade selecionada (VM)\tV");
    AppendMenuA(nav, MF_STRING, ID_DIALOG_NEXT, "Proximo script de dialogo na entidade selecionada\tK");
    AppendMenuA(nav, MF_STRING, ID_SURVEY, "Levantar scripts do mapa (VM)\tB");
    AppendMenuA(nav, MF_STRING, ID_SELNEXT, "Selecionar proxima entidade\tTab");
    AppendMenuA(nav, MF_STRING, ID_SELPREV, "Selecionar entidade anterior\tShift+Tab");
    AppendMenuA(nav, MF_STRING, ID_FOCUS, "Focar na selecionada\tZ");
    AppendMenuA(nav, MF_STRING, ID_HOME, "Camera inicial\tHome");
    AppendMenuA(nav, MF_STRING, ID_TOP, "Vista de cima\t2");
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), "Arquivo");
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(view), "Exibir");
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(nav), "Navegar");
    return bar;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: on_command(static_cast<int>(LOWORD(wp))); return 0;
        case WM_SIZE: A.w = std::max(1, static_cast<int>(LOWORD(lp))); A.h = std::max(1, static_cast<int>(HIWORD(lp))); return 0;
        case WM_LBUTTONDOWN: A.ldown = true; A.mx = static_cast<short>(LOWORD(lp)); A.my = static_cast<short>(HIWORD(lp)); SetCapture(hwnd); return 0;
        case WM_LBUTTONUP: A.ldown = false; ReleaseCapture(); return 0;
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: A.rdown = true; A.mx = static_cast<short>(LOWORD(lp)); A.my = static_cast<short>(HIWORD(lp)); SetCapture(hwnd); return 0;
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: A.rdown = false; ReleaseCapture(); return 0;
        case WM_MOUSEMOVE: {
            const int x = static_cast<short>(LOWORD(lp)), y = static_cast<short>(HIWORD(lp));
            const int dx = x - A.mx, dy = y - A.my;
            A.mx = x; A.my = y;
            if (A.ldown) { A.yaw += dx * 0.4f; A.pitch = std::max(-89.f, std::min(89.f, A.pitch + dy * 0.4f)); }
            if (A.rdown) {
                const float k = A.dist * 0.0018f, ya = A.yaw * 3.14159265f / 180.f;
                A.cx -= (std::cos(ya) * dx) * k; A.cz -= (std::sin(ya) * dx) * k;
                A.cy += dy * k;
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            const float d = static_cast<float>(static_cast<short>(HIWORD(wp))) / 120.f;
            A.dist = std::max(A.radius * 0.02f, std::min(A.radius * 60.f, A.dist * std::pow(0.88f, d)));
            return 0;
        }
        case WM_KEYDOWN: {
            const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (A.dlg_open && ((wp >= '1' && wp <= '9') || (wp >= VK_NUMPAD1 && wp <= VK_NUMPAD9))) {      // com dialogo aberto, 1-9 escolhem a resposta
                dialog_choose(static_cast<std::size_t>(wp >= VK_NUMPAD1 ? wp - VK_NUMPAD1 : wp - '1'));
                return 0;
            }
            switch (wp) {
                case VK_ESCAPE: DestroyWindow(hwnd); break;
                case 'O': on_command(ID_OPEN); break;
                case VK_NEXT: on_command(ID_NEXT); break;
                case VK_PRIOR: on_command(ID_PREV); break;
                case 'G': on_command(ID_GEOM); break;
                case 'R': on_command(ID_FLOOR); break;
                case 'E': on_command(ID_ENTS); break;
                case 'N': on_command(ID_NAV); break;
                case 'T': on_command(ID_TRIG); break;
                case 'P': on_command(ID_PATHS); break;
                case 'S': on_command(ID_MISC); break;
                case 'A': on_command(ID_WATER); break;
                case 'W': on_command(ID_WIRE); break;
                case 'L': on_command(ID_LIGHT); break;
                case 'C': on_command(ID_VCOL); break;
                case 'M': on_command(ID_CMODE); break;
                case 'X': on_command(ID_TEXMODE); break;
                case 'V': on_command(ID_DIALOG); break;
                case 'K': on_command(ID_DIALOG_NEXT); break;
                case 'B': on_command(ID_SURVEY); break;
                case VK_TAB: on_command(shift ? ID_SELPREV : ID_SELNEXT); break;
                case 'Z': on_command(ID_FOCUS); break;
                case VK_HOME: on_command(ID_HOME); break;
                case '1': A.pitch = 35.f; A.yaw = 35.f; A.status = "Vista 3D"; LOGF("comando vista 3D"); break;
                case '2': on_command(ID_TOP); break;
                case VK_F1: on_command(ID_HELP); break;
                case VK_F2: on_command(ID_REPORT); break;
                default: break;
            }
            return 0;
        }
        case WM_DESTROY:
            LOGF("janela fechada (uptime %.1f s, %llu quadros)", A.uptime, static_cast<unsigned long long>(A.total_frames));
            PostQuitMessage(0);
            return 0;
        default: break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void open_log() {
    const std::string dir = exe_dir() + "\\logs";
    CreateDirectoryA(dir.c_str(), nullptr);
    if (!A.log.open(dir, "f3mapviewer_log") && !A.log.open(exe_dir(), "f3mapviewer_log")) {
        char tmp[MAX_PATH] = {0};
        GetTempPathA(MAX_PATH, tmp);
        A.log.open(tmp, "f3mapviewer_log");
    }
}

std::string first_arg(const char* cmdline) {
    std::string s = cmdline ? cmdline : "";
    if (s.empty()) return s;
    if (s[0] == '"') { const std::size_t e = s.find('"', 1); return s.substr(1, e == std::string::npos ? std::string::npos : e - 1); }
    const std::size_t e = s.find(' ');
    return s.substr(0, e);
}

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmdline, int show) {
    open_log();
    LOGF("%s (compilado em %s %s)", kVersion, __DATE__, __TIME__);
    LOGF("executavel: %s | linha de comando: %s", exe_dir().c_str(), cmdline && *cmdline ? cmdline : "(vazia)");
    LOGF("arquivo de log: %s", A.log.is_open() ? A.log.path().c_str() : "(NAO foi possivel criar o log)");
    LOGF("pasta padrao de mapas: %s", default_maps_dir().c_str());
    WNDCLASSA wc;
    std::memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = "F3VBMapViewerWnd";
    if (!RegisterClassA(&wc)) return 1;
    RECT r = {0, 0, A.w, A.h};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, TRUE);
    A.hwnd = CreateWindowExA(0, wc.lpszClassName, kVersion, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, make_menu(), inst, nullptr);
    if (!A.hwnd) return 1;
    A.hdc = GetDC(A.hwnd);
    PIXELFORMATDESCRIPTOR pfd;
    std::memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    const int fmt = ChoosePixelFormat(A.hdc, &pfd);
    if (!fmt || !SetPixelFormat(A.hdc, fmt, &pfd)) { MessageBoxA(nullptr, "Nao consegui configurar o OpenGL.", kVersion, MB_OK | MB_ICONERROR); return 1; }
    A.rc = wglCreateContext(A.hdc);
    if (!A.rc || !wglMakeCurrent(A.hdc, A.rc)) { MessageBoxA(nullptr, "Nao consegui criar o contexto OpenGL.", kVersion, MB_OK | MB_ICONERROR); return 1; }
    build_font();
    {
        const auto gs = [](GLenum e) { const GLubyte* p = glGetString(e); return p ? reinterpret_cast<const char*>(p) : "?"; };
        LOGF("OpenGL: fornecedor=%s | renderizador=%s | versao=%s | janela %dx%d", gs(GL_VENDOR), gs(GL_RENDERER), gs(GL_VERSION), A.w, A.h);
    }
    init_gl_extensions();
    ShowWindow(A.hwnd, show);
    UpdateWindow(A.hwnd);
    const std::string arg = first_arg(cmdline);
    if (!arg.empty()) load_map(arg);
    else {
        A.status = "Escolha um mapa na janela que vai abrir (ou aperte O).";
        render();
        cmd_open();                                  // ao abrir sem argumento, ja pergunta qual mapa
    }
    QueryPerformanceFrequency(&A.freq);
    QueryPerformanceCounter(&A.last);
    MSG msg;
    bool run = true;
    while (run) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { run = false; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!run) break;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const double dt = static_cast<double>(now.QuadPart - A.last.QuadPart) / static_cast<double>(A.freq.QuadPart);
        A.last = now;
        A.uptime += dt; ++A.total_frames; A.heartbeat += dt;
        if (A.heartbeat >= 30.0) {
            A.heartbeat = 0;
            LOGF("estado | mapa=%s triangulos=%zu entidades=%zu selecionada=%d geometria=%d planta=%d fps=%.0f", A.map.name.c_str(), A.map.triangle_count(), A.map.entities.size(), A.sel, A.geom, A.floor_, A.fps);
        }
        A.fps_acc += dt; ++A.frames;
        if (A.fps_acc >= 0.5) { A.fps = A.frames / A.fps_acc; A.fps_acc = 0; A.frames = 0; }
        render();
        Sleep(1);
    }
    LOGF("sessao encerrada normalmente");
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(A.rc);
    ReleaseDC(A.hwnd, A.hdc);
    return 0;
}
