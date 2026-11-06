// F3VB (v20): o JOGO. Reimplementacao da demo tecnica do Van Buren (Fallout 3 cancelado da Black Isle) sobre a copia do proprio jogador.
// Win32 + OpenGL, 64 bits, sem bibliotecas externas (so o Windows: opengl32, gdi32, user32, winmm, shell32, ole32).
//
// v20: le a demo ORIGINAL direto (resource.rht + data\*.grp inteiros, sem extrair nada). Na primeira vez abre o seletor de pastas do Windows;
// a pasta escolhida fica gravada em F3VB.ini AO LADO do F3VB.exe (outro .exe, outra pasta = pergunta de novo). Detalhes: docs/JOGO.md.
//
// Fluxo (o da demo original):
//   Menu principal (Mainmenu.int sobre o mapa 3D "mainmenu.map", camera do sys.ini da demo) -> New Game -> Criacao de personagem
//   (CharacterGeneration.int; valores iniciais do Default_PC.CRT) -> Loading (Loading.int) -> mapa inicial do sys.ini ([Server] Start map)
//   com a barra de acao (Actionbar.int) e a camera tatica do sys.ini ([Camera]). ESC = menu de pausa. Teclas = as do F3.ini original.
#include "win_platform.h"
#include <GL/gl.h>

#include <algorithm>
#include <random>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "actor.h"
#include "actor_gl.h"
#include "ground.h"
#include "audio.h"
#include "chargen.h"
#include "gui.h"
#include "gui_gl.h"
#include "image.h"
#include "log.h"
#include "map_build.h"
#include "map_render.h"
#include "script_world.h"
#include "stf.h"
#include "ui_assets.h"
#include "vfs.h"
#include "video_player.h"

using namespace f3v;

namespace {

constexpr const char* kVersion = "F3VB Engine v21";
constexpr float kPi = 3.14159265f;

enum class Mode { MainMenu, Options, CharGen, Game, Video };
enum OptTab { TAB_LOAD = 0, TAB_HOTKEYS, TAB_MULTI, TAB_VIDEO, TAB_GAME, TAB_COUNT };
const char* kTabPic[TAB_COUNT] = {"picLoadTab", "picHotkeysTab", "picMultiplayerTab", "picVideoTab", "picGameTab"};
const char* kTabInt[TAB_COUNT] = {"Options_Load", "Options_Hotkeys", "Options_Multiplayer", "Options_Video", "Options_Game"};

// Camera orbital do jogo original (sys.ini): forward = (sin az . cos el, -sin el, cos az . cos el); olho = alvo - forward x distancia.
// Mundo do Direct3D (mao esquerda: +X para a DIREITA olhando para +Z); no OpenGL espelhamos o X na projecao.
struct OrbitCam { float tx = 0, ty = 0, tz = 0, az = 0, el = 50, dist = 350, fov = 10; };
struct CamLimits { float dist = 350, el_min = 38.5f, el_max = 89, fov_min = 6, fov_max = 15, az_speed = 360, el_speed = 360, fov_speed = 6.5f, scroll_border = 0.01f, scroll_speed = 96; };
struct M4 { float m[16]; };
// v21: estado de jogo de cada ator (vida, caminho, alvo, combate em tempo real)
struct LiveActor {
    Actor a; ActorGL g;
    std::string name;                    // nome para mostrar (protótipo / personagem)
    bool creature = false;               // personagem ou criatura animada (pisa no chão, anda, luta); objetos ficam de fora
    int hp = 0, max_hp = 0;
    bool dead = false, hostile = false;
    std::vector<float> path;             // pontos x,z ainda por andar
    float speed = 0;                     // unidades/s do passo atual
    int target = -1;                     // índice em game_actors do alvo (-1 nenhum)
    float cooldown = 0, repath = 0, hit_at = -1;
    int dmg_min = 1, dmg_max = 3, hit_chance = 60;
    float range = 3.f;                   // alcance do ataque (unidades do mapa)
};

struct App {
    HINSTANCE inst = nullptr; HWND hwnd = nullptr; HDC hdc = nullptr; HGLRC rc = nullptr;
    int w = 1280, h = 960;
    bool fullscreen = false; RECT win_rect{}; LONG_PTR win_style = 0;
    Log log;
    std::string exe, config_path, demo_dir, assets_dir;     // UTF-8
    KeyValueFile cfg;                                          // F3VB.ini ao lado do .exe (pasta da demo + opcoes)
    Vfs vfs;
    std::map<std::string, std::string> sys;                    // sys.ini da demo: "secao.chave" (minusculo) -> valor
    UiAssets ui;
    std::unique_ptr<GuiGL> gl;
    Audio audio;
    Mode mode = Mode::MainMenu, options_back = Mode::MainMenu;
    std::map<std::string, std::unique_ptr<GuiScreen>> screens;     // nome do .int -> tela (so as que carregaram)
    int opt_tab = TAB_GAME;
    bool esc_open = false, msg_open = false, chargen_view = false, name_edit = false;
    int panel = 0;                                                   // 0 nenhum, 1 inventario, 2 pip-boy
    std::function<void(int)> msg_cb;
    // mouse
    int mx = 0, my = 0; float vx = 0, vy = 0;
    bool ldown = false, rdown = false;
    GuiScreen* press_screen = nullptr; GuiWidget* press_widget = nullptr;
    std::string slider; GuiScreen* slider_screen = nullptr;
    // mapas
    MapRenderer menu_map, game_map;
    std::vector<std::unique_ptr<LiveActor>> menu_actors, game_actors;   // v20: personagens/objetos com modelo 3D
    ActorLight menu_light, game_light;
    // v20: videos (Bink da demo, pelo ajudante F3VBVideo32.exe)
    VideoPlayer video;
    std::vector<std::string> video_queue;
    std::function<void()> after_videos;
    std::string video_helper;
    bool menu_ok = false, game_ok = false;
    OrbitCam menu_cam, cam;
    CamLimits lim;
    float menu_znear = 1, menu_zfar = 10000;
    std::string game_map_name = "00_03_Tutorial_Junktown", start_map_override;
    std::string pending_map; int loading_frames = 0;
    int hover_ent = -1;
    bool lightmaps = true, wire = false, markers = false, fog = true, debug = false;
    float aniso = 1.f; int msaa = 0;
    M4 proj{}, view{};
    // personagem
    Character pc_template, pc;
    const GuiWidget* cg_tip = nullptr;                         // ultimo widget com dica sob o mouse na criacao de personagem
    int app_off[8] = {0, 0, 7, 0, 0, 3, 1, 0};                 // rolagem inicial das listas de aparencia (reproduz o print do original)
    // dialogo
    ScriptLibrary scripts;
    Stf stf;
    std::unique_ptr<ScriptInstance> dinst;
    std::unique_ptr<DialogSession> dsess;
    bool dlg_open = false; std::string dlg_npc; int dlg_hover = -1;
    // diversos
    std::string toast; double toast_until = 0;
    std::vector<std::string> log_lines;
    int start_entry = 0;                          // sys.ini [Server] Start map entry point
    // v21: jogo de verdade
    Ground ground;                                // chão do mapa do jogo (altura, clique, caminho)
    int player = -1;                              // índice do jogador em game_actors
    bool attack_mode = false;                     // tecla A (AttackMode no F3.ini original) ou clique na arma da barra
    int hover_actor = -1;
    struct WorldText { std::string text; float pos[3]; double t0; std::uint8_t rgba[4]; };
    std::vector<WorldText> world_texts;           // números de dano, "Errou", falas
    double player_dead_at = -1;
    // v21: --autoteste (percorre o jogo sozinho e escreve logs\autoteste_<data>.txt)
    bool autotest = false; int at_step = 0; double at_t0 = 0; std::vector<std::string> at_lines; int at_fail = 0;
    std::vector<std::string> at_maps; std::size_t at_map = 0; float at_sig = 0, at_x = 0, at_z = 0; int at_target = -1;
    bool pending_cam_set = false; OrbitCam pending_cam;   // v20: camera restaurada depois de carregar um jogo salvo
    std::unique_ptr<LiveActor> cg_actor;          // v20: boneco 3D da criacao de personagem (quadro escuro a direita das estatisticas)
    std::string cg_actor_proto;          // v20: registro de mensagens da barra (canto esquerdo), como "-Welcome to Fallout" no original
    double t = 0, fps = 0, fps_acc = 0, heartbeat = 0; int frames = 0; unsigned long long total_frames = 0;
    LARGE_INTEGER freq{}, last{};
} A;

// v21: jogo (definidos na secao "jogo de verdade", perto do mouse)
void init_gameplay();
void update_gameplay(float dt);
void draw_shadows();
void draw_world_texts();
void autotest_tick();

#define LOGF(...) A.log.linef(__VA_ARGS__)

// ------------------------------------------------------------------------------------------------ utilidades
// Textos fixos do F3VB estao em UTF-8 no codigo; a fonte TrueType usa cp1252 (como os .stf). Converte o que for latino.
std::string cp(const std::string& u8) {
    std::string o;
    for (std::size_t i = 0; i < u8.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(u8[i]);
        if (c < 0x80) o.push_back(static_cast<char>(c));
        else if ((c & 0xE0) == 0xC0 && i + 1 < u8.size()) { const unsigned v = ((c & 0x1F) << 6) | (static_cast<unsigned char>(u8[i + 1]) & 0x3F); o.push_back(v < 256 ? static_cast<char>(v) : '?'); ++i; }
        else { o.push_back('?'); while (i + 1 < u8.size() && (static_cast<unsigned char>(u8[i + 1]) & 0xC0) == 0x80) ++i; }
    }
    return o;
}

std::string stf1(std::uint32_t id) {                     // texto 1-based do .stf, sem codigos de cor
    std::int32_t col;
    return strip_color_codes(A.stf.text_cp1252(id), col);
}

void say(const std::string& u8, double secs = 3.0) { A.toast = cp(u8); A.toast_until = A.t + secs; LOGF("aviso: %s", u8.c_str()); }
int box(const std::string& u8, UINT flags) { return MessageBoxW(A.hwnd, widen(u8).c_str(), L"F3VB Engine", flags); }

GuiScreen* scr(const std::string& name) { const auto it = A.screens.find(lower_ascii(name)); return it == A.screens.end() ? nullptr : it->second.get(); }

float setting(const std::string& k, float def) { return A.cfg.getf(k, def); }
void set_setting(const std::string& k, float v) { char b[32]; std::snprintf(b, sizeof b, "%.3f", v); A.cfg.values[k] = b; }
void save_settings() {
    if (!A.cfg.save(A.config_path, "; F3VB Engine v21 - configuracao (ao lado do F3VB.exe). demo = pasta da demo original escolhida na primeira vez.\r\n"
                                   "; Apague este arquivo (ou rode F3VB.exe --escolher-pasta) para escolher outra pasta.\r\n"))
        LOGF("AVISO: nao consegui gravar %s", A.config_path.c_str());
}
void apply_volumes() {
    A.audio.set_volumes(setting("MasterVolume", 1.f), setting("MusicVolume", 0.6f), setting("SfxVolume", 1.f), setting("AmbientVolume", 0.7f));
}

// ------------------------------------------------------------------------------------------------ sys.ini da demo
void load_sys_ini() {
    std::vector<std::uint8_t> d;
    if (!A.vfs.read("sys", RT_INI, d)) { LOGF("AVISO: sys.ini nao achado na demo: uso os valores medidos na demo como padrao"); return; }
    std::string sec, text(d.begin(), d.end());
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[') { sec = lower_ascii(line.substr(1, line.find(']') - 1)); continue; }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        while (!k.empty() && k.back() == ' ') k.pop_back();
        while (!v.empty() && v.front() == ' ') v.erase(v.begin());
        A.sys[sec + "." + lower_ascii(k)] = v;
    }
    LOGF("sys.ini da demo: %zu valores", A.sys.size());
}
float sysf(const char* key, float def) { const auto it = A.sys.find(key); return it == A.sys.end() ? def : static_cast<float>(std::atof(it->second.c_str())); }
std::string syss(const char* key, const std::string& def) { const auto it = A.sys.find(key); return it == A.sys.end() ? def : it->second; }

// ------------------------------------------------------------------------------------------------ telas
bool load_screen(const char* name) {
    auto s = std::make_unique<GuiScreen>();
    std::vector<std::uint8_t> d;
    std::string err;
    const ResEntry* e = A.vfs.find(name, RT_INT);
    if (!e || !A.vfs.read(*e, d, err)) { LOGF("tela %s: NAO existe na demo", name); return false; }
    if (!s->load(d.data(), d.size(), err)) { LOGF("tela %s: NAO decodificou (%s)", name, err.c_str()); return false; }
    LOGF("tela %s: %zu objetos (GUI V1.%d, %s)", name, s->file().count(), s->file().version % 10, A.vfs.where(*e).c_str());
    A.screens[lower_ascii(name)] = std::move(s);
    return true;
}

void build_and_draw(GuiScreen* s) {
    if (!s) return;
    std::vector<GuiDraw> d;
    s->build(A.ui.resources(), d);
    A.gl->draw(d);
}

// Registro de mensagens no quadro escuro a esquerda da barra (tela virtual: x 18..286, y 618..750 com a barra em 604).
// Medido no print do original: texto claro, Arial pequena, ultima linha embaixo, encostada a esquerda.
void draw_message_log() {
    GuiScreen* ab = scr("Actionbar");
    if (!ab) return;
    if (A.log_lines.empty()) return;
    const float px = font_px(9), line = px + 3.f;
    const std::uint8_t col[4] = {200, 200, 190, 255};
    float y = ab->offset_y + 146.f - line;                   // ultima linha
    const int max_lines = 8;
    int n = 0;
    for (auto it = A.log_lines.rbegin(); it != A.log_lines.rend() && n < max_lines; ++it, ++n, y -= line) A.gl->text("Arial", px, 20.f, y, *it, col, 0);
}

void center_screen(GuiScreen* s) {
    if (!s || s->file().roots.empty()) return;
    const IRect& r = s->file().roots[0]->rect;
    s->offset_x = std::floor((1024.f - static_cast<float>(r.w())) * 0.5f) - static_cast<float>(r.x0);
    s->offset_y = std::floor((768.f - static_cast<float>(r.h())) * 0.5f) - static_cast<float>(r.y0);
}

// ------------------------------------------------------------------------------------------------ MessageBox (MessageBox.int)
// botoes: 1 = so "DONE" (btn2); 2 = "YES" (btn1) e "NO" (btn3). O callback recebe 1 (YES), 2 (DONE) ou 3 (NO).
void message(const std::string& cp1252_text, int buttons, std::function<void(int)> cb = nullptr) {
    GuiScreen* m = scr("MessageBox");
    if (!m) { A.toast = cp1252_text; A.toast_until = A.t + 4; if (cb) cb(buttons == 1 ? 2 : 3); return; }
    m->set_text("lblMessage", cp1252_text);
    m->set_hidden("winButton1", buttons != 2);
    m->set_hidden("winButton3", buttons != 2);
    m->set_hidden("winButton2", buttons != 1);
    center_screen(m);
    A.msg_open = true;
    A.msg_cb = std::move(cb);
    A.audio.sfx("f3_messagebox_open");
    {   // v21: o log e todo UTF-8 (antes as mensagens saiam em cp1252 e os acentos viravam "n\xef\xbf\xbdo" no log)
        const int n = MultiByteToWideChar(1252, 0, cp1252_text.data(), static_cast<int>(cp1252_text.size()), nullptr, 0);
        std::wstring wtxt(static_cast<std::size_t>(std::max(0, n)), L'\0');
        if (n > 0) MultiByteToWideChar(1252, 0, cp1252_text.data(), static_cast<int>(cp1252_text.size()), wtxt.data(), n);
        LOGF("mensagem: %s", narrow(wtxt).c_str());
    }
}
void close_message(int which) {
    A.msg_open = false;
    A.audio.sfx("f3_messagebox_close");
    auto cb = std::move(A.msg_cb);
    A.msg_cb = nullptr;
    if (cb) cb(which);
}

// ------------------------------------------------------------------------------------------------ camera e matrizes
M4 perspective(float fovy_deg, float aspect, float n, float f) {
    M4 r{};
    const float t = 1.f / std::tan(fovy_deg * kPi / 360.f);
    r.m[0] = -t / aspect;                  // X espelhado: o mundo da demo e de mao esquerda (Direct3D)
    r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
M4 look_at(const float e[3], const float c[3]) {
    float F[3] = {c[0] - e[0], c[1] - e[1], c[2] - e[2]};
    float l = std::sqrt(F[0] * F[0] + F[1] * F[1] + F[2] * F[2]); if (l < 1e-6f) l = 1;
    for (float& v : F) v /= l;
    float S[3] = {F[1] * 0 - F[2] * 1, F[2] * 0 - F[0] * 0, F[0] * 1 - F[1] * 0};          // F x up(0,1,0)
    l = std::sqrt(S[0] * S[0] + S[1] * S[1] + S[2] * S[2]); if (l < 1e-6f) { S[0] = 1; S[1] = S[2] = 0; l = 1; }
    for (float& v : S) v /= l;
    const float U[3] = {S[1] * F[2] - S[2] * F[1], S[2] * F[0] - S[0] * F[2], S[0] * F[1] - S[1] * F[0]};
    M4 r{};
    r.m[0] = S[0]; r.m[4] = S[1]; r.m[8] = S[2];
    r.m[1] = U[0]; r.m[5] = U[1]; r.m[9] = U[2];
    r.m[2] = -F[0]; r.m[6] = -F[1]; r.m[10] = -F[2];
    r.m[12] = -(S[0] * e[0] + S[1] * e[1] + S[2] * e[2]);
    r.m[13] = -(U[0] * e[0] + U[1] * e[1] + U[2] * e[2]);
    r.m[14] = (F[0] * e[0] + F[1] * e[1] + F[2] * e[2]);
    r.m[15] = 1;
    return r;
}
bool project(const float p[3], float& sx, float& sy) {
    float v[4], c[4];
    for (int r = 0; r < 4; ++r) v[r] = A.view.m[r] * p[0] + A.view.m[4 + r] * p[1] + A.view.m[8 + r] * p[2] + A.view.m[12 + r];
    for (int r = 0; r < 4; ++r) c[r] = A.proj.m[r] * v[0] + A.proj.m[4 + r] * v[1] + A.proj.m[8 + r] * v[2] + A.proj.m[12 + r] * v[3];
    if (c[3] <= 0.01f) return false;
    sx = (c[0] / c[3] * 0.5f + 0.5f) * static_cast<float>(A.w);
    sy = (1.f - (c[1] / c[3] * 0.5f + 0.5f)) * static_cast<float>(A.h);
    return true;
}

void orbit_eye(const OrbitCam& c, float eye[3], float tgt[3]) {
    const float az = c.az * kPi / 180.f, el = c.el * kPi / 180.f;
    const float f[3] = {std::sin(az) * std::cos(el), -std::sin(el), std::cos(az) * std::cos(el)};
    tgt[0] = c.tx; tgt[1] = c.ty; tgt[2] = c.tz;
    const float d = c.dist > 0.001f ? c.dist : 0.f;
    if (d > 0) { for (int k = 0; k < 3; ++k) eye[k] = tgt[k] - f[k] * d; }
    else { for (int k = 0; k < 3; ++k) { eye[k] = tgt[k]; tgt[k] = eye[k] + f[k] * 10.f; } }     // distancia 0 (menu): a camera FICA no alvo e olha na direcao
}

void set_3d(const OrbitCam& c, float znear, float zfar, const MapData* env_map) {
    glViewport(0, 0, A.w, A.h);
    std::uint8_t bg[3] = {0, 0, 0};
    const bool use_fog = A.fog && env_map && env_map->has_env && env_map->env_f[1] > env_map->env_f[0] && env_map->env_f[0] > 0;
    if (env_map && env_map->has_env) for (int k = 0; k < 3; ++k) bg[k] = env_map->env_a[k];   // cor A do cabecalho do mapa = fundo (hipotese)
    glClearColor(bg[0] / 255.f, bg[1] / 255.f, bg[2] / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    if (A.msaa) glEnable(0x809D /* GL_MULTISAMPLE */);
    float eye[3], tgt[3];
    orbit_eye(c, eye, tgt);
    // FOV vertical do jogo, largura cresce com a tela (widescreen "Hor+"): nada some dos lados em 16:9
    const float aspect = A.h > 0 ? static_cast<float>(A.w) / static_cast<float>(A.h) : 1.f;
    float vfov = c.fov;
    if (&c == &A.menu_cam && aspect > 4.f / 3.f) {
        // v21: o menu original e 4:3; em tela larga, mostrar mais dos lados revela o "limbo" sem mapa (print do autor). Mantem a abertura
        // HORIZONTAL do 4:3 e corta em cima/embaixo ("Vert-"): o enquadramento dos personagens e do vagao fica o do original.
        const float hx = std::tan(c.fov * 0.5f * kPi / 180.f) * (4.f / 3.f);
        vfov = 2.f * std::atan(hx / aspect) * 180.f / kPi;
    }
    A.proj = perspective(vfov, aspect, znear, zfar);
    A.view = look_at(eye, tgt);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(A.proj.m);
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(A.view.m);
    if (use_fog) {
        const GLfloat fc[4] = {bg[0] / 255.f, bg[1] / 255.f, bg[2] / 255.f, 1.f};
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fc);
        // a neblina do mapa conta a partir do alvo da camera (a camera tatica fica 350 unidades longe)
        glFogf(GL_FOG_START, c.dist + env_map->env_f[0]);
        glFogf(GL_FOG_END, c.dist + env_map->env_f[1]);
    } else glDisable(GL_FOG);
}

void clamp_camera() {
    const MapData& m = A.game_map.map();
    float x0, x1, z0, z1;
    const bool default_cam = m.camera[0] == 0 && m.camera[1] == 1000 && m.camera[2] == 1000 && m.camera[3] == 0;
    if (m.has_camera && !default_cam) { x0 = m.camera[0]; z1 = m.camera[1]; x1 = m.camera[2]; z0 = m.camera[3]; }   // ECAM (xmin, zmax, xmax, zmin): hipotese
    else { float mn[3], mx[3]; if (!A.game_map.bounds(mn, mx)) return; x0 = mn[0]; x1 = mx[0]; z0 = mn[2]; z1 = mx[2]; }
    if (x0 > x1) std::swap(x0, x1);
    if (z0 > z1) std::swap(z0, z1);
    A.cam.tx = std::clamp(A.cam.tx, x0, x1);
    A.cam.tz = std::clamp(A.cam.tz, z0, z1);
    // v21: alem do alvo, a AREA VISIVEL tambem fica dentro do mapa. Os 4 cantos da tela sao projetados no chao (plano do alvo) e o alvo
    // e empurrado para dentro ate o "pegada" caber na caixa da geometria. Antes, perto da borda aparecia o fundo sem mapa ("limbo").
    if (!A.ground.empty() && A.w > 0 && A.h > 0) {
        float gx0, gz0, gx1, gz1;
        A.ground.bounds(gx0, gz0, gx1, gz1);
        for (int it = 0; it < 3; ++it) {
            float eye[3], tgt[3];
            orbit_eye(A.cam, eye, tgt);
            float f[3] = {tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2]};
            const float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
            if (fl < 1e-4f) break;
            for (float& v : f) v /= fl;
            float r[3] = {f[2], 0.f, -f[0]};
            const float rl = std::sqrt(r[0] * r[0] + r[2] * r[2]);
            if (rl < 1e-4f) break;
            r[0] /= rl; r[2] /= rl;
            const float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
            const float th = std::tan(A.cam.fov * 0.5f * kPi / 180.f), asp = static_cast<float>(A.w) / static_cast<float>(A.h);
            float mnx = 1e30f, mxx = -1e30f, mnz = 1e30f, mxz = -1e30f;
            bool ok = true;
            for (int c = 0; c < 4; ++c) {
                const float sx = (c & 1) ? 1.f : -1.f, sy = (c & 2) ? 1.f : -1.f;
                float d[3];
                for (int k = 0; k < 3; ++k) d[k] = f[k] + u[k] * th * sy + r[k] * th * asp * sx;
                if (d[1] > -1e-3f) { ok = false; break; }                 // canto acima do horizonte: nao da para limitar
                const float t = (A.cam.ty - eye[1]) / d[1];
                const float px = eye[0] + d[0] * t, pz = eye[2] + d[2] * t;
                mnx = std::min(mnx, px); mxx = std::max(mxx, px); mnz = std::min(mnz, pz); mxz = std::max(mxz, pz);
            }
            if (!ok) break;
            float dx = 0, dz = 0;
            if (mxx - mnx >= gx1 - gx0) dx = 0.5f * (gx0 + gx1) - 0.5f * (mnx + mxx);
            else if (mnx < gx0) dx = gx0 - mnx; else if (mxx > gx1) dx = gx1 - mxx;
            if (mxz - mnz >= gz1 - gz0) dz = 0.5f * (gz0 + gz1) - 0.5f * (mnz + mxz);
            else if (mnz < gz0) dz = gz0 - mnz; else if (mxz > gz1) dz = gz1 - mxz;
            if (std::fabs(dx) < 1e-3f && std::fabs(dz) < 1e-3f) break;
            A.cam.tx += dx; A.cam.tz += dz;
        }
    }
    A.cam.el = std::clamp(A.cam.el, A.lim.el_min, A.lim.el_max);
    A.cam.fov = std::clamp(A.cam.fov, A.lim.fov_min, A.lim.fov_max);
    while (A.cam.az < 0) A.cam.az += 360.f;
    while (A.cam.az >= 360.f) A.cam.az -= 360.f;
}

// ------------------------------------------------------------------------------------------------ menu principal
void setup_menu_camera() {
    // sys.ini [Mainmenu]: target (0, 5.5, 0), azimuth 0, elevation 0, distance 0, fov 68, znear 1, zfar 10000 (valores da demo)
    A.menu_cam.tx = sysf("mainmenu.target x", 0.f);
    A.menu_cam.ty = sysf("mainmenu.target y", 5.5f);
    A.menu_cam.tz = sysf("mainmenu.target z", 0.f);
    A.menu_cam.az = sysf("mainmenu.azimuth", 0.f);
    A.menu_cam.el = sysf("mainmenu.elevation", 0.f);
    A.menu_cam.dist = sysf("mainmenu.distance", 0.f);
    A.menu_cam.fov = sysf("mainmenu.fov", 68.f);
    A.menu_znear = std::max(0.05f, sysf("mainmenu.znear", 1.f));
    A.menu_zfar = std::max(A.menu_znear + 1.f, sysf("mainmenu.zfar", 10000.f));
    LOGF("camera do menu (sys.ini): alvo (%.1f, %.1f, %.1f) azimute %.1f elevacao %.1f distancia %.1f fov %.1f", A.menu_cam.tx, A.menu_cam.ty, A.menu_cam.tz,
         A.menu_cam.az, A.menu_cam.el, A.menu_cam.dist, A.menu_cam.fov);
}

void draw_actors(std::vector<std::unique_ptr<LiveActor>>& v);

void render_menu_scene() {
    if (!A.menu_ok) {
        glViewport(0, 0, A.w, A.h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        A.gl->begin(A.w, A.h);
        A.gl->image("f3_front_end.tga", 0, 0, 1024, 768);              // fundo 2D da demo, se o mapa 3D nao abrir
        A.gl->end();
        return;
    }
    set_3d(A.menu_cam, A.menu_znear, A.menu_zfar, &A.menu_map.map());
    A.menu_map.sky_time = static_cast<float>(A.t);
    A.menu_map.sky_u_per_s = setting("SkyScrollU", 0.0054f);     // nuvens: ~34 px em 7,5 s a 800x600 no original (docs/JOGO.md)
    A.menu_map.sky_v_per_s = setting("SkyScrollV", 0.f);
    A.menu_map.draw(A.lightmaps, false);
    draw_actors(A.menu_actors);
    glDisable(GL_FOG);
}

void enter_mainmenu() {
    A.mode = Mode::MainMenu;
    A.esc_open = false; A.panel = 0; A.dlg_open = false;
    // musica: a peca MA1 da demo (Sounds\PCE\MA1_PS2.pce: start MA1_A1, MA1_A2, loop MA1_A3). Que ela toca no menu e hipotese.
    A.audio.stop_ambient();
    A.audio.music({"MA1_A1", "MA1_A2"}, "MA1_A3");
    LOGF("== menu principal ==");
    // v21: o vagao "FALLOUT 3" CHEGA da direita para a esquerda (clipe Walk de 9,33 s do IT_MainMenu_boxcar, chamado pelo script
    // 00_MainMenu_Train com A_PlayAnim) e para; depois fica parado. Medido: centro vai de (30.8, 8.0) a (6.4, 23.3) no mapa.
    for (auto& L : A.menu_actors) {
        std::string sc = L->a.script;
        for (char& c : sc) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (sc.find("train") != std::string::npos && L->a.clip_index[AN_WALK] >= 0) {
            L->a.play(AN_WALK, static_cast<float>(A.t), false);
            LOGF("menu: %s chegando (animacao %s, %.2f s, script %s)", L->a.model.c_str(), L->a.clip_names[AN_WALK].c_str(), L->a.anim_length(AN_WALK), L->a.script.c_str());
        }
    }
}

// ------------------------------------------------------------------------------------------------ videos (v20)
// Os .mve da demo sao Bink 1; quem decodifica e a binkw32.dll da PROPRIA demo, num ajudante de 32 bits (video_helper32.cpp).
void video_next() {
    while (!A.video_queue.empty()) {
        const std::string mv = A.video_queue.front();
        A.video_queue.erase(A.video_queue.begin());
        const std::string path = path_join(path_join(A.demo_dir, "Movies"), mv);
        if (!file_exists(path)) { LOGF("video %s: nao existe em %s", mv.c_str(), path.c_str()); continue; }
        std::string err;
        if (A.video.start(A.video_helper, A.demo_dir, path, A.audio.muted(), err, A.hwnd, path_join(path_join(A.exe, "logs"), "video32_log.txt"),
                          setting("VideoWaveOut", 0.f) > 0.5f)) {
            LOGF("video %s: %dx%d, %u quadros (Bink da demo, pelo F3VBVideo32.exe)", mv.c_str(), A.video.width(), A.video.height(), A.video.frames());
            return;
        }
        LOGF("video %s: %s", mv.c_str(), err.c_str());
    }
    A.video.stop();
    A.video.release_gl();
    auto cb = std::move(A.after_videos);
    A.after_videos = nullptr;
    if (cb) cb();
}

void play_videos(std::vector<std::string> list, std::function<void()> after) {
    if (A.video_helper.empty() || A.demo_dir.empty()) {
        LOGF("videos: sem o ajudante F3VBVideo32.exe ao lado do F3VB.exe (ou sem a pasta da demo): pulados");
        if (after) after();
        return;
    }
    A.audio.stop_music();
    A.audio.stop_ambient();
    A.video_queue = std::move(list);
    A.after_videos = std::move(after);
    A.mode = Mode::Video;
    video_next();
}

void video_skip() {
    LOGF("video pulado");
    A.video.stop();
    video_next();
}

// ------------------------------------------------------------------------------------------------ opcoes
void open_options(int tab, Mode back) {
    if (!scr("Options")) { say("Tela de opções (Options.int) não carregou."); return; }
    A.options_back = back;
    A.opt_tab = std::clamp(tab, 0, TAB_COUNT - 1);
    A.mode = Mode::Options;
    A.esc_open = false;
    LOGF("== opcoes (aba %s) ==", kTabInt[A.opt_tab]);
}
void close_options() { A.mode = A.options_back; LOGF("opcoes fechadas"); }

// sliders: pares "btnXRegion" (trilho) + "btnX" (cursor) nas paginas de opcoes
bool slider_names(GuiScreen* s, const std::string& name, std::string& base, GuiWidget*& region, GuiWidget*& thumb) {
    base = name;
    if (base.size() > 6 && lower_ascii(base.substr(base.size() - 6)) == "region") base = base.substr(0, base.size() - 6);
    region = s->find(base + "Region");
    thumb = s->find(base);
    return region && thumb && region != thumb;
}
void slider_set(GuiScreen* s, const std::string& base, float value) {
    GuiWidget* region = s->find(base + "Region");
    GuiWidget* thumb = s->find(base);
    if (!region || !thumb) return;
    value = std::clamp(value, 0.f, 1.f);
    const float range = static_cast<float>(region->rect.w() - thumb->rect.w());
    const float x = static_cast<float>(region->rect.x0) + value * std::max(0.f, range);
    s->set_offset(base, x - static_cast<float>(thumb->rect.x0), 0);
}
void slider_drag(float vx) {
    GuiScreen* s = A.slider_screen;
    if (!s) return;
    GuiWidget* region = s->find(A.slider + "Region");
    GuiWidget* thumb = s->find(A.slider);
    if (!region || !thumb) return;
    const float range = std::max(1.f, static_cast<float>(region->rect.w() - thumb->rect.w()));
    const float v = std::clamp((vx - s->offset_x - static_cast<float>(region->rect.x0) - thumb->rect.w() * 0.5f) / range, 0.f, 1.f);
    slider_set(s, A.slider, v);
    const std::string key = A.slider.size() > 3 ? A.slider.substr(3) : A.slider;      // "btnMusicVolume" -> "MusicVolume"
    set_setting(key, v);
    apply_volumes();
}
void apply_sliders() {
    for (int t = 0; t < TAB_COUNT; ++t) {
        GuiScreen* s = scr(kTabInt[t]);
        if (!s) continue;
        s->file().walk([&](GuiWidget& w, int) {
            if (w.kind != WidgetKind::Button) return;
            const std::string n = w.name;
            if (n.size() > 6 && lower_ascii(n.substr(n.size() - 6)) == "region") {
                const std::string base = n.substr(0, n.size() - 6);
                slider_set(s, base, setting(base.size() > 3 ? base.substr(3) : base, 0.6f));
            }
        });
    }
}

// ------------------------------------------------------------------------------------------------ criacao de personagem
// Listas de aparencia (7 colunas, 6 linhas visiveis, setas para rolar). Nomes = textos do .stf; conteudo = as pecas de modelo de homem da demo
// (Critters: HumM*_Hai*, Mus*, Bea*, Pon*...) e as peles Skin_HumM_*. A rolagem inicial reproduz o print do original. Trocar a aparencia
// de verdade depende dos modelos 3D (v21); aqui a lista so rola.
const std::vector<std::uint32_t>& appearance_list(int col) {
    static const std::vector<std::uint32_t> L[8] = {
        {},
        {197, 198, 199, 200},                                                              // Body Type: Normal, Heavy, Strong, Wiry
        {201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218},   // Skin Color: Black 1 .. Yellow 3
        {219, 220, 223, 226, 229, 230, 231, 232, 233, 235, 236, 237},                      // Hairstyle: Afro, Bald, Bowl, Crown, Greaser, Long, Military...
        {7, 1458, 1459},                                                                   // Ponytail: None, Ponytail 1, Ponytail 2
        {241, 240, 242, 243, 244, 245, 247, 1843, 248},                                    // Mustache: Full, Fu Manchu, Goatee, Guvner, Handlebar, Hermit, Mutton, Small, Stubble
        {238, 241, 240, 242, 243, 245, 248},                                               // Beard: Amish, Full, Fu Manchu, Goatee, Guvner, Hermit, Stubble
        {252, 250, 251, 253, 254, 3077}};                                                  // Hair Color: Brown, Black, Blonde, Red, White, Grey Brown
    return L[col >= 1 && col <= 7 ? col : 0];
}

void refresh_appearance(GuiScreen* s) {
    for (int col = 1; col <= 7; ++col) {
        const auto& L = appearance_list(col);
        const int maxoff = std::max(0, static_cast<int>(L.size()) - 6);
        A.app_off[col] = std::clamp(A.app_off[col], 0, maxoff);
        for (int row = 1; row <= 6; ++row) {
            const std::size_t i = static_cast<std::size_t>(A.app_off[col] + row - 1);
            const std::string id = std::to_string(col) + std::to_string(row);
            s->set_text("lblAppearance" + id, i < L.size() ? stf1(L[i]) : std::string());
            s->set_checked("btnAppearance" + id, i < L.size() && A.pc.look[static_cast<std::size_t>(col)] == static_cast<int>(i));
            s->set_hidden("btnAppearance" + id, i >= L.size());
        }
    }
}

// v20: escolha da aparencia -> pecas de modelo da demo (Critters\G3D_Model\HumMNor_HaiAfro.b3d...). O nome da peca sai do texto do .stf
// ("Afro" -> HaiAfro, "Ponytail 1" -> PonPonytail1, "Mutton" -> MusMuttonChops), a pele de Skin_HumM_<Cor><NN>_LG.tga e a cor do
// cabelo das texturas <Peca>_<cor>_LG.tga. Medido nos nomes dos arquivos da demo; a ligacao lista->arquivo e a obvia (hipotese razoavel).
ActorLook look_of(const Character& c) {
    ActorLook L;
    L.set_sex = true; L.female = c.female;
    auto pick = [&](int col) -> std::string {
        const int i = c.look[static_cast<std::size_t>(col)];
        const auto& list = appearance_list(col);
        if (i < 0 || i >= static_cast<int>(list.size())) return std::string();
        std::string t = stf1(list[static_cast<std::size_t>(i)]), o;
        for (char ch : t) if (ch != ' ') o.push_back(ch);
        return o.empty() ? std::string("?") : o;
    };
    static const char* kBody[4] = {"Nor", "Fat", "Str", "Wir"};
    if (c.look[1] >= 0 && c.look[1] < 4) L.body = kBody[c.look[1]];
    if (std::string sk = pick(2); !sk.empty() && sk.size() >= 2 && std::isdigit(static_cast<unsigned char>(sk.back()))) {
        L.skin = sk.substr(0, sk.size() - 1) + "0" + sk.substr(sk.size() - 1);          // "Tan1" -> "Tan01"
    }
    const std::string model = std::string("Hum") + (c.female ? "F" : "M") + (L.body.empty() ? "Nor" : L.body);
    auto part = [&](const std::string& slot, std::string name) -> std::string {
        if (name.empty() || name == "None" || name == "?") return std::string();
        for (const std::string& cand : {slot + name, slot + name + "Chops"})
            if (A.vfs.find(model + "_" + cand, RT_MODEL) || A.vfs.find(model.substr(0, 4) + "Nor_" + cand, RT_MODEL)) return cand;
        return slot + name;
    };
    if (c.look[3] >= 0) L.slots["Hai"] = part("Hai", pick(3));
    if (c.look[4] >= 0) L.slots["Pon"] = part("Pon", pick(4));
    if (c.look[5] >= 0) L.slots["Mus"] = part("Mus", pick(5));
    if (c.look[6] >= 0) L.slots["Bea"] = part("Bea", pick(6));
    if (c.female) { L.slots["Mus"] = ""; L.slots["Bea"] = ""; }   // a demo nao tem bigode/barba para HumF*
    static const char* kHair[6] = {"brown", "black", "blonde", "red", "white", "brownGrey"};
    if (c.look[7] >= 0 && c.look[7] < 6) L.hair_color = kHair[c.look[7]];
    return L;
}

std::string look_key(const ActorLook& L) {
    std::string k = L.body + "|" + (L.female ? "F" : "M") + "|" + L.skin + "|" + L.hair_color;
    for (const auto& kv : L.slots) k += "|" + kv.first + "=" + kv.second;
    return k;
}

// dica (titulo, descricao e imagem) do widget sob o mouse: campos da dica do .int (titulo = id do .stf, descricao = outro id, imagem)
void set_chargen_tip(GuiScreen* s, const GuiWidget* w) {
    A.cg_tip = w;
    s->set_text("lblToolTipTitle", w && w->tip_text > 0 ? stf1(static_cast<std::uint32_t>(w->tip_text)) : std::string());
    s->set_text("lblToolTip", w && w->tip_a > 0 ? stf1(static_cast<std::uint32_t>(w->tip_a)) : std::string());
    if (w && !w->tip_texture.empty()) { s->set_hidden("picToolTip", false); s->set_image("picToolTip", w->tip_texture); }
    else s->set_hidden("picToolTip", true);
}

const GuiWidget* tip_widget_at(GuiScreen* s, float x, float y) {
    const GuiWidget* best = nullptr;
    s->file().walk([&](GuiWidget& w, int) {
        if (s->is_hidden(&w) || w.tip_text <= 0) return;
        if (x >= w.rect.x0 && x < w.rect.x1 && y >= w.rect.y0 && y < w.rect.y1) best = &w;   // o mais fundo (ultimo na ordem) ganha
    });
    return best;
}

void refresh_chargen() {
    GuiScreen* s = scr("CharacterGeneration");
    if (!s) return;
    Character& c = A.pc;
    for (int k = 0; k < SPECIAL_COUNT; ++k) {
        const std::string code = special_code(k);
        const int v = c.special[static_cast<std::size_t>(k)];
        s->set_image("win" + code + "10", "f3_digit_" + std::to_string(v / 10) + ".tga");
        s->set_image("win" + code + "1", "f3_digit_" + std::to_string(v % 10) + ".tga");
        s->set_text("lbl" + code, stf1(static_cast<std::uint32_t>(Character::descriptor_text_id(v))));
        s->set_disabled("btn" + code + "+", A.chargen_view || c.points <= 0 || v >= Character::kMaxStat);
        s->set_disabled("btn" + code + "-", A.chargen_view || v <= Character::kMinStat);
    }
    s->set_image("winCharPoints10", "f3_digit_" + std::to_string(c.points / 10) + ".tga");
    s->set_image("winCharPoints1", "f3_digit_" + std::to_string(c.points % 10) + ".tga");
    const int left = Character::kTags - static_cast<int>(c.tags.size());
    s->set_image("winTagSkills10", "f3_digit_" + std::to_string(left / 10) + ".tga");
    s->set_image("winTagSkills1", "f3_digit_" + std::to_string(left % 10) + ".tga");
    for (int k = 1; k <= Character::kSkills; ++k) {           // colunas "Base" e "Total" sem "%" (como no print do original)
        s->set_text("lblSkill" + std::to_string(k) + "b", std::to_string(c.skill_base(k)));
        s->set_text("lblSkill" + std::to_string(k) + "c", std::to_string(c.skill_total(k)));
        s->set_checked("btnSkill" + std::to_string(k), c.tags.count(k) > 0);
    }
    for (int k = 1; k <= 14; ++k) s->set_text("lblDerivedStat" + std::to_string(k) + "b", c.derived(k));
    for (int a = 1; a <= 2; ++a) for (int b = 1; b <= 8; ++b) { const int id = a * 10 + b; s->set_checked("btnTrait" + std::to_string(id), c.traits.count(id) > 0); }
    s->set_text("lblName", cp(c.name) + (A.name_edit ? "_" : ""));
    s->set_text("lblSex", stf1(c.female ? 26 : 25));
    s->set_text("lblAge", stf1(24) + " " + std::to_string(c.age));
    s->set_text("lblHP2", std::to_string(c.hit_points()) + "/" + std::to_string(c.hit_points()));
    refresh_appearance(s);
    if (!A.cg_tip) set_chargen_tip(s, nullptr);
}

void new_character() {
    A.pc = A.pc_template;
    A.pc.name = A.stf.loaded() ? stf1(7) : std::string("None");    // "None" (texto 7), como o original
}

void enter_chargen(bool view) {
    if (!scr("CharacterGeneration")) { say("Tela de criação de personagem não carregou."); return; }
    A.chargen_view = view;
    A.name_edit = false;
    A.cg_tip = nullptr;
    A.mode = Mode::CharGen;
    A.esc_open = false;
    refresh_chargen();
    LOGF("== criacao de personagem%s ==", view ? " (ficha, so leitura)" : "");
}

// ------------------------------------------------------------------------------------------------ salvar / carregar (v20)
// Um jogo salvo do F3VB (formato PROPRIO, texto chave=valor; o formato .sav do original nao foi decifrado): saves\quicksave.f3vbsave ao lado
// do F3VB.exe. Guarda o personagem inteiro (nome, sexo, idade, SPECIAL, pericias, marcadas, tracos, aparencia), o mapa e a camera.
// Ainda nao ha posicao do jogador (ele nao anda) nem inventario/estado dos scripts: tudo isso entra quando existir no jogo.
void start_loading(const std::string& map_name);
std::string save_path() { return path_join(path_join(A.exe, "saves"), "quicksave.f3vbsave"); }

template <class C> std::string join_ints(const C& c) { std::string o; for (int v : c) { if (!o.empty()) o += ","; o += std::to_string(v); } return o; }
std::vector<int> split_ints(const std::string& s) {
    std::vector<int> v; std::size_t p = 0;
    while (p < s.size()) { std::size_t q = s.find(',', p); if (q == std::string::npos) q = s.size(); if (q > p) v.push_back(std::atoi(s.substr(p, q - p).c_str())); p = q + 1; }
    return v;
}

bool save_game(std::string& err) {
    if (A.mode != Mode::Game && !(A.mode == Mode::Options && A.options_back == Mode::Game)) { err = "so da para salvar durante o jogo"; return false; }
    CreateDirectoryW(widen(path_join(A.exe, "saves")).c_str(), nullptr);
    KeyValueFile f;
    const Character& c = A.pc;
    f.values["versao"] = "1";
    f.values["mapa"] = A.game_map.map().name;
    f.values["nome"] = c.name;
    f.values["sexo"] = c.female ? "F" : "M";
    f.values["idade"] = std::to_string(c.age);
    f.values["special"] = join_ints(c.special);
    f.values["pontos"] = std::to_string(c.points);
    f.values["pericias"] = join_ints(c.skill_points);
    f.values["marcadas"] = join_ints(c.tags);
    f.values["tracos"] = join_ints(c.traits);
    f.values["aparencia"] = join_ints(c.look);
    char cam[160];
    std::snprintf(cam, sizeof cam, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f", A.cam.tx, A.cam.ty, A.cam.tz, A.cam.az, A.cam.el, A.cam.fov);
    f.values["camera"] = cam;
    if (!f.save(save_path(), "; F3VB Engine v21 - jogo salvo (formato proprio do F3VB)\r\n")) { err = "nao consegui gravar " + save_path(); return false; }
    LOGF("jogo salvo em %s (mapa %s, %s)", save_path().c_str(), f.values["mapa"].c_str(), c.name.c_str());
    return true;
}

bool load_game(std::string& err) {
    KeyValueFile f;
    if (!f.load(save_path())) { err = "nenhum jogo salvo (" + save_path() + ")"; return false; }
    const std::string map = f.get("mapa");
    if (map.empty() || !A.vfs.find(map, RT_MAP)) { err = "o mapa do jogo salvo nao existe nesta demo: " + map; return false; }
    Character c = A.pc_template;
    c.name = f.get("nome", c.name);
    c.female = f.get("sexo") == "F";
    c.age = std::atoi(f.get("idade", "21").c_str());
    auto fill = [](auto& arr, const std::vector<int>& v) { for (std::size_t i = 0; i < arr.size() && i < v.size(); ++i) arr[i] = v[i]; };
    fill(c.special, split_ints(f.get("special")));
    fill(c.skill_points, split_ints(f.get("pericias")));
    fill(c.look, split_ints(f.get("aparencia")));
    c.points = std::atoi(f.get("pontos", "0").c_str());
    c.tags.clear(); for (int k : split_ints(f.get("marcadas"))) c.tags.insert(k);
    c.traits.clear(); for (int k : split_ints(f.get("tracos"))) c.traits.insert(k);
    A.pc = c;
    std::vector<float> cv;
    { const std::string cs = f.get("camera"); std::size_t p = 0; while (p < cs.size()) { std::size_t q = cs.find(',', p); if (q == std::string::npos) q = cs.size(); cv.push_back(static_cast<float>(std::atof(cs.substr(p, q - p).c_str()))); p = q + 1; } }
    A.pending_cam_set = cv.size() == 6;
    if (A.pending_cam_set) { A.pending_cam = OrbitCam{}; A.pending_cam.tx = cv[0]; A.pending_cam.ty = cv[1]; A.pending_cam.tz = cv[2]; A.pending_cam.az = cv[3]; A.pending_cam.el = cv[4]; A.pending_cam.fov = cv[5]; A.pending_cam.dist = A.lim.dist; }
    LOGF("carregando jogo salvo: %s (mapa %s)", c.name.c_str(), map.c_str());
    if (A.mode == Mode::Options) close_options();
    start_loading(map);
    return true;
}

// ------------------------------------------------------------------------------------------------ jogo
void start_loading(const std::string& map_name) {
    A.pending_map = map_name;
    A.loading_frames = 0;
    A.audio.stop_music();
    LOGF("carregando o mapa %s...", map_name.c_str());
}

// Ponto de entrada inicial: sys.ini [Server] "Start map entry point" (0-based na lista do .map). Fora da faixa: o primeiro.
const MapEntry* start_entry(const MapData& m) {
    if (m.entries.empty()) return nullptr;
    std::size_t k = static_cast<std::size_t>(std::max(0, A.start_entry));
    if (k >= m.entries.size() || m.entries[k].pos.empty()) k = 0;
    return m.entries[k].pos.empty() ? nullptr : &m.entries[k];
}

void place_camera_at_start() {
    const MapData& m = A.game_map.map();
    float mn[3], mx[3];
    A.game_map.bounds(mn, mx);
    A.cam = OrbitCam{};
    A.cam.tx = (mn[0] + mx[0]) * 0.5f; A.cam.tz = (mn[2] + mx[2]) * 0.5f; A.cam.ty = 0;
    const MapEntry* best = start_entry(m);
    if (best) { A.cam.tx = best->pos[0][0]; A.cam.ty = best->pos[0][1]; A.cam.tz = best->pos[0][2]; }
    else if (A.player >= 0 && static_cast<std::size_t>(A.player) < A.game_actors.size()) {   // v21: sem ponto de entrada: no jogador
        const Actor& p = A.game_actors[static_cast<std::size_t>(A.player)]->a;
        A.cam.tx = p.pos[0]; A.cam.ty = p.pos[1]; A.cam.tz = p.pos[2];
    }
    A.cam.dist = A.lim.dist;                                    // 350 (sys.ini: Distance Min = Max)
    // v20: a camera comeca olhando para onde o jogador esta virado (angulo do ponto de entrada). No Junktown o ponto 0 fica na borda
    // norte virado para a cidade (pi): com azimute 0 fixo (v19) a camera olhava para fora do mapa e mostrava o ceu.
    A.cam.az = best && !best->angles.empty() ? best->angles[0] * 180.f / kPi : 0.f;
    // v20: o print do original (gameplay) mostra camera bem inclinada, sem ceu: meio da faixa do sys.ini [Camera]
    // (Elevation 38,5..89 -> 63,75) e meio da faixa de FOV (6..15 -> 10,5). Nada de hipotese "50 graus" do v19.
    A.cam.el = std::clamp(0.5f * (A.lim.el_min + A.lim.el_max), A.lim.el_min, A.lim.el_max);
    A.cam.fov = std::clamp(0.5f * (A.lim.fov_min + A.lim.fov_max), A.lim.fov_min, A.lim.fov_max);
    clamp_camera();
    LOGF("camera no inicio: alvo (%.1f, %.1f, %.1f) az %.0f el %.0f fov %.0f%s", A.cam.tx, A.cam.ty, A.cam.tz, A.cam.az, A.cam.el, A.cam.fov,
         best ? " (ponto de entrada)" : " (centro do mapa)");
}

bool load_map_into(MapRenderer& r, const std::string& name, std::string& err) {
    MapData m;
    MapBuildStats st;
    const DWORD t0 = GetTickCount();
    if (!build_map(A.vfs, name, m, err, &st)) return false;
    const DWORD t1 = GetTickCount();
    LOGF("mapa %s montado da demo em %lu ms: %zu triangulos, %zu/%zu nos pela gramatica completa (%s, %s)", m.name.c_str(), static_cast<unsigned long>(t1 - t0),
         m.triangle_count(), st.nodes_full, st.nodes, st.tre_name.c_str(), st.rle_name.c_str());
    r.set_anisotropy(A.aniso);
    MapRenderer::TexLoader tex = [](const std::string& tname, Image& img) {
        std::vector<std::uint8_t> d;
        std::string e;
        const ResEntry* re = A.vfs.find(tname, RT_TGA);
        return re && A.vfs.read(*re, d, e) && decode_image(d.data(), d.size(), img, e);
    };
    if (!r.load_data(std::move(m), tex, &A.log, err)) return false;
    LOGF("texturas enviadas em %lu ms", static_cast<unsigned long>(GetTickCount() - t1));
    return true;
}

// v20: entidades do mapa com modelo 3D (personagens, criaturas, o vagao do menu...), montadas da demo (actor.cpp)
void release_actors(std::vector<std::unique_ptr<LiveActor>>& v) { for (auto& a : v) actor_gl_release(a->g); v.clear(); }

// luz dos personagens: o "sol" do mapa (a luz mais distante do centro, no .8tre) e a cor A do cabecalho como ambiente (hipotese:
// no menu ela da 115,111,98, e com o material ambiente 0,5 da demo isso vira ~0,45 de luz de fundo, como no print original)
ActorLight light_for(const MapData& m) {
    ActorLight L;
    float c[3] = {0, 0, 0};
    float mn[3], mx[3];
    if (!m.tris.empty()) {
        bool first = true;
        for (const MapEntity& e : m.entities) for (int k = 0; k < 3; ++k) { const float v = e.pos[static_cast<std::size_t>(k)]; if (first) { mn[k] = mx[k] = v; } else { mn[k] = std::min(mn[k], v); mx[k] = std::max(mx[k], v); } (void)0; first = false; }
        if (!first) for (int k = 0; k < 3; ++k) c[k] = (mn[k] + mx[k]) * 0.5f;
    }
    const MapLight* sun = nullptr;
    float best = 0;
    for (const MapLight& l : m.lights) {
        float d = 0;
        for (int k = 0; k < 3; ++k) d += (l.pos[static_cast<std::size_t>(k)] - c[k]) * (l.pos[static_cast<std::size_t>(k)] - c[k]);
        if (d > best) { best = d; sun = &l; }
    }
    if (sun && best > 100.f * 100.f) {
        float dir[3], n = 0;
        for (int k = 0; k < 3; ++k) { dir[k] = c[k] - sun->pos[static_cast<std::size_t>(k)]; n += dir[k] * dir[k]; }
        n = std::sqrt(n);
        for (int k = 0; k < 3; ++k) { L.dir[k] = dir[k] / n; L.diffuse[k] = 0.47f * sun->color[static_cast<std::size_t>(k)]; }
    }
    if (m.has_env) for (int k = 0; k < 3; ++k) L.ambient[k] = std::clamp(m.env_a[k] * 1.25f / 255.f, 0.35f, 1.0f);   // calibrado no menu: macacao 78 e vagao 121 no original
    return L;
}

void build_actors(const MapData& m, std::vector<std::unique_ptr<LiveActor>>& out) {
    release_actors(out);
    const DWORD t0 = GetTickCount();
    int fails = 0;
    for (std::size_t i = 0; i < m.entities.size(); ++i) {
        auto la = std::make_unique<LiveActor>();
        std::string err;
        std::vector<std::string> notes;
        ActorBuildOptions opt;
        opt.log = &notes;
        if (!build_actor(A.vfs, m.entities[i], la->a, err, opt)) {
            if (++fails <= 12) LOGF("  entidade %zu (%s): sem modelo: %s", i, m.entities[i].prototype.c_str(), err.c_str());
            continue;
        }
        la->a.entity = static_cast<int>(i);
        for (const std::string& n : notes) LOGF("  entidade %zu (%s): %s", i, m.entities[i].prototype.c_str(), n.c_str());
        actor_gl_upload(la->a, la->g, A.aniso);
        out.push_back(std::move(la));
    }
    LOGF("modelos 3D das entidades (%s): %zu de %zu montados da demo em %lu ms%s", m.name.c_str(), out.size(), m.entities.size(),
         static_cast<unsigned long>(GetTickCount() - t0), fails ? "" : " (todos)");
    for (const auto& a : out) LOGF("  %s", a->a.description.c_str());
}

// v20: o jogador aparece no ponto de entrada inicial, com o prototipo Default_PC.CRT (o mesmo do menu: macacao da prisao).
// Fica no fim de game_actors com entity = -1.
void spawn_player(const MapData& m) {
    const MapEntry* e = start_entry(m);
    MapEntity pe;
    pe.kind = "CRT"; pe.prototype = "Default_PC.CRT"; pe.name = "Player";
    if (e) {
        pe.pos = e->pos[0];
        pe.yaw = e->angles.empty() ? 0.f : e->angles[0];
    } else {
        // v21: mapa sem ponto de entrada (ex.: 04_0202_Spelunking, que a demo so abre por viagem): o jogador nasce no chao mais alto do meio
        float x0, z0, x1, z1, gy = 0;
        A.ground.bounds(x0, z0, x1, z1);
        pe.pos = {0.5f * (x0 + x1), 0.f, 0.5f * (z0 + z1)};
        for (float r = 0; r < 60.f && !A.ground.top(pe.pos[0], pe.pos[2], gy); r += 2.f) pe.pos[0] = 0.5f * (x0 + x1) + r;
        pe.pos[1] = gy;
        LOGF("jogador: o mapa nao tem ponto de entrada; nasce no meio do mapa (%.1f, %.1f, %.1f)", pe.pos[0], pe.pos[1], pe.pos[2]);
    }
    auto la = std::make_unique<LiveActor>();
    std::string err;
    const ActorLook look = look_of(A.pc);                   // a aparencia escolhida na criacao de personagem
    ActorBuildOptions opt;
    opt.look = &look;
    if (!build_actor(A.vfs, pe, la->a, err, opt)) { LOGF("jogador: %s", err.c_str()); return; }
    la->a.entity = -1;
    actor_gl_upload(la->a, la->g, A.aniso);
    LOGF("jogador: %s em (%.1f, %.1f, %.1f), angulo %.2f", la->a.description.c_str(), pe.pos[0], pe.pos[1], pe.pos[2], pe.yaw);
    A.game_actors.push_back(std::move(la));
}

// ------------------------------------------------------------------------------------------------ boneco da criacao de personagem (v20)
// Print do original (83004cb4, aba STATS): o personagem aparece inteiro, em pe, parado (Idle), sobre o fundo escuro do quadro a
// direita das "Derived Statistics" (800x600: x 612..783, y 30..305 -> tela virtual 783..1002 x 38..390), um pouco virado.
// Na aba THRESHOLDS o quadro mostra as resistencias e o boneco nao aparece. FOV 35 = sys.ini [CharGen] "Camera FOV".
void draw_chargen_actor() {
    GuiScreen* s = scr("CharacterGeneration");
    if (!s) return;
    if (const GuiWidget* th = s->find("winThresholds"); th && !s->is_hidden(th)) return;
    const std::string proto = "Default_PC.CRT";             // base: o prototipo do jogador; a aparencia escolhida troca corpo/partes/pele
    const ActorLook look = look_of(A.pc);
    const std::string key = proto + "#" + look_key(look);
    if (!A.cg_actor || A.cg_actor_proto != key) {
        if (A.cg_actor) actor_gl_release(A.cg_actor->g);
        A.cg_actor.reset();
        MapEntity pe;
        pe.kind = "CRT"; pe.prototype = proto;
        auto la = std::make_unique<LiveActor>();
        std::string err;
        A.cg_actor_proto = key;
        ActorBuildOptions opt;
        std::vector<std::string> notes;
        opt.look = &look; opt.log = &notes;
        const bool ok = build_actor(A.vfs, pe, la->a, err, opt);
        for (const std::string& nt : notes) LOGF("criacao de personagem: %s", nt.c_str());
        if (!ok) { LOGF("criacao de personagem: boneco 3D: %s", err.c_str()); return; }
        actor_gl_upload(la->a, la->g, A.aniso);
        LOGF("criacao de personagem: boneco 3D %s (altura %.1f)", la->a.description.c_str(), la->a.height);
        A.cg_actor = std::move(la);
    }
    Actor& a = A.cg_actor->a;
    a.yaw = setting("ChargenActorYaw", 200.f) * kPi / 180.f;
    a.animate(static_cast<float>(A.t));
    const float sc = std::min(static_cast<float>(A.w) / 1024.f, static_cast<float>(A.h) / 768.f);
    const float ox = (static_cast<float>(A.w) - 1024.f * sc) * 0.5f, oy = (static_cast<float>(A.h) - 768.f * sc) * 0.5f;
    const int vx = static_cast<int>(ox + 783.f * sc), vy = static_cast<int>(static_cast<float>(A.h) - (oy + 390.f * sc));
    const int vw = static_cast<int>(219.f * sc), vh = static_cast<int>(352.f * sc);
    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);
    glClear(GL_DEPTH_BUFFER_BIT);
    const float h = a.height > 1.f ? a.height : 18.f;
    const float fov = sysf("chargen.camera fov", 35.f);
    // o boneco ocupa ~90% da altura do quadro (print: dos pes y 300 a cabeca y 58, de 275)
    const float dist = (h * 0.5f / 0.80f) / std::tan(fov * 0.5f * kPi / 180.f);
    const float eye[3] = {a.pos[0], a.pos[1] + h * 0.55f, a.pos[2] - dist};
    const float tgt[3] = {a.pos[0], a.pos[1] + h * 0.5f, a.pos[2]};
    const M4 P = perspective(fov, static_cast<float>(vw) / static_cast<float>(std::max(1, vh)), dist * 0.1f, dist * 4.f);
    const M4 V = look_at(eye, tgt);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(P.m);
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(V.m);
    glDisable(GL_FOG);
    actor_gl_draw(a, A.cg_actor->g, A.menu_light, false);
    actor_gl_draw(a, A.cg_actor->g, A.menu_light, true);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, A.w, A.h);
}

void draw_actors(std::vector<std::unique_ptr<LiveActor>>& v) {
    const ActorLight& L = &v == &A.menu_actors ? A.menu_light : A.game_light;
    if (v.empty()) return;
    for (auto& a : v) a->a.animate(static_cast<float>(A.t));
    for (auto& a : v) actor_gl_draw(a->a, a->g, L, false);
    for (auto& a : v) actor_gl_draw(a->a, a->g, L, true);
}

void finish_loading() {
    std::string err;
    const std::string name = A.pending_map;
    A.pending_map.clear();
    release_actors(A.game_actors);
    A.game_map.release();
    A.game_ok = load_map_into(A.game_map, name, err);
    if (!A.game_ok) {
        LOGF("FALHA ao montar o mapa %s: %s", name.c_str(), err.c_str());
        enter_mainmenu();
        message(cp("Não consegui abrir o mapa " + name + ".\n" + err), 1);
        return;
    }
    build_actors(A.game_map.map(), A.game_actors);
    A.ground.build(A.game_map.map());        // v21: o chao vem antes do jogador (sem ponto de entrada, ele nasce no chao do meio do mapa)
    spawn_player(A.game_map.map());
    init_gameplay();
    A.game_light = light_for(A.game_map.map());
    place_camera_at_start();
    if (A.pending_cam_set) { A.cam = A.pending_cam; A.pending_cam_set = false; clamp_camera(); }
    A.mode = Mode::Game;
    A.esc_open = false; A.panel = 0; A.dlg_open = false;
    A.audio.ambient("AMB_WindBlowing");
    if (GuiScreen* ab = scr("Actionbar")) {
        ab->offset_y = 604.f;                                    // 604 + 164 = 768: as telas do jogo ficam acima da barra (medido nos .int)
        ab->set_text("lblName", cp(A.pc.name));
        ab->set_text("lblActionPointsValue", A.pc.derived(1));
        // v20, pelo print do jogo original (9db4e32a): so o 1o quadro do grupo aparece (nome do jogador), os outros 5 ficam escondidos.
        for (int k = 1; k <= 5; ++k) { char nm[16]; std::snprintf(nm, sizeof nm, "picPlayer%02d", k); ab->set_hidden(nm, true); }
        // Arma 1 desarmada: "PUNCH" (texto 888 do .stf) em cima, "AP3" embaixo. Arma 2 aparece apagada (desabilitada) com o mesmo texto.
        std::string punch = stf1(888);
        LOGF("barra de acoes: modo da arma 1 = \"%s\" (texto 888)", punch.c_str());
        if (punch.empty()) punch = "PUNCH";
        for (const char* w : {"01", "02"})
            for (const char* st : {"_up", "_down"}) {
                ab->set_text(std::string("lblWeapon") + w + "Mode" + st, punch);
                ab->set_text(std::string("lblWeapon") + w + "AP" + st, "AP3");
            }
        // v21: a arma 2 fica normal, SEM a camada cinza do v20 (pedido do autor; o cinza imitava o print do original)
        // Mostradores HEALTH/FATIGUE/EVASION: no print do original os tres marcam 000 (a demo nao liga esses contadores).
        // Desenhados com as imagens f3_digit_N.tga, um digito por janela (centena, dezena, unidade).
        auto digits = [&](const char* name, int v) {
            v = std::clamp(v, 0, 999);
            const int d[3] = {v / 100, (v / 10) % 10, v % 10};
            const char* suf[3] = {"100", "10", "1"};
            for (int i = 0; i < 3; ++i) ab->set_image(std::string("win") + name + "Digit" + suf[i], "f3_digit_" + std::to_string(d[i]) + ".tga");
        };
        digits("Health", A.pc.hit_points()); digits("Fatigue", 0); digits("Evasion", 0);   // v21: vida de verdade (muda no combate)
    }
    A.log_lines.clear();
    A.log_lines.push_back("-Welcome to Fallout");                // primeira linha do registro no print do original (nao esta no .stf)
    if (GuiScreen* inv = scr("Inventory")) {
        for (int k = 1; k <= 13; ++k) {                          // rotulos de pericia do inventario estao em ordem alfabetica: casa pelo id do texto
            char nm[32]; std::snprintf(nm, sizeof nm, "lblSkill%02d", k);
            if (GuiWidget* w = inv->find(nm)) {
                const int skill = static_cast<int>(w->text_id) - 82;
                if (skill >= 1 && skill <= 13) inv->set_text(std::string(nm) + "Value", std::to_string(A.pc.skill_total(skill)));
            }
        }
    }
    LOGF("== jogo: %s ==", A.game_map.map().name.c_str());
}

// ------------------------------------------------------------------------------------------------ dialogo (Dialog.int + VM)
std::string entity_text(const MapEntity& e) { return e.prototype + " " + e.instance + " " + e.name + " " + e.script; }
std::string clean_name(const MapEntity& e) {
    std::string n = e.name.empty() ? e.instance : e.name;
    std::size_t i = 0;
    while (i < n.size() && ((n[i] >= '0' && n[i] <= '9') || n[i] == ' ' || n[i] == '_')) ++i;     // "00 Corporal Armstrong" -> "Corporal Armstrong"
    return n.substr(i);
}

bool entity_has_dialogue(int i) {
    const MapData& m = A.game_map.map();
    if (i < 0 || i >= static_cast<int>(m.entities.size())) return false;
    return !A.scripts.match_dialogue(entity_text(m.entities[static_cast<std::size_t>(i)])).empty();
}

void close_dialog() { A.dlg_open = false; A.dsess.reset(); A.dinst.reset(); LOGF("dialogo fechado"); }

void update_dialog_screen() {
    GuiScreen* s = scr("Dialog");
    if (!s || !A.dsess) return;
    std::string npc_text;
    for (auto it = A.dsess->lines().rbegin(); it != A.dsess->lines().rend(); ++it) if (it->who != "Jogador") { npc_text = it->text; break; }
    s->set_text("SpeakerTextLbl", npc_text.empty() ? std::string("...") : npc_text);
    s->set_text("SpeakerNameLbl", A.dlg_npc);
    s->set_text("ListenerNameLbl", cp(A.pc.name));
    s->set_text("ListenerTextLbl", "");                              // as respostas sao desenhadas por cima, uma por linha (ver draw_dialog_replies)
}

void open_dialog(int ent) {
    const MapData& m = A.game_map.map();
    if (ent < 0 || ent >= static_cast<int>(m.entities.size())) return;
    const MapEntity& e = m.entities[static_cast<std::size_t>(ent)];
    const auto cands = A.scripts.match_dialogue(entity_text(e));
    if (cands.empty()) { say("Esta entidade não tem script de diálogo."); return; }
    std::string err;
    std::vector<std::uint8_t> bytes;
    if (!A.scripts.read(cands[0].path, bytes)) { say("Script de diálogo não abriu."); return; }
    A.dinst = ScriptInstance::from_bytes(bytes, cands[0].name, 1000 + ent, 19, err);
    if (!A.dinst) { say("Script de diálogo não carregou: " + err); LOGF("dialogo: %s", err.c_str()); return; }
    A.dsess = std::make_unique<DialogSession>(*A.dinst, &A.stf);
    if (!A.dsess->start() || A.dsess->finished()) {
        LOGF("dialogo '%s' terminou ao iniciar: %s", cands[0].name.c_str(), A.dsess->error().c_str());
        say("O diálogo terminou logo ao começar (veja o log).");
        A.dsess.reset(); A.dinst.reset();
        return;
    }
    A.dlg_npc = cp(clean_name(e));
    A.dlg_open = true;
    A.dlg_hover = -1;
    if (GuiScreen* s = scr("Dialog")) {                          // retrato: port_<nome>.tga quando o nome da entidade cita um dos retratos da demo
        s->clear_image("SpeakerPic");
        const std::string low = lower_ascii(entity_text(e));
        for (const char* p : {"armstrong", "bobby", "commie", "testocles"})
            if (low.find(p) != std::string::npos || (std::strcmp(p, "commie") == 0 && low.find("communist") != std::string::npos)) {
                s->set_image("SpeakerPic", std::string("port_") + p + ".tga"); break;
            }
    }
    update_dialog_screen();
    LOGF("dialogo com '%s' (script '%s'): %s", clean_name(e).c_str(), cands[0].name.c_str(), A.dsess->lines().empty() ? "" : A.dsess->lines().back().text.c_str());
}

void dialog_choose(std::size_t i) {
    if (!A.dsess || i >= A.dsess->choices().size()) return;
    LOGF("  [resposta %zu] %s", i + 1, A.dsess->choices()[i].text.c_str());
    A.audio.sfx("f3_button_up_01");
    A.dsess->choose(i);
    if (A.dsess->finished()) { LOGF("  dialogo terminou"); close_dialog(); return; }
    update_dialog_screen();
}

struct ReplyRow { float x0, y0, x1, y1; };
std::vector<ReplyRow> reply_rows() {
    std::vector<ReplyRow> rows;
    GuiScreen* s = scr("Dialog");
    if (!s || !A.dsess) return rows;
    GuiWidget* lw = s->find("ListenerWin");
    const float x0 = lw ? static_cast<float>(lw->rect.x0) + 8.f : 160.f, x1 = lw ? static_cast<float>(lw->rect.x1) - 8.f : 524.f;
    float y = lw ? static_cast<float>(lw->rect.y1) + 4.f : 330.f;
    for (std::size_t i = 0; i < A.dsess->choices().size(); ++i) { rows.push_back({x0, y, x1, y + 16.f}); y += 17.f; }
    return rows;
}

void draw_dialog_replies() {
    if (!A.dsess) return;
    const auto rows = reply_rows();
    if (rows.empty()) return;
    A.gl->rect(rows.front().x0 - 6, rows.front().y0 - 3, rows.front().x1 + 6, rows.back().y1 + 3, 0.f, 0.f, 0.f, 0.78f);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const DialogChoice& c = A.dsess->choices()[i];
        const bool hot = static_cast<int>(i) == A.dlg_hover;
        std::uint8_t col[4] = {120, 200, 255, 255};
        if (c.color >= 0) { col[0] = static_cast<std::uint8_t>(c.color & 0xFF); col[1] = static_cast<std::uint8_t>((c.color >> 8) & 0xFF); col[2] = static_cast<std::uint8_t>((c.color >> 16) & 0xFF); }
        if (col[0] + col[1] + col[2] < 200) { col[0] = static_cast<std::uint8_t>(std::min(255, col[0] + 90)); col[1] = static_cast<std::uint8_t>(std::min(255, col[1] + 120)); col[2] = static_cast<std::uint8_t>(std::min(255, col[2] + 60)); }
        if (hot) { col[0] = col[1] = 255; col[2] = 160; A.gl->rect(rows[i].x0 - 4, rows[i].y0, rows[i].x1 + 4, rows[i].y1, 0.25f, 0.35f, 0.2f, 0.6f); }
        A.gl->text("Gothic 821 Condensed BT", 14.f, rows[i].x0, rows[i].y0, std::to_string(i + 1) + ". " + c.text, col, 0);
    }
}

// ------------------------------------------------------------------------------------------------ cliques
void on_click(GuiScreen* s, const std::string& name) {
    const std::string n = lower_ascii(name);
    LOGF("clique: %s / %s", s ? s->file().name.c_str() : "?", name.c_str());
    if (s == scr("MessageBox")) {
        if (n == "btn1") close_message(1); else if (n == "btn2") close_message(2); else if (n == "btn3") close_message(3);
        return;
    }
    if (s == scr("Mainmenu")) {
        if (n == "newgamebtn") { new_character(); enter_chargen(false); }
        else if (n == "loadgamebtn") open_options(TAB_LOAD, Mode::MainMenu);
        else if (n == "optionsbtn") open_options(TAB_GAME, Mode::MainMenu);
        else if (n == "multibtn") message(cp("O modo multijogador não existe nesta demo técnica."), 1);
        else if (n == "cinematicsbtn") play_videos({"IPlogo.mve", "BISlogo.mve"}, [] { enter_mainmenu(); });
        else if (n == "quitbtn") PostQuitMessage(0);
        return;
    }
    if (s == scr("EscapeMenu")) {
        if (n == "continuebtn") A.esc_open = false;
        else if (n == "loadbtn") open_options(TAB_LOAD, Mode::Game);
        else if (n == "optionsbtn") open_options(TAB_GAME, Mode::Game);
        else if (n == "quitbtn") message(stf1(34), 2, [](int r) { if (r == 1) enter_mainmenu(); });      // "Do you really want to go back to the main menu?"
        return;
    }
    if (s == scr("CharacterGeneration")) {
        Character& c = A.pc;
        if (A.chargen_view && n != "btndone" && n != "btncancel" && n != "btnoptions" && n != "btnstats" && n != "btnthresholds") return;
        for (int k = 0; k < SPECIAL_COUNT; ++k) {
            const std::string code = lower_ascii(special_code(k));
            if (n == "btn" + code + "+") { c.inc(k); refresh_chargen(); return; }
            if (n == "btn" + code + "-") { c.dec(k); refresh_chargen(); return; }
        }
        if (n.rfind("btnskill", 0) == 0) {
            const int k = std::atoi(n.c_str() + 8);
            if (!c.toggle_tag(k)) message(stf1(99), 1);                                                    // "You already have the maximum of three tag skills!"
            refresh_chargen(); return;
        }
        if (n.rfind("btntrait", 0) == 0 && n.size() > 8 && n[8] >= '0' && n[8] <= '9') {
            if (!c.toggle_trait(std::atoi(n.c_str() + 8))) message(cp("Você pode escolher no máximo dois traços opcionais."), 1);
            refresh_chargen(); return;
        }
        if (n.rfind("btnappearance", 0) == 0 && n.size() == 15 && std::isdigit(static_cast<unsigned char>(n[13])) && std::isdigit(static_cast<unsigned char>(n[14]))) {
            const int col = n[13] - '0', row = n[14] - '0';
            if (col >= 1 && col <= 7 && row >= 1 && row <= 6) {
                const int i = A.app_off[col] + row - 1;
                if (i < static_cast<int>(appearance_list(col).size())) c.look[static_cast<std::size_t>(col)] = i;
                refresh_chargen();
            }
            return;
        }
        if (n.rfind("btnappearanceup", 0) == 0 || n.rfind("btnappearancedown", 0) == 0) {
            const bool up = n[13] == 'u';
            const int col = std::atoi(n.c_str() + (up ? 15 : 17));
            if (col >= 1 && col <= 7) { A.app_off[col] += up ? -1 : 1; refresh_chargen(); }
            return;
        }
        if (n == "btnsex") { c.female = !c.female; refresh_chargen(); return; }
        if (n == "btnage") { c.age = c.age >= 45 ? 16 : c.age + 1; refresh_chargen(); return; }
        if (n == "btnname") { A.name_edit = !A.name_edit; refresh_chargen(); return; }
        if (n == "btnstats" || n == "btnthresholds") {
            const bool th = n == "btnthresholds";
            s->set_hidden("winStats", th); s->set_hidden("winThresholds", !th);
            return;
        }
        if (n == "btnoptions") { open_options(TAB_GAME, Mode::CharGen); return; }
        if (n == "btncancel") { if (A.chargen_view) A.mode = Mode::Game; else enter_mainmenu(); return; }
        if (n == "btndone") {
            if (A.chargen_view) { A.mode = Mode::Game; return; }
            if (!c.ready()) { message(stf1(30), 1); return; }                                              // "You must spend all character points and tag skills..."
            LOGF("personagem: %s, %s, %d anos | ST %d PE %d EN %d CH %d IN %d AG %d LK %d | pericias marcadas %zu, tracos %zu", c.name.c_str(), c.female ? "mulher" : "homem", c.age,
                 c.special[0], c.special[1], c.special[2], c.special[3], c.special[4], c.special[5], c.special[6], c.tags.size(), c.traits.size());
            start_loading(A.game_map_name);
            return;
        }
        return;
    }
    if (s == scr("Actionbar")) {
        if (n == "btninventory") A.panel = A.panel == 1 ? 0 : 1;
        else if (n == "btnpipboy") A.panel = A.panel == 2 ? 0 : 2;
        else if (n == "btnoptions") open_options(TAB_GAME, Mode::Game);
        else if (n == "btncharacter") enter_chargen(true);
        else if (n == "btnsneak") say("Furtividade: ainda não implementada.");
        else if (n == "btnuseskill") say("Usar perícia: ainda não implementado.");
        else if (n.rfind("btnweapon", 0) == 0) { A.attack_mode = !A.attack_mode; say(A.attack_mode ? "Modo de ataque: clique num alvo" : "Modo de ataque desligado", 1.5); }
        return;
    }
    for (int t = 0; t < TAB_COUNT; ++t) {
        if (s != scr(kTabInt[t])) continue;
        if (n == "btndone" || n == "btncancel") { save_settings(); close_options(); return; }
        if (n == "btndefault") {
            for (const char* k : {"MasterVolume", "MusicVolume", "SfxVolume", "AmbientVolume"}) A.cfg.values.erase(k);
            apply_sliders(); apply_volumes(); save_settings(); return;
        }
        if (n == "btnsave") { std::string e; message(cp(save_game(e) ? "Jogo salvo." : ("Não salvou: " + e)), 1); return; }
        if (n == "btnload") { std::string e; if (!load_game(e)) message(cp("Não carregou: " + e), 1); return; }
        if (n == "btndelete") {
            const bool had = DeleteFileW(widen(save_path()).c_str()) != 0;
            message(cp(had ? "Jogo salvo apagado." : "Não há jogo salvo para apagar."), 1); return;
        }
        return;
    }
    if (s == scr("Inventory") || s == scr("PipBoy")) { say("Ainda sem itens/Pip-Boy funcionando (só a tela original)."); return; }
}

// telas que recebem o mouse agora, da de cima para a de baixo
std::vector<GuiScreen*> input_screens() {
    std::vector<GuiScreen*> v;
    if (A.msg_open) { if (GuiScreen* m = scr("MessageBox")) v.push_back(m); return v; }
    switch (A.mode) {
        case Mode::MainMenu: if (GuiScreen* s = scr("Mainmenu")) v.push_back(s); break;
        case Mode::Options: if (GuiScreen* s = scr(kTabInt[A.opt_tab])) v.push_back(s); break;
        case Mode::CharGen: if (GuiScreen* s = scr("CharacterGeneration")) v.push_back(s); break;
        case Mode::Video: break;
        case Mode::Game:
            if (A.esc_open) { if (GuiScreen* s = scr("EscapeMenu")) v.push_back(s); return v; }
            if (A.panel == 1) { if (GuiScreen* s = scr("Inventory")) v.push_back(s); }
            if (A.panel == 2) { if (GuiScreen* s = scr("PipBoy")) v.push_back(s); }
            if (GuiScreen* s = scr("Actionbar")) v.push_back(s);
            break;
    }
    return v;
}

int options_tab_at(float x, float y) {
    GuiScreen* o = scr("Options");
    if (!o) return -1;
    for (int t = 0; t < TAB_COUNT; ++t)
        if (GuiWidget* w = o->find(kTabPic[t])) if (x >= w->rect.x0 && x < w->rect.x1 && y >= w->rect.y0 && y < w->rect.y1) return t;
    return -1;
}

int entity_at_mouse() {
    if (A.mode != Mode::Game || !A.game_ok) return -1;
    const MapData& m = A.game_map.map();
    int best = -1; float bd = 28.f * 28.f;
    for (std::size_t i = 0; i < m.entities.size(); ++i) {
        const MapEntity& e = m.entities[i];
        const float p[3] = {e.pos[0], e.pos[1] + 3.f, e.pos[2]};      // meio do corpo (humano ~6,7 unidades, medido no menu)
        float sx, sy;
        if (!project(p, sx, sy)) continue;
        const float d = (sx - static_cast<float>(A.mx)) * (sx - static_cast<float>(A.mx)) + (sy - static_cast<float>(A.my)) * (sy - static_cast<float>(A.my));
        if (d < bd) { bd = d; best = static_cast<int>(i); }
    }
    return best;
}


// ================================================================================================ jogo de verdade (v21)
// Tudo aqui e do F3VB (a logica do F3.exe nao foi decifrada): numeros marcados como HIPOTESE estao em docs/JOGO.md.
//  - chao: Ground (ground.cpp), altura por raio contra os triangulos do .8tre;
//  - andar: clique no chao -> caminho A* -> animacao de andar/correr do .gr2 da demo, sempre colado no chao;
//  - combate em TEMPO REAL: tecla A (AttackMode do F3.ini) ou clique na arma da barra -> clicar num personagem ataca; o atacado revida.
namespace {
std::mt19937 g_rng(12345);
int rnd(int a, int b) { return a >= b ? a : std::uniform_int_distribution<int>(a, b)(g_rng); }

bool mat_inverse(const float m[16], float inv[16]) {
    float t[16];
    t[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    t[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    t[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    t[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    t[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    t[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    t[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    t[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    t[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    t[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    t[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    t[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    t[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    t[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    t[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    t[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];
    float det = m[0]*t[0] + m[1]*t[4] + m[2]*t[8] + m[3]*t[12];
    if (std::fabs(det) < 1e-20f) return false;
    det = 1.f / det;
    for (int i = 0; i < 16; ++i) inv[i] = t[i] * det;
    return true;
}

// raio da camera pelo pixel do mouse (mundo)
bool mouse_ray(float o[3], float d[3]) {
    float pv[16];
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) {
        float v = 0;
        for (int k = 0; k < 4; ++k) v += A.proj.m[k * 4 + r] * A.view.m[c * 4 + k];
        pv[c * 4 + r] = v;
    }
    float inv[16];
    if (!mat_inverse(pv, inv)) return false;
    const float nx = 2.f * static_cast<float>(A.mx) / static_cast<float>(std::max(1, A.w)) - 1.f, ny = 1.f - 2.f * static_cast<float>(A.my) / static_cast<float>(std::max(1, A.h));
    float p[2][3];
    for (int z = 0; z < 2; ++z) {
        const float in[4] = {nx, ny, z ? 1.f : -1.f, 1.f};
        float out[4];
        for (int r = 0; r < 4; ++r) out[r] = inv[r] * in[0] + inv[4 + r] * in[1] + inv[8 + r] * in[2] + inv[12 + r] * in[3];
        if (std::fabs(out[3]) < 1e-12f) return false;
        for (int k = 0; k < 3; ++k) p[z][k] = out[k] / out[3];
    }
    for (int k = 0; k < 3; ++k) { o[k] = p[0][k]; d[k] = p[1][k] - p[0][k]; }
    return true;
}

LiveActor* actor(int i) { return i >= 0 && static_cast<std::size_t>(i) < A.game_actors.size() ? A.game_actors[static_cast<std::size_t>(i)].get() : nullptr; }
float dist2d(const LiveActor& a, const LiveActor& b) { return std::hypot(a.a.pos[0] - b.a.pos[0], a.a.pos[2] - b.a.pos[2]); }

void world_text(const std::string& t, const float p[3], std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    App::WorldText w;
    w.text = t; w.pos[0] = p[0]; w.pos[1] = p[1]; w.pos[2] = p[2]; w.t0 = A.t;
    w.rgba[0] = r; w.rgba[1] = g; w.rgba[2] = b; w.rgba[3] = 255;
    A.world_texts.push_back(w);
}

// HIPOTESE: dano/alcance/chance pelo tipo de arma na mao (a demo nao expoe essa tabela de forma que eu tenha decifrado)
void set_weapon_stats(LiveActor& L) {
    const std::string w = L.a.weapon_class;
    const bool dog = L.a.model.rfind("CR_", 0) == 0;
    if (dog) { L.dmg_min = 3; L.dmg_max = 6; L.range = 3.5f; L.hit_chance = 65; }
    else if (w == "2HH") { L.dmg_min = 8; L.dmg_max = 15; L.range = 40.f; L.hit_chance = 55; }
    else if (w == "1H" && !L.a.melee) { L.dmg_min = 5; L.dmg_max = 10; L.range = 35.f; L.hit_chance = 60; }
    else if (w == "1H") { L.dmg_min = 4; L.dmg_max = 8; L.range = 3.5f; L.hit_chance = 65; }
    else { L.dmg_min = 1; L.dmg_max = 4; L.range = 3.f; L.hit_chance = 60; }
}

void update_digits() {
    GuiScreen* ab = scr("Actionbar");
    LiveActor* P = actor(A.player);
    if (!ab || !P) return;
    const int v = std::clamp(P->hp, 0, 999);
    const int d[3] = {v / 100, (v / 10) % 10, v % 10};
    const char* suf[3] = {"100", "10", "1"};
    for (int i = 0; i < 3; ++i) ab->set_image(std::string("winHealthDigit") + suf[i], "f3_digit_" + std::to_string(d[i]) + ".tga");
}
}  // namespace

void init_gameplay() {
    A.player = -1; A.attack_mode = false; A.world_texts.clear(); A.player_dead_at = -1;
    int grounded = 0; float worst = 0; std::string worst_name;
    for (std::size_t i = 0; i < A.game_actors.size(); ++i) {
        LiveActor& L = *A.game_actors[i];
        const bool crt = L.a.description.rfind("CRT", 0) == 0;
        L.creature = crt && L.a.anim_length(AN_IDLE) > 0.1f && L.a.clip_index[AN_WALK] >= 0;
        if (L.a.entity < 0) { A.player = static_cast<int>(i); L.creature = true; }
        if (!L.creature) continue;
        if (L.a.entity >= 0) {
            const MapEntity& e = A.game_map.map().entities[static_cast<std::size_t>(L.a.entity)];
            L.name = clean_name(e).empty() ? e.prototype : clean_name(e);
        } else L.name = A.pc.name;
        L.max_hp = L.hp = L.a.entity < 0 ? A.pc.hit_points() : (L.a.model.rfind("CR_", 0) == 0 ? 25 : 50);   // NPC: HIPOTESE
        set_weapon_stats(L);
        float gy;
        if (A.ground.height(L.a.pos[0], L.a.pos[2], L.a.pos[1] + 0.5f, gy)) {
            const float dif = L.a.pos[1] - gy;
            if (std::fabs(dif) > std::fabs(worst)) { worst = dif; worst_name = L.name; }
            L.a.pos[1] = gy; ++grounded;
        } else LOGF("  chao: %s em (%.1f, %.1f, %.1f) SEM chao embaixo", L.name.c_str(), L.a.pos[0], L.a.pos[1], L.a.pos[2]);
    }
    LOGF("chao: %zu triangulos | %d personagens colados no chao (maior diferenca corrigida: %.2f em %s) | jogador = ator %d",
         A.ground.triangles(), grounded, worst, worst_name.c_str(), A.player);
    for (const auto& L : A.game_actors) if (L->creature) {
        std::string an;
        for (int r = 0; r < AN_COUNT; ++r) { an += std::string(" ") + actor_anim_name(r) + (L->a.clip_index[r] >= 0 ? "=ok" : "=FALTA"); }
        LOGF("  ator %s: vida %d, dano %d-%d, alcance %.0f |%s", L->name.c_str(), L->max_hp, L->dmg_min, L->dmg_max, L->range, an.c_str());
    }
    update_digits();
}

// passo do caminho; true = chegou ao fim
bool walk_step(LiveActor& L, float dt, int self) {
    if (L.path.size() < 2) return true;
    const float tx = L.path[0], tz = L.path[1];
    const float dx = tx - L.a.pos[0], dz = tz - L.a.pos[2], d = std::hypot(dx, dz);
    const float step = L.speed * dt;
    L.a.yaw = std::atan2(dx, dz);
    float nx = L.a.pos[0], nz = L.a.pos[2];
    if (d <= step) { nx = tx; nz = tz; L.path.erase(L.path.begin(), L.path.begin() + 2); }
    else { nx += dx / d * step; nz += dz / d * step; }
    for (std::size_t j = 0; j < A.game_actors.size(); ++j) {                   // nao atravessa outros personagens vivos
        const LiveActor& o = *A.game_actors[j];
        if (static_cast<int>(j) == self || !o.creature || o.dead) continue;
        const float od = std::hypot(nx - o.a.pos[0], nz - o.a.pos[2]);
        if (od < 1.6f && od < std::hypot(L.a.pos[0] - o.a.pos[0], L.a.pos[2] - o.a.pos[2])) { L.path.clear(); return true; }
    }
    float ny;
    if (!A.ground.walkable(L.a.pos[0], L.a.pos[2], L.a.pos[1], nx, nz, &ny)) { L.path.clear(); return true; }
    L.a.pos[0] = nx; L.a.pos[2] = nz; L.a.pos[1] = ny;
    return L.path.empty();
}

void go_to(LiveActor& L, float x, float z) {
    if (!A.ground.path(L.a.pos[0], L.a.pos[2], L.a.pos[1], x, z, L.path)) { L.path.clear(); return; }
    float len = 0, px = L.a.pos[0], pz = L.a.pos[2];
    for (std::size_t i = 0; i + 1 < L.path.size(); i += 2) { len += std::hypot(L.path[i] - px, L.path[i + 1] - pz); px = L.path[i]; pz = L.path[i + 1]; }
    // HIPOTESE de escala: humano ~6 unidades = 1,8 m -> andar 1,4 m/s ~ 5 un/s; correr ~12 un/s (o clipe de corrida tem 0,5 s)
    const bool run = len > 25.f || L.hostile;
    L.speed = run && L.a.clip_index[AN_RUN] >= 0 ? 12.f : 5.f;
    L.a.play(run && L.a.clip_index[AN_RUN] >= 0 ? AN_RUN : AN_WALK, static_cast<float>(A.t), true);
}

void hit(LiveActor& att, int ti) {
    LiveActor* T = actor(ti);
    if (!T || T->dead) return;
    float head[3] = {T->a.pos[0], T->a.pos[1] + std::max(T->a.height, 3.f) + 0.5f, T->a.pos[2]};
    int chance = att.hit_chance;
    if (&att == actor(A.player)) {                                               // jogador: pericia de verdade da ficha
        const int skill = att.a.weapon_class == "Una" ? 3 : (att.a.melee ? 2 : 1);
        chance = std::clamp(A.pc.skill_total(skill) / 2 + 20, 5, 95);           // HIPOTESE: 130 de Firearms ~ 85%
    }
    if (rnd(1, 100) > chance) { world_text("Errou", head, 200, 200, 200); return; }
    int dmg = rnd(att.dmg_min, att.dmg_max);
    if (&att == actor(A.player) && att.a.weapon_class == "Una") dmg += std::atoi(A.pc.derived(7).c_str() + 1) / 2;   // Melee Damage (+ST)
    T->hp -= dmg;
    world_text(std::to_string(dmg), head, 255, 70, 50);
    if (T->hp <= 0) {
        T->hp = 0; T->dead = true; T->path.clear(); T->target = -1;
        T->a.play(AN_DEATH, static_cast<float>(A.t), false);
        LOGF("combate: %s morreu", T->name.c_str());
        if (ti == A.player) { A.player_dead_at = A.t; A.log_lines.push_back("-You have died."); }
        else A.log_lines.push_back("-" + T->name + " died.");
    } else {
        T->a.play(AN_HIT, static_cast<float>(A.t), false);
        if (ti != A.player && !T->hostile) {                                     // o atacado revida, e quem esta perto do mesmo tipo tambem
            T->hostile = true; T->target = A.player;
            for (std::size_t j = 0; j < A.game_actors.size(); ++j) {
                LiveActor& o = *A.game_actors[j];
                if (!o.creature || o.dead || static_cast<int>(j) == A.player || o.hostile) continue;
                if (o.a.model == T->a.model && dist2d(o, *T) < 40.f) { o.hostile = true; o.target = A.player; }
            }
        }
    }
    if (ti == A.player) update_digits();
}

void update_gameplay(float dt) {
    if (A.player < 0) return;
    const float now = static_cast<float>(A.t);
    for (std::size_t i = 0; i < A.game_actors.size(); ++i) {
        LiveActor& L = *A.game_actors[i];
        if (!L.creature || L.dead) continue;
        const int self = static_cast<int>(i);
        L.cooldown -= dt; L.repath -= dt;
        // golpe sai no meio da animacao de ataque
        if (L.hit_at >= 0 && now >= L.hit_at) { L.hit_at = -1; hit(L, L.target); }
        const bool busy = (L.a.cur_anim == AN_ATTACK || L.a.cur_anim == AN_HIT) && !L.a.anim_done(now);
        LiveActor* T = actor(L.target);
        if (T && T->dead) { L.target = -1; T = nullptr; if (self != A.player) L.hostile = false; }
        if (T && !busy) {
            const float d = dist2d(L, *T);
            if (d > L.range) {
                if (L.repath <= 0 || L.path.empty()) { go_to(L, T->a.pos[0], T->a.pos[2]); L.repath = 0.6f; }
            } else {
                L.path.clear();
                L.a.yaw = std::atan2(T->a.pos[0] - L.a.pos[0], T->a.pos[2] - L.a.pos[2]);
                if (L.cooldown <= 0) {
                    L.a.play(AN_ATTACK, now, false);
                    const float len = std::max(0.4f, L.a.anim_length(AN_ATTACK));
                    L.hit_at = now + len * 0.5f;
                    L.cooldown = len + 0.35f;
                } else if (L.a.anim_done(now) || L.a.cur_anim != AN_IDLE) L.a.play(AN_IDLE, now, true);
            }
        }
        if (!busy && !L.path.empty()) {
            if (walk_step(L, dt, self) && !T) L.a.play(AN_IDLE, now, true);
        } else if (!busy && L.path.empty() && (L.a.cur_anim == AN_WALK || L.a.cur_anim == AN_RUN || L.a.anim_done(now))) L.a.play(AN_IDLE, now, true);
    }
    // camera acompanha o jogador enquanto ele anda
    LiveActor* P = actor(A.player);
    if (P && !P->path.empty() && setting("CameraFollow", 1.f) > 0.5f) {
        const float k = std::min(1.f, dt * 3.f);
        A.cam.tx += (P->a.pos[0] - A.cam.tx) * k; A.cam.tz += (P->a.pos[2] - A.cam.tz) * k; A.cam.ty = P->a.pos[1];
        clamp_camera();
    }
    if (A.player_dead_at >= 0 && A.t - A.player_dead_at > 4.0) { A.player_dead_at = -1; message(cp("Você morreu."), 1, [](int) { enter_mainmenu(); }); }
    A.world_texts.erase(std::remove_if(A.world_texts.begin(), A.world_texts.end(), [](const App::WorldText& w) { return A.t - w.t0 > 1.6; }), A.world_texts.end());
}

int actor_at_mouse() {
    int best = -1; float bd = 30.f * 30.f;
    for (std::size_t i = 0; i < A.game_actors.size(); ++i) {
        const LiveActor& L = *A.game_actors[i];
        if (!L.creature || static_cast<int>(i) == A.player) continue;
        const float p[3] = {L.a.pos[0], L.a.pos[1] + std::max(2.f, L.a.height * 0.5f), L.a.pos[2]};
        float sx, sy;
        if (!project(p, sx, sy)) continue;
        const float d = (sx - static_cast<float>(A.mx)) * (sx - static_cast<float>(A.mx)) + (sy - static_cast<float>(A.my)) * (sy - static_cast<float>(A.my));
        if (d < bd) { bd = d; best = static_cast<int>(i); }
    }
    return best;
}

void gameplay_click() {
    LiveActor* P = actor(A.player);
    if (!P || P->dead) return;
    const int ai = actor_at_mouse();
    LiveActor* T = actor(ai);
    if (T && !T->dead && (A.attack_mode || T->hostile)) {                       // atacar
        P->target = ai; P->path.clear(); P->repath = 0;
        LOGF("combate: jogador ataca %s", T->name.c_str());
        return;
    }
    if (T && T->a.entity >= 0 && entity_has_dialogue(T->a.entity) && !T->dead) { open_dialog(T->a.entity); return; }
    float o[3], d[3], hitp[3];
    if (!mouse_ray(o, d) || !A.ground.pick(o, d, hitp)) return;
    P->target = -1;
    go_to(*P, hitp[0], hitp[2]);
    if (P->path.empty()) { say("Não dá para chegar lá.", 1.5); LOGF("andar: sem caminho ate (%.1f, %.1f, %.1f)", hitp[0], hitp[1], hitp[2]); }
}

// sombra redonda sob cada personagem (o original tem sombra nos personagens; a demo pinta a do cenario no lightmap)
void draw_shadows() {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-2.f, -2.f);
    glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_VERTEX_ARRAY); glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    for (const auto& Lp : A.game_actors) {
        const LiveActor& L = *Lp;
        if (!L.creature) continue;
        const float r = std::max(1.2f, L.a.height * 0.28f);
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(0, 0, 0, 0.45f);
        glVertex3f(L.a.pos[0], L.a.pos[1] + 0.05f, L.a.pos[2]);
        glColor4f(0, 0, 0, 0.f);
        for (int k = 0; k <= 20; ++k) {
            const float an = static_cast<float>(k) / 20.f * 2.f * kPi;
            float gx = L.a.pos[0] + std::cos(an) * r, gz = L.a.pos[2] + std::sin(an) * r, gy = L.a.pos[1];
            float hy; if (A.ground.height(gx, gz, L.a.pos[1] + 0.5f, hy)) gy = hy;
            glVertex3f(gx, gy + 0.05f, gz);
        }
        glEnd();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glColor4f(1, 1, 1, 1);
}

// textos no mundo (2D, depois da interface): numeros de dano, nome sob o mouse, barra de vida dos feridos
void draw_world_texts() {
    auto at = [](const float p[3], float& vx, float& vy) {
        float sx, sy;
        if (!project(p, sx, sy)) return false;
        A.gl->to_virtual(static_cast<int>(sx), static_cast<int>(sy), vx, vy);
        return true;
    };
    for (const App::WorldText& w : A.world_texts) {
        const float age = static_cast<float>(A.t - w.t0);
        const float p[3] = {w.pos[0], w.pos[1] + age * 2.5f, w.pos[2]};
        float vx, vy;
        if (!at(p, vx, vy)) continue;
        std::uint8_t c[4] = {w.rgba[0], w.rgba[1], w.rgba[2], static_cast<std::uint8_t>(255.f * std::clamp(1.6f - age, 0.f, 1.f))};
        A.gl->text("Arial", 18, vx, vy, w.text, c, 1);
    }
    for (std::size_t i = 0; i < A.game_actors.size(); ++i) {
        const LiveActor& L = *A.game_actors[i];
        if (!L.creature || (static_cast<int>(i) != A.hover_actor && !(L.hp < L.max_hp && !L.dead))) continue;
        const float p[3] = {L.a.pos[0], L.a.pos[1] + std::max(L.a.height, 3.f) + 1.2f, L.a.pos[2]};
        float vx, vy;
        if (!at(p, vx, vy)) continue;
        if (static_cast<int>(i) == A.hover_actor) {
            const std::uint8_t c[4] = {static_cast<std::uint8_t>(L.hostile ? 255 : 120), static_cast<std::uint8_t>(L.hostile ? 90 : 220), 90, 255};
            A.gl->text("Arial", 14, vx, vy - 16.f, cp(L.name) + (L.dead ? " (morto)" : ""), c, 1);
        }
        if (!L.dead && L.max_hp > 0 && L.hp < L.max_hp) {
            const float w = 40.f, f = static_cast<float>(L.hp) / static_cast<float>(L.max_hp);
            A.gl->rect(vx - w * 0.5f, vy, vx + w * 0.5f, vy + 4.f, 0, 0, 0, 0.7f);
            A.gl->rect(vx - w * 0.5f, vy, vx - w * 0.5f + w * f, vy + 4.f, 0.9f, 0.15f, 0.1f, 0.9f);
        }
    }
}


// ================================================================================================ autoteste (v21)
// F3VB.exe --autoteste: sem ninguem mexer, passa por videos -> menu -> criacao de personagem -> cada mapa de verdade da demo, mede
// (animacao mexendo? som com pico? personagens no chao? andar? combate?), tira prints (logs\autoteste_*.bmp) e escreve o relatorio.
namespace {
float pose_sig(const Actor& a) {
    float s = 0;
    for (const ActorPart& p : a.parts) for (const auto& j : p.scene.joint_world) for (int k = 0; k < 16; ++k) s += std::fabs(j.m[k]) * static_cast<float>(k + 1);
    return s;
}
void at(bool ok, const std::string& what) {
    A.at_lines.push_back(std::string(ok ? "[OK]    " : "[FALHA] ") + what);
    if (!ok) ++A.at_fail;
    LOGF("autoteste: %s %s", ok ? "OK" : "FALHA", what.c_str());
}
void at_shot(const std::string& name) {
    std::vector<std::uint8_t> px(static_cast<std::size_t>(A.w) * static_cast<std::size_t>(A.h) * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, A.w, A.h, 0x80E0 /*GL_BGR*/, GL_UNSIGNED_BYTE, px.data());
    const int row = (A.w * 3 + 3) & ~3;
    std::vector<std::uint8_t> f(54 + static_cast<std::size_t>(row) * static_cast<std::size_t>(A.h), 0);
    auto w32 = [&](std::size_t o, std::uint32_t v) { for (int i = 0; i < 4; ++i) f[o + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i)); };
    f[0] = 'B'; f[1] = 'M'; w32(2, static_cast<std::uint32_t>(f.size())); w32(10, 54); w32(14, 40); w32(18, static_cast<std::uint32_t>(A.w)); w32(22, static_cast<std::uint32_t>(A.h));
    f[26] = 1; f[28] = 24;
    for (int y = 0; y < A.h; ++y) std::memcpy(&f[54 + static_cast<std::size_t>(y) * static_cast<std::size_t>(row)], &px[static_cast<std::size_t>(y) * static_cast<std::size_t>(A.w) * 3], static_cast<std::size_t>(A.w) * 3);
    const std::string path = path_join(path_join(A.exe, "logs"), "autoteste_" + name + ".bmp");
    if (FILE* fp = _wfopen(widen(path).c_str(), L"wb")) { std::fwrite(f.data(), 1, f.size(), fp); std::fclose(fp); }
}
}  // namespace

void autotest_tick() {
    if (!A.autotest) return;
    const double el = A.t - A.at_t0;
    auto next = [](int s) { A.at_step = s; A.at_t0 = A.t; };
    switch (A.at_step) {
        case 0:   // videos (os dois, com o ajudante e a binkw32.dll da demo)
            if (A.mode == Mode::Video && el > 4.0) {
                at(A.video.shown() > 20, "video " + std::string("IPlogo.mve") + ": " + std::to_string(A.video.shown()) + " quadros mostrados em 4 s (ajudante de 32 bits + binkw32.dll da demo)");
                at_shot("1_video"); video_skip(); next(1);
            } else if (A.mode == Mode::MainMenu && el > 1.0) { at(false, "videos: nao tocaram (ajudante, DLL ou StartupMovies=0; ver log)"); next(2); }
            break;
        case 1: if (A.mode == Mode::Video && el > 2.0) { video_skip(); } if (A.mode == Mode::MainMenu) next(2); break;
        case 2:   // menu
            if (el < 0.6) { A.audio.take_peak(); if (!A.menu_actors.empty()) A.at_sig = pose_sig(A.menu_actors[0]->a); }
            else if (el > 3.0 && el < 3.2) {
                at(A.menu_ok, "menu: mapa Mainmenu montado");
                at(A.menu_actors.size() >= 3, "menu: " + std::to_string(A.menu_actors.size()) + " modelos 3D (jogador, Armstrong, vagao)");
                if (!A.menu_actors.empty()) at(std::fabs(pose_sig(A.menu_actors[0]->a) - A.at_sig) > 1e-3f, "menu: personagens animando (pose mudou em 2,4 s)");
                bool train = false;
                for (auto& L : A.menu_actors) if (L->a.cur_anim == AN_WALK && !L->a.anim_done(static_cast<float>(A.t))) train = true;
                at(train, "menu: vagao chegando (clipe Walk do script 00_MainMenu_Train tocando)");
                at(A.menu_map.sky_lists() > 0, "menu: nuvens andando (" + std::to_string(A.menu_map.sky_lists()) + " parede de ceu)");
                const int pk = A.audio.take_peak();
                at(pk > 200 || A.audio.muted(), "som: musica do menu saindo do mixer (pico " + std::to_string(pk) + " de 32767)");
                at_shot("2_menu");
                next(5);
            }
            break;
        case 5: if (el > 8.0) { at_shot("3_menu_vagao_parado"); new_character(); enter_chargen(false); next(3); } break;
        case 3:   // criacao de personagem
            if (el > 0.5 && el < 0.7 && A.cg_actor) A.at_sig = pose_sig(A.cg_actor->a);
            else if (el > 2.5) {
                at(A.cg_actor != nullptr, "criacao: boneco 3D montado (" + (A.cg_actor ? A.cg_actor->a.description : std::string("-")) + ")");
                if (A.cg_actor) at(std::fabs(pose_sig(A.cg_actor->a) - A.at_sig) > 1e-3f, "criacao: boneco animando");
                at(A.pc.skill_total(1) == 130 && A.pc.derived(1) == "25", "criacao: Firearms 130 e AP 25 com tudo em 5 (como o print do original)");
                at_shot("4_criacao");
                A.at_maps.clear();
                for (const ResEntry* e : A.vfs.list(RT_MAP)) if (e->name.rfind("zz_", 0) != 0 && e->name != "Mainmenu" && e->name != "Default_StartMap") A.at_maps.push_back(e->name);
                std::sort(A.at_maps.begin(), A.at_maps.end());
                A.at_map = 0;
                start_loading(A.at_maps.empty() ? A.game_map_name : A.at_maps[0]);
                next(4);
            }
            break;
        case 4:   // cada mapa
            if (A.mode != Mode::Game) { if (el > 60) { at(false, "mapa " + A.pending_map + ": nao carregou em 60 s"); next(9); } break; }
            if (el < 1.5) break;
            if (A.at_target == -1) {
                const std::string mn = A.game_map.map().name;
                int crit = 0, grounded = 0;
                for (auto& L : A.game_actors) if (L->creature) {
                    ++crit;
                    float gy;
                    if (A.ground.height(L->a.pos[0], L->a.pos[2], L->a.pos[1] + 0.1f, gy) && std::fabs(gy - L->a.pos[1]) < 0.05f) ++grounded;
                }
                at(A.game_actors.size() > 0, "mapa " + mn + ": " + std::to_string(A.game_actors.size()) + " modelos 3D, " + std::to_string(crit) + " personagens/criaturas, fps " + std::to_string(static_cast<int>(A.fps)));
                at(grounded == crit, "mapa " + mn + ": " + std::to_string(grounded) + " de " + std::to_string(crit) + " personagens com os pes no chao (diferenca < 0,05)");
                const int pk = A.audio.take_peak();
                A.at_lines.push_back("        som no mapa: pico " + std::to_string(pk) + " de 32767");
                at_shot("5_mapa_" + mn);
                A.at_target = -2;
                LiveActor* P = A.player >= 0 ? A.game_actors[static_cast<std::size_t>(A.player)].get() : nullptr;
                if (P) {   // andar: 12 unidades para a frente do jogador
                    A.at_x = P->a.pos[0]; A.at_z = P->a.pos[2];
                    go_to(*P, P->a.pos[0] + std::sin(P->a.yaw) * 12.f, P->a.pos[2] + std::cos(P->a.yaw) * 12.f);
                    if (P->path.empty()) go_to(*P, P->a.pos[0] - std::sin(P->a.yaw) * 12.f, P->a.pos[2] - std::cos(P->a.yaw) * 12.f);
                }
                A.at_t0 = A.t;
            } else if (A.at_target == -2 && el > 4.0) {
                LiveActor* P = A.player >= 0 ? A.game_actors[static_cast<std::size_t>(A.player)].get() : nullptr;
                if (P) {
                    const float moved = std::hypot(P->a.pos[0] - A.at_x, P->a.pos[2] - A.at_z);
                    float gy; const bool on = A.ground.height(P->a.pos[0], P->a.pos[2], P->a.pos[1] + 0.1f, gy) && std::fabs(gy - P->a.pos[1]) < 0.05f;
                    at(moved > 5.f && on, "mapa " + A.game_map.map().name + ": jogador andou " + std::to_string(static_cast<int>(moved)) + " unidades com animacao de andar, sempre no chao");
                    // combate: o personagem vivo mais perto
                    int best = -1; float bd = 1e9f;
                    std::vector<float> tmp;
                    for (std::size_t i = 0; i < A.game_actors.size(); ++i) {
                        LiveActor& L = *A.game_actors[i];
                        if (!L.creature || L.dead || static_cast<int>(i) == A.player) continue;
                        const float d = std::hypot(L.a.pos[0] - P->a.pos[0], L.a.pos[2] - P->a.pos[2]);
                        // so alvos que da para alcancar a pe (numa caverna o mais perto pode estar do outro lado da parede)
                        if (d < bd && A.ground.path(P->a.pos[0], P->a.pos[2], P->a.pos[1], L.a.pos[0], L.a.pos[2], tmp, 20000)) { bd = d; best = static_cast<int>(i); }
                    }
                    if (best >= 0 && bd < 80.f) { P->target = best; A.at_target = best; A.at_t0 = A.t; break; }
                }
                A.at_target = -3;
            } else if (A.at_target >= 0 && el > 10.0) {
                LiveActor& T = *A.game_actors[static_cast<std::size_t>(A.at_target)];
                LiveActor* P = A.game_actors[static_cast<std::size_t>(A.player)].get();
                at(T.hp < T.max_hp || P->hp < P->max_hp, "mapa " + A.game_map.map().name + ": combate em tempo real com " + T.name + " (vida dele " + std::to_string(T.hp) + "/" + std::to_string(T.max_hp) +
                                                           ", jogador " + std::to_string(P->hp) + "/" + std::to_string(P->max_hp) + ")");
                at_shot("6_combate_" + A.game_map.map().name);
                A.at_target = -3;
            }
            if (A.at_target == -3) {
                A.at_target = -1;
                if (++A.at_map < A.at_maps.size()) { start_loading(A.at_maps[A.at_map]); A.at_t0 = A.t; } else next(9);
            }
            break;
        case 9: {
            SYSTEMTIME st; GetLocalTime(&st);
            char stamp[32]; std::snprintf(stamp, sizeof stamp, "%02d-%02d-%02d_%02d-%02d-%02d", st.wMonth, st.wDay, st.wYear % 100, st.wHour, st.wMinute, st.wSecond);
            const std::string path = path_join(path_join(A.exe, "logs"), std::string("autoteste_") + stamp + ".txt");
            if (FILE* fp = _wfopen(widen(path).c_str(), L"wb")) {
                std::fprintf(fp, "F3VB Engine v21 - autoteste (%s)\r\n\r\n", A.demo_dir.c_str());
                for (const std::string& l : A.at_lines) std::fprintf(fp, "%s\r\n", l.c_str());
                std::fprintf(fp, "\r\nRESULTADO: %s (%d falha(s))\r\n", A.at_fail ? "HA FALHAS" : "TUDO OK", A.at_fail);
                std::fclose(fp);
            }
            LOGF("autoteste terminado: %d falha(s) -> %s", A.at_fail, path.c_str());
            A.autotest = false;
            PostMessageW(A.hwnd, WM_CLOSE, 0, 0);
            break;
        }
        default: break;
    }
}

void mouse_down() {
    if (A.mode == Mode::Video) { video_skip(); return; }
    A.gl->to_virtual(A.mx, A.my, A.vx, A.vy);
    if (A.mode == Mode::Game && A.dlg_open && !A.msg_open && !A.esc_open) {
        const auto rows = reply_rows();
        for (std::size_t i = 0; i < rows.size(); ++i) if (A.vx >= rows[i].x0 && A.vx < rows[i].x1 && A.vy >= rows[i].y0 && A.vy < rows[i].y1) { dialog_choose(i); return; }
    }
    for (GuiScreen* s : input_screens()) {
        GuiWidget* w = s->hit(A.vx, A.vy);
        if (!w) continue;
        std::string base; GuiWidget* region = nullptr; GuiWidget* thumb = nullptr;
        if (A.mode == Mode::Options && slider_names(s, w->name, base, region, thumb)) {     // slider: arrasta
            A.slider = base; A.slider_screen = s;
            slider_drag(A.vx);
            return;
        }
        A.press_screen = s; A.press_widget = w;
        s->set_pressed(w); s->set_hover(w);
        A.audio.sfx(w->sounds[0].empty() ? std::string("f3_button_down_01") : w->sounds[0]);
        return;
    }
    if (A.mode == Mode::Options && !A.msg_open) {
        const int t = options_tab_at(A.vx, A.vy);
        if (t >= 0) { A.opt_tab = t; A.audio.sfx("f3_button_down_01"); LOGF("aba de opcoes: %s", kTabInt[t]); }
        return;
    }
    if (A.mode == Mode::Game && !A.msg_open && !A.esc_open) {
        gameplay_click();
    }
}

void mouse_up() {
    A.gl->to_virtual(A.mx, A.my, A.vx, A.vy);
    if (!A.slider.empty()) { A.slider.clear(); A.slider_screen = nullptr; save_settings(); return; }
    GuiScreen* s = A.press_screen;
    GuiWidget* w = A.press_widget;
    A.press_screen = nullptr; A.press_widget = nullptr;
    if (!s || !w) return;
    s->set_pressed(nullptr);
    if (s->hit(A.vx, A.vy) == w) {
        A.audio.sfx(w->sounds[1].empty() ? std::string("f3_button_up_01") : w->sounds[1]);
        const std::string name = w->name;
        on_click(s, name);
    }
}

void update_hover() {
    A.gl->to_virtual(A.mx, A.my, A.vx, A.vy);
    for (auto& kv : A.screens) kv.second->set_hover(nullptr);
    for (GuiScreen* s : input_screens()) { if (GuiWidget* w = s->hit(A.vx, A.vy)) { s->set_hover(w); break; } }
    A.dlg_hover = -1;
    if (A.dlg_open) { const auto rows = reply_rows(); for (std::size_t i = 0; i < rows.size(); ++i) if (A.vx >= rows[i].x0 && A.vx < rows[i].x1 && A.vy >= rows[i].y0 && A.vy < rows[i].y1) A.dlg_hover = static_cast<int>(i); }
    A.hover_ent = (A.mode == Mode::Game && !A.esc_open && !A.msg_open && !A.dlg_open) ? entity_at_mouse() : -1;
    A.hover_actor = (A.mode == Mode::Game && !A.esc_open && !A.msg_open && !A.dlg_open) ? actor_at_mouse() : -1;
    if (A.mode == Mode::CharGen && !A.msg_open) {
        if (GuiScreen* s = scr("CharacterGeneration")) {
            const GuiWidget* w = tip_widget_at(s, A.vx, A.vy);
            if (w && w != A.cg_tip) set_chargen_tip(s, w);          // a dica fica ate passar por outro item (como no print: "Science")
        }
    }
}

// ------------------------------------------------------------------------------------------------ desenho
void draw_cursor() {
    std::string cur = "GUI_DefaultCursor.tga";
    float size = 32;
    if (A.mode == Mode::Game && A.hover_actor >= 0 && (A.attack_mode || A.game_actors[static_cast<std::size_t>(A.hover_actor)]->hostile)) { cur = "AttackCursor.tga"; size = 40; }
    else if (A.mode == Mode::Game && A.hover_ent >= 0 && entity_has_dialogue(A.hover_ent)) { cur = "TalkCursor.tga"; size = 40; }
    else if (A.mode == Mode::Game && A.attack_mode) { cur = "TargetCursor.tga"; size = 40; }
    if (!A.ui.has_image(cur)) cur = "GUI_DefaultCursor.tga";
    if (!A.ui.has_image(cur)) return;
    A.gl->image(cur, A.vx, A.vy, A.vx + size, A.vy + size);
}

void draw_toast() {
    if (A.toast.empty() || A.t > A.toast_until) return;
    const std::uint8_t col[4] = {120, 255, 120, 255};
    const float w = A.gl->text_width("Fallouty", 16, A.toast) + 24;
    const float x = (1024.f - w) * 0.5f, y = A.mode == Mode::Game ? 560.f : 700.f;
    A.gl->rect(x, y, x + w, y + 26, 0, 0, 0, 0.7f);
    A.gl->text("Fallouty", 16, 512, y + 4, A.toast, col, 1);
}

void draw_debug() {
    if (!A.debug) return;
    char b[512];
    std::snprintf(b, sizeof b, "%s | %.0f fps | %dx%d MSAA %dx aniso %.0fx | modo %d | %s | %s", kVersion, A.fps, A.w, A.h, A.msaa, A.aniso, static_cast<int>(A.mode),
                  A.mode == Mode::Game ? A.game_map.map().name.c_str() : "menu", A.vfs.demo_mode() ? "demo original (.grp)" : "copia extraida");
    const std::uint8_t col[4] = {255, 255, 120, 255};
    A.gl->rect(0, 0, 1024, 18, 0, 0, 0, 0.6f);
    A.gl->text("Gothic 821 Condensed BT", 13, 6, 2, b, col, 0);
    if (A.mode == Mode::Game) {
        std::snprintf(b, sizeof b, "camera alvo (%.1f, %.1f) az %.0f el %.1f fov %.1f dist %.0f | entidade sob o mouse: %d", A.cam.tx, A.cam.tz, A.cam.az, A.cam.el, A.cam.fov, A.cam.dist, A.hover_ent);
        A.gl->rect(0, 18, 1024, 36, 0, 0, 0, 0.6f);
        A.gl->text("Gothic 821 Condensed BT", 13, 6, 20, b, col, 0);
    }
}

void render() {
    if (A.mode == Mode::Video) {
        glViewport(0, 0, A.w, A.h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        A.video.draw(A.w, A.h);
        SwapBuffers(A.hdc);
        return;
    }
    if (!A.pending_map.empty()) {
        glViewport(0, 0, A.w, A.h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else if (A.mode == Mode::Game || (A.mode == Mode::Options && A.options_back == Mode::Game) || (A.mode == Mode::CharGen && A.chargen_view)) {
        if (A.game_ok) {
            set_3d(A.cam, 50.f, A.cam.dist + 2000.f, &A.game_map.map());
            A.game_map.draw(A.lightmaps, A.wire);
            draw_shadows();
            draw_actors(A.game_actors);
            glDisable(GL_FOG);
            if (A.markers) A.game_map.draw_entity_markers(A.hover_ent);
        }
    } else render_menu_scene();
    if (A.msaa) glDisable(0x809D);
    A.gl->begin(A.w, A.h);
    if (!A.pending_map.empty()) {
        // tela de carregamento como a do original: so a moldura com a imagem "LOADING!", centrada, fundo preto
        if (GuiScreen* l = scr("Loading")) { center_screen(l); l->set_image("Picture", "main-loading.bmp"); build_and_draw(l); }
    } else {
        switch (A.mode) {
            case Mode::MainMenu: build_and_draw(scr("Mainmenu")); break;
            case Mode::Video: break;
            case Mode::Options:
                if (A.options_back == Mode::Game) build_and_draw(scr("Actionbar"));   // como no print do original: opcoes sobre o jogo, barra embaixo
                build_and_draw(scr(kTabInt[A.opt_tab]));
                build_and_draw(scr("Options"));
                break;
            case Mode::CharGen:
                // v21: telas 2D cheias (4:3): faixas pretas nas laterais em vez do menu 3D aparecendo atras
                A.gl->rect(-4000.f, -4000.f, 0.f, 4768.f, 0, 0, 0, 1); A.gl->rect(1024.f, -4000.f, 5024.f, 4768.f, 0, 0, 0, 1);
                A.gl->rect(-4000.f, -4000.f, 5024.f, 0.f, 0, 0, 0, 1); A.gl->rect(-4000.f, 768.f, 5024.f, 4768.f, 0, 0, 0, 1);
                build_and_draw(scr("CharacterGeneration"));
                A.gl->end();
                draw_chargen_actor();
                A.gl->begin(A.w, A.h);
                break;
            case Mode::Game:
                draw_world_texts();
                build_and_draw(scr("Actionbar"));
                draw_message_log();
                if (A.panel == 1) build_and_draw(scr("Inventory"));
                if (A.panel == 2) build_and_draw(scr("PipBoy"));
                if (A.dlg_open) { build_and_draw(scr("Dialog")); draw_dialog_replies(); }
                if (A.esc_open) build_and_draw(scr("EscapeMenu"));
                break;
        }
    }
    if (A.msg_open) { A.gl->rect(-2000, -2000, 3000, 3000, 0, 0, 0, 0.35f); build_and_draw(scr("MessageBox")); }
    draw_toast();
    draw_debug();
    draw_cursor();
    A.gl->end();
    SwapBuffers(A.hdc);
}

// ------------------------------------------------------------------------------------------------ atualizacao
void update(double dt) {
    autotest_tick();                                   // v21: --autoteste
    if (A.mode == Mode::Video) { if (A.video.finished()) video_next(); return; }
    if (!A.pending_map.empty()) {                      // mostra a tela de loading por 2 quadros e entao carrega (bloqueante)
        if (++A.loading_frames >= 3) finish_loading();
        return;
    }
    if (A.mode != Mode::Game || A.esc_open || A.msg_open || (GetForegroundWindow() != A.hwnd && !A.autotest)) return;
    if (!A.dlg_open && A.panel == 0) update_gameplay(static_cast<float>(dt));
    const float f = static_cast<float>(dt);
    auto key = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    if (key(VK_CONTROL) || A.dlg_open) return;
    // teclas do F3.ini original: setas esquerda/direita = azimute; cima/baixo = elevacao (CameraTop/CameraDown); Page Up/Down = zoom (FOV);
    // teclado numerico 8/2/4/6 = rolar. Extras do F3VB: W A S D tambem rolam, roda do mouse = zoom, botao direito/meio (ou X) arrastando = girar.
    // Velocidades do sys.ini [Camera] x 0,25 (360 graus/s e rapido demais segurando a tecla; ajuste do F3VB).
    float fwd = 0, side = 0;
    if (key(VK_NUMPAD8)) fwd += 1;
    if (key(VK_NUMPAD2)) fwd -= 1;
    if (key(VK_NUMPAD4)) side -= 1;
    if (key(VK_NUMPAD6)) side += 1;
    const float border = A.lim.scroll_border * static_cast<float>(std::min(A.w, A.h));
    if (A.fullscreen || GetCapture() == nullptr) {
        if (A.mx >= 0 && A.mx <= border) side -= 1;
        if (A.mx >= A.w - 1 - border && A.mx < A.w) side += 1;
        if (A.my >= 0 && A.my <= border) fwd += 1;
        if (A.my >= A.h - 1 - border && A.my < A.h) fwd -= 1;
    }
    if (key(VK_LEFT)) A.cam.az += A.lim.az_speed * 0.25f * f;
    if (key(VK_RIGHT)) A.cam.az -= A.lim.az_speed * 0.25f * f;
    if (key(VK_UP)) A.cam.el += A.lim.el_speed * 0.25f * f;
    if (key(VK_DOWN)) A.cam.el -= A.lim.el_speed * 0.25f * f;
    if (key(VK_PRIOR)) A.cam.fov -= A.lim.fov_speed * f;
    if (key(VK_NEXT)) A.cam.fov += A.lim.fov_speed * f;
    // rolar no plano, relativo ao azimute (escala com o zoom: o mesmo passo na tela)
    const float speed = A.lim.scroll_speed * (A.cam.fov / 10.f) * f;
    const float az = A.cam.az * kPi / 180.f;
    const float fx = std::sin(az), fz = std::cos(az);      // "frente" na tela = direcao do olhar no chao
    const float rx = std::cos(az), rz = -std::sin(az);     // "direita" na tela (mundo de mao esquerda)
    A.cam.tx += (fx * fwd + rx * side) * speed;
    A.cam.tz += (fz * fwd + rz * side) * speed;
    clamp_camera();
}

// ------------------------------------------------------------------------------------------------ janela
void toggle_fullscreen() {
    if (!A.fullscreen) {
        A.win_style = GetWindowLongPtrW(A.hwnd, GWL_STYLE);
        GetWindowRect(A.hwnd, &A.win_rect);
        MONITORINFO mi{}; mi.cbSize = sizeof mi;
        GetMonitorInfoW(MonitorFromWindow(A.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongPtrW(A.hwnd, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP | WS_VISIBLE));
        SetWindowPos(A.hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
        A.fullscreen = true;
    } else {
        SetWindowLongPtrW(A.hwnd, GWL_STYLE, A.win_style);
        SetWindowPos(A.hwnd, nullptr, A.win_rect.left, A.win_rect.top, A.win_rect.right - A.win_rect.left, A.win_rect.bottom - A.win_rect.top, SWP_FRAMECHANGED | SWP_NOZORDER);
        A.fullscreen = false;
    }
    set_setting("Fullscreen", A.fullscreen ? 1.f : 0.f);
    save_settings();
    LOGF("tela cheia: %s", A.fullscreen ? "sim" : "nao");
}

void key_down(WPARAM k) {
    if (A.mode == Mode::Video) { if (k == VK_ESCAPE || k == VK_SPACE || k == VK_RETURN) video_skip(); return; }
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (A.mode == Mode::CharGen && A.name_edit) {
        if (k == VK_RETURN || k == VK_ESCAPE) { A.name_edit = false; refresh_chargen(); }
        else if (k == VK_BACK && !A.pc.name.empty()) { A.pc.name.pop_back(); refresh_chargen(); }
        return;
    }
    if (A.msg_open) {
        if (k == VK_RETURN) close_message(scr("MessageBox") && !scr("MessageBox")->is_hidden(scr("MessageBox")->find("winButton1")) ? 1 : 2);
        else if (k == VK_ESCAPE) close_message(scr("MessageBox") && !scr("MessageBox")->is_hidden(scr("MessageBox")->find("winButton3")) ? 3 : 2);
        return;
    }
    if (k == VK_F1 || k == VK_F3) { A.debug = !A.debug; return; }                       // F1 = ToggleStatistics no original
    if (k == VK_F8) { A.audio.set_muted(!A.audio.muted()); say(A.audio.muted() ? "Som desligado (F8)" : "Som ligado (F8)", 2); return; }
    if (ctrl && k == 'F') { toggle_fullscreen(); return; }                              // Ctrl+F = ToggleFullscreen no original
    switch (A.mode) {
        case Mode::Video: break;
        case Mode::MainMenu:
            if (k == VK_RETURN && scr("CharacterGeneration")) { new_character(); enter_chargen(false); }
            break;
        case Mode::Options:
            if (k == VK_ESCAPE) { save_settings(); close_options(); }
            break;
        case Mode::CharGen:
            if (k == VK_ESCAPE) { if (A.chargen_view) A.mode = Mode::Game; else enter_mainmenu(); }
            break;
        case Mode::Game:
            if (A.dlg_open) {
                if (k == VK_ESCAPE) close_dialog();
                else if (k >= '1' && k <= '9') dialog_choose(static_cast<std::size_t>(k - '1'));
                else if (k >= VK_NUMPAD1 && k <= VK_NUMPAD9) dialog_choose(static_cast<std::size_t>(k - VK_NUMPAD1));
                return;
            }
            if (k == VK_ESCAPE) { if (A.panel) A.panel = 0; else A.esc_open = !A.esc_open; if (A.esc_open) A.audio.sfx("f3_messagebox_open"); return; }
            if (A.esc_open) break;
            if (ctrl) {                                                                  // Ctrl+... do F3.ini original
                if (k == 'L') { A.lightmaps = !A.lightmaps; say(A.lightmaps ? "Iluminação (lightmaps) ligada" : "Iluminação (lightmaps) desligada", 2); }
                else if (k == 'W') A.wire = !A.wire;
                else if (k == 'N') { new_character(); enter_chargen(false); }              // RequestNewGame
                break;
            }
            if (k == 'A') { A.attack_mode = !A.attack_mode; say(A.attack_mode ? "Modo de ataque: clique num alvo" : "Modo de ataque desligado", 1.5); }   // AttackMode
            else if (k == 'I') A.panel = A.panel == 1 ? 0 : 1;                           // InventoryWindow
            else if (k == 'P') A.panel = A.panel == 2 ? 0 : 2;                           // PipBoy
            else if (k == 'C') enter_chargen(true);                                      // CharacterRecordWindow
            else if (k == 'G') open_options(TAB_GAME, Mode::Game);                       // GameOptions
            else if (k == 'V') open_options(TAB_VIDEO, Mode::Game);                      // VideoOptions
            else if (k == 'L') open_options(TAB_LOAD, Mode::Game);                       // LoadGameOptions
            else if (k == 'H') open_options(TAB_HOTKEYS, Mode::Game);                    // HotKeyOptions
            else if (k == 'M') open_options(TAB_MULTI, Mode::Game);                      // MultiplayerOptions
            else if (k == VK_HOME) place_camera_at_start();                              // CameraReset
            else if (k == VK_F5) { std::string e; say(save_game(e) ? std::string("Jogo salvo (F5).") : "Não salvou: " + e); }   // v20: atalhos extras do F3VB
            else if (k == VK_F9) { std::string e; if (!load_game(e)) say("Não carregou: " + e); }
            else if (k == VK_F2) A.markers = !A.markers;                                 // extra do F3VB: marcadores das entidades
            else if (k == VK_F4) { A.fog = !A.fog; say(A.fog ? "Névoa do mapa ligada (F4)" : "Névoa desligada (F4)", 2); }
            break;
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: A.w = std::max(1, static_cast<int>(LOWORD(lp))); A.h = std::max(1, static_cast<int>(HIWORD(lp))); return 0;
        case WM_SETCURSOR: if (LOWORD(lp) == HTCLIENT) { SetCursor(nullptr); return TRUE; } break;          // o cursor e desenhado pelo jogo (GUI_DefaultCursor.tga)
        case WM_MOUSEMOVE: {
            const int x = static_cast<short>(LOWORD(lp)), y = static_cast<short>(HIWORD(lp));
            const int dx = x - A.mx, dy = y - A.my;
            A.mx = x; A.my = y;
            if (!A.slider.empty() && A.ldown) { A.gl->to_virtual(x, y, A.vx, A.vy); slider_drag(A.vx); }
            const bool rotate = A.rdown || ((GetKeyState('X') & 0x8000) != 0);           // X = CameraMouseRotate no original
            if (rotate && A.mode == Mode::Game && !A.dlg_open) { A.cam.az -= dx * 0.4f; A.cam.el = std::clamp(A.cam.el + dy * 0.3f, A.lim.el_min, A.lim.el_max); }
            return 0;
        }
        case WM_LBUTTONDOWN: A.ldown = true; A.mx = static_cast<short>(LOWORD(lp)); A.my = static_cast<short>(HIWORD(lp)); SetCapture(hwnd); mouse_down(); return 0;
        case WM_LBUTTONUP: A.ldown = false; ReleaseCapture(); mouse_up(); return 0;
        case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: A.rdown = true; SetCapture(hwnd); return 0;
        case WM_RBUTTONUP: case WM_MBUTTONUP: A.rdown = false; ReleaseCapture(); return 0;
        case WM_MOUSEWHEEL: if (A.mode == Mode::Game) { const float d = static_cast<float>(static_cast<short>(HIWORD(wp))) / 120.f; A.cam.fov -= d * 1.f; clamp_camera(); } return 0;
        case WM_CHAR:
            if (A.mode == Mode::CharGen && A.name_edit && wp >= 32 && wp < 256 && wp != 127 && A.pc.name.size() < 16) { A.pc.name.push_back(static_cast<char>(wp)); refresh_chargen(); }
            return 0;
        case WM_SYSKEYDOWN: if (wp == VK_RETURN) { toggle_fullscreen(); return 0; } break;
        case WM_KEYDOWN: key_down(wp); return 0;
        case WM_DESTROY: LOGF("janela fechada (%.1f s, %llu quadros)", A.t, A.total_frames); PostQuitMessage(0); return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------------------------------------------ escolha da pasta da demo
// Ordem: --demo <pasta> > --assets <original_assets> (copia extraida, para depuracao) > pasta gravada no F3VB.ini > F3VB_DEMO_DIR >
// seletor de pastas do Windows. A pasta escolhida no seletor (ou por --demo) e gravada no F3VB.ini ao lado do .exe.
bool open_source(const std::vector<std::string>& args) {
    std::string arg_demo, arg_assets;
    bool repick = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if ((args[i] == "--demo" || args[i] == "--pasta") && i + 1 < args.size()) arg_demo = args[++i];
        else if (args[i] == "--assets" && i + 1 < args.size()) arg_assets = args[++i];
        else if (args[i] == "--escolher-pasta" || args[i] == "--reset") repick = true;
    }
    if (arg_assets.empty() && arg_demo.empty()) arg_assets = env_utf8("F3VB_ASSETS_DIR");
    std::string err;
    if (!arg_demo.empty()) {
        if (A.vfs.open_demo(arg_demo, err)) { A.demo_dir = arg_demo; A.cfg.values["demo"] = arg_demo; save_settings(); return true; }
        LOGF("--demo \"%s\" recusada: %s", arg_demo.c_str(), err.c_str());
        box("A pasta passada em --demo não é a demo do Van Buren:\n" + arg_demo + "\n\n" + err, MB_OK | MB_ICONWARNING);
    } else if (!arg_assets.empty()) {
        if (A.vfs.open_extracted(arg_assets, err)) { A.assets_dir = arg_assets; return true; }
        LOGF("copia extraida \"%s\" recusada: %s", arg_assets.c_str(), err.c_str());
    }
    if (!repick) {
        const std::string saved = A.cfg.get("demo");
        if (!saved.empty()) {
            if (A.vfs.open_demo(saved, err)) { A.demo_dir = saved; LOGF("pasta da demo (F3VB.ini): %s", saved.c_str()); return true; }
            LOGF("pasta gravada no F3VB.ini nao serve mais (%s): %s", saved.c_str(), err.c_str());
            box("A pasta da demo gravada não está mais disponível:\n" + saved + "\n\n" + err + "\n\nEscolha a pasta de novo.", MB_OK | MB_ICONINFORMATION);
        }
        const std::string envd = env_utf8("F3VB_DEMO_DIR");
        if (!envd.empty() && A.vfs.open_demo(envd, err)) { A.demo_dir = envd; A.cfg.values["demo"] = envd; save_settings(); return true; }
    }
    std::string start = A.cfg.get("demo");
    for (;;) {
        const std::string pick = pick_folder(A.hwnd, L"F3VB: escolha a pasta da demo do Van Buren (a que tem F3.exe, resource.rht e a pasta data)", start);
        if (pick.empty()) { LOGF("seletor de pasta cancelado"); return false; }
        if (A.vfs.open_demo(pick, err)) {
            A.demo_dir = pick;
            A.cfg.values["demo"] = pick;
            save_settings();
            LOGF("pasta da demo escolhida e gravada em %s: %s", A.config_path.c_str(), pick.c_str());
            return true;
        }
        LOGF("pasta escolhida recusada (%s): %s", pick.c_str(), err.c_str());
        if (box("Esta pasta não é a demo do Van Buren:\n" + pick + "\n\n" + err + "\n\nEscolher outra pasta?", MB_YESNO | MB_ICONWARNING) != IDYES) return false;
        start = pick;
    }
}

void load_fonts_and_texts() {
    std::size_t nf = 0;
    std::vector<std::string> font_dirs, stf_files;
    if (!A.demo_dir.empty()) { font_dirs.push_back(path_join(A.demo_dir, "Fonts")); stf_files.push_back(path_join(A.demo_dir, "English.stf")); }
    if (!A.assets_dir.empty()) {
        font_dirs.push_back(path_join(path_join(A.assets_dir, "RETAIL"), "Fonts"));
        stf_files.push_back(path_join(path_join(A.assets_dir, "RETAIL"), "English.stf"));
        stf_files.push_back(path_join(path_join(path_join(A.assets_dir, "CONVERTED"), "scripts"), "English.stf"));
        const std::string d = env_utf8("F3VB_DEMO_DIR");
        if (!d.empty()) { font_dirs.push_back(path_join(d, "Fonts")); stf_files.push_back(path_join(d, "English.stf")); }
    }
    const std::string envstf = env_utf8("F3VB_STF");
    if (!envstf.empty()) stf_files.insert(stf_files.begin(), envstf);
    for (const std::string& d : font_dirs) nf += A.ui.add_font_dir(d);
    {   // v20: fontes do Windows (Arial etc.): o jogo original, sem as fontes da demo instaladas, desenha os textos em Arial (prints)
        wchar_t wd[MAX_PATH] = {0};
        if (GetWindowsDirectoryW(wd, MAX_PATH)) {
            const std::size_t ns = A.ui.add_system_fonts(path_join(narrow(wd), "Fonts"));
            LOGF("fontes do Windows: %zu (Arial, Arial Black, Times New Roman, Lucida Console)", ns);
        }
        A.ui.set_faithful_fonts(setting("DemoFonts", 0.f) < 0.5f);
        LOGF("modo de fontes: %s (F3VB.ini: DemoFonts=1 usa as fontes da pasta Fonts da demo)", A.ui.faithful_fonts() ? "fiel ao original (Arial no lugar das fontes nao instaladas)" : "fontes da demo");
    }
    std::string fonts;
    for (const std::string& f : A.ui.font_names()) fonts += " [" + f + "]";
    LOGF("fontes TrueType: %zu%s%s", A.ui.font_count(), fonts.c_str(), A.ui.font_count() ? "" : " (NENHUMA: a pasta Fonts da demo esta faltando)");
    LOGF("fontes do .int -> TrueType: Fallouty=%s | Gothic 821 Condensed BT=%s | Arial=%s | Lucida Console=%s", A.ui.resolve_font("Fallouty").c_str(),
         A.ui.resolve_font("Gothic 821 Condensed BT").c_str(), A.ui.resolve_font("Arial").c_str(), A.ui.resolve_font("Lucida Console").c_str());
    (void)nf;
    for (const std::string& s : stf_files) {
        std::vector<std::uint8_t> d;
        std::string err;
        if (!read_whole_file(s, d)) continue;
        if (A.stf.load(d.data(), d.size(), err)) { std::string e2; A.ui.load_stf(s, e2); LOGF("textos: %s (%zu entradas; ids 1-based)", s.c_str(), A.stf.count()); break; }
    }
    if (!A.stf.loaded()) LOGF("AVISO: English.stf nao encontrado: rotulos e dialogos ficam sem texto");
}

void load_everything() {
    A.ui.set_vfs(&A.vfs);
    load_sys_ini();
    load_fonts_and_texts();
    for (const char* n : {"Mainmenu", "Options", "Options_Game", "Options_Video", "Options_Hotkeys", "Options_Load", "Options_Multiplayer", "CharacterGeneration",
                          "Loading", "Actionbar", "EscapeMenu", "MessageBox", "Dialog", "Inventory", "PipBoy"})
        load_screen(n);
    if (GuiScreen* s = scr("CharacterGeneration")) s->set_hidden("winThresholds", true);
    // camera do jogo e mapa inicial (sys.ini)
    A.lim.dist = sysf("camera.distance max", 350.f);
    A.lim.el_min = sysf("camera.elevation min", 38.5f);
    A.lim.el_max = sysf("camera.elevation max", 89.f);
    A.lim.fov_min = sysf("camera.fov min", 6.f);
    A.lim.fov_max = sysf("camera.fov max", 15.f);
    A.lim.az_speed = sysf("camera.azimuth speed", 360.f);
    A.lim.el_speed = sysf("camera.elevation speed", 360.f);
    A.lim.fov_speed = sysf("camera.fov speed", 6.5f);
    A.lim.scroll_border = sysf("camera.scroll border", 0.01f);
    A.lim.scroll_speed = sysf("camera.scroll speed", 96.f);
    {
        std::string sm = syss("server.start map", "00_03_Tutorial_Junktown.map");
        const std::size_t dot = sm.find_last_of('.');
        if (dot != std::string::npos) sm = sm.substr(0, dot);
        A.game_map_name = A.start_map_override.empty() ? sm : A.start_map_override;
        A.start_entry = static_cast<int>(sysf("server.start map entry point", 0.f));
        LOGF("camera do jogo (sys.ini): distancia %.0f, elevacao %.1f a %.1f, FOV %.1f a %.1f | mapa inicial: %s", A.lim.dist, A.lim.el_min, A.lim.el_max, A.lim.fov_min,
             A.lim.fov_max, A.game_map_name.c_str());
    }
    // personagem inicial: Default_PC.CRT da demo (pericias 80/80/80/75/75, idade 21)
    {
        std::vector<std::uint8_t> d;
        if (A.vfs.read("Default_PC", RT_CRT, d) && A.pc_template.load_crt(d.data(), d.size(), false))
            LOGF("personagem padrao (Default_PC.CRT): idade %d, pericias base %d/%d/%d/%d/%d/%d/%d/%d...", A.pc_template.age, A.pc_template.skill_points[0], A.pc_template.skill_points[1],
                 A.pc_template.skill_points[2], A.pc_template.skill_points[3], A.pc_template.skill_points[4], A.pc_template.skill_points[5], A.pc_template.skill_points[6], A.pc_template.skill_points[7]);
        else LOGF("AVISO: Default_PC.CRT nao lido: uso os valores medidos na demo");
    }
    // som (direto do Sounds.grp)
    A.audio.init([](const std::string& name, std::vector<std::uint8_t>& out) { return A.vfs.read(name, RT_WAV, out); }, &A.log);
    apply_volumes();
    apply_sliders();
    // mapa do menu (montado direto da demo)
    std::string merr;
    setup_menu_camera();
    A.menu_ok = load_map_into(A.menu_map, "Mainmenu", merr);
    if (A.menu_ok) LOGF("menu: %zu lote(s) de triangulos reconhecidos como parede de ceu (nuvens andando)", A.menu_map.sky_lists());
    if (!A.menu_ok) LOGF("mapa do menu nao abriu: %s (uso o fundo 2D f3_front_end.tga)", merr.c_str());
    else { build_actors(A.menu_map.map(), A.menu_actors); A.menu_light = light_for(A.menu_map.map()); }
    // scripts (direto do Scripts.grp): biblioteca por nome, lida pela Vfs
    A.scripts.clear();
    A.scripts.reader = [](const std::string& path, std::vector<std::uint8_t>& out) { return A.vfs.read(path, RT_AMX, out); };
    for (const ResEntry* e : A.vfs.list(RT_AMX)) A.scripts.add(e->name, e->name);
    for (const ResEntry* e : A.vfs.list(RT_SMA)) A.scripts.add(e->name, e->name);
    LOGF("scripts: %zu na demo | %zu com dialogo", A.scripts.size(), A.scripts.dialogue_scripts().size());
    // videos de abertura (Movies\IPlogo.mve e BISlogo.mve): sao Bink 1 (assinatura BIKi). Toca com a binkw32.dll da propria demo,
    // pelo ajudante de 32 bits F3VBVideo32.exe (que o build.bat compila ao lado do F3VB.exe)
    {
        const std::string h = path_join(A.exe, "F3VBVideo32.exe");
        A.video_helper = file_exists(h) ? h : std::string();
        const bool dll = !A.demo_dir.empty() && file_exists(path_join(A.demo_dir, "binkw32.dll"));
        LOGF("videos: ajudante %s | binkw32.dll da demo %s", A.video_helper.empty() ? "NAO ACHADO (rode o build.bat completo)" : "ok", dll ? "ok" : "NAO ACHADA");
        if (!dll) A.video_helper.clear();
    }
    for (const std::string& p : A.ui.problems()) LOGF("problema: %s", p.c_str());
}

// --check: confere tudo sem abrir o jogo e mostra um resumo (o log tem os detalhes)
int run_check() {
    std::string report;
    int bad = 0;
    report += std::string("Origem: ") + (A.vfs.demo_mode() ? "demo original (.grp)\n" : "cópia extraída\n") + "Pasta: " + A.vfs.root() + "\n";
    report += "Recursos no índice: " + std::to_string(A.vfs.size()) + " em " + std::to_string(A.vfs.packs().size()) + " .grp\n";
    std::vector<std::uint8_t> d;
    std::string err;
    std::size_t read_fail = 0;
    for (const ResEntry& e : A.vfs.entries()) if (!A.vfs.read(e, d, err)) ++read_fail;
    report += "Leitura de todos os recursos: " + std::to_string(read_fail) + " falhas\n";
    bad += read_fail ? 1 : 0;
    load_sys_ini();
    load_fonts_and_texts();
    report += "Fontes TrueType: " + std::to_string(A.ui.font_count()) + " (esperado 3)\n";
    report += std::string("Textos (English.stf): ") + (A.stf.loaded() ? std::to_string(A.stf.count()) + " entradas" : "NÃO ACHADO") + "\n";
    bad += (A.ui.font_count() == 0 || !A.stf.loaded()) ? 1 : 0;
    int screens = 0;
    for (const char* n : {"Mainmenu", "Options", "Options_Game", "Options_Video", "Options_Hotkeys", "Options_Load", "Options_Multiplayer", "CharacterGeneration",
                          "Loading", "Actionbar", "EscapeMenu", "MessageBox", "Dialog", "Inventory", "PipBoy"})
        screens += load_screen(n) ? 1 : 0;
    report += "Telas .int: " + std::to_string(screens) + " de 15\n";
    bad += screens < 15 ? 1 : 0;
    for (const char* m : {"Mainmenu", "00_03_Tutorial_Junktown"}) {
        MapData md;
        MapBuildStats st;
        if (build_map(A.vfs, m, md, err, &st)) report += std::string("Mapa ") + m + ": " + std::to_string(md.triangle_count()) + " triângulos, " + std::to_string(st.nodes_full) + "/" + std::to_string(st.nodes) + " nós\n";
        else { report += std::string("Mapa ") + m + ": FALHOU (" + err + ")\n"; ++bad; }
    }
    std::size_t wavs = A.vfs.list(RT_WAV).size();
    report += "Sons (.wav): " + std::to_string(wavs) + "\n";
    report += bad ? "\nRESULTADO: HÁ PROBLEMAS (veja o log)" : "\nRESULTADO: TUDO CERTO";
    LOGF("--check:\n%s", report.c_str());
    box(report, MB_OK | (bad ? MB_ICONWARNING : MB_ICONINFORMATION));
    return bad ? 1 : 0;
}

void open_log() {
    const std::string dir = path_join(A.exe, "logs");
    CreateDirectoryW(widen(dir).c_str(), nullptr);
    if (!A.log.open(dir, "f3vb_log") && !A.log.open(A.exe, "f3vb_log")) { char tmp[MAX_PATH] = {0}; GetTempPathA(MAX_PATH, tmp); A.log.open(tmp, "f3vb_log"); }
}

bool setup_gl(HINSTANCE inst) {
    A.hdc = GetDC(A.hwnd);
    int fmt = 0;
    const int want = static_cast<int>(setting("MSAA", 8.f));
    A.msaa = want >= 2 ? choose_msaa_pixel_format(inst, A.hdc, want, fmt) : 0;
    PIXELFORMATDESCRIPTOR pfd;
    std::memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd; pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.iLayerType = PFD_MAIN_PLANE;
    if (!A.msaa || !fmt) { fmt = ChoosePixelFormat(A.hdc, &pfd); A.msaa = 0; }
    else DescribePixelFormat(A.hdc, fmt, sizeof pfd, &pfd);
    if (!fmt || !SetPixelFormat(A.hdc, fmt, &pfd)) return false;
    A.rc = wglCreateContext(A.hdc);
    if (!A.rc || !wglMakeCurrent(A.hdc, A.rc)) return false;
    set_vsync(setting("VSync", 1.f) > 0.5f);
    A.aniso = std::min(max_anisotropy(), setting("Anisotropy", 16.f));
    if (A.aniso < 1.f) A.aniso = 1.f;
    const auto gs = [](GLenum e) { const GLubyte* p = glGetString(e); return p ? reinterpret_cast<const char*>(p) : "?"; };
    GLint depth_bits = 0;
    glGetIntegerv(GL_DEPTH_BITS, &depth_bits);
    LOGF("OpenGL: %s | %s | %s | MSAA %dx | anisotropico %.0fx | vsync %s | profundidade %d bits", gs(GL_VENDOR), gs(GL_RENDERER), gs(GL_VERSION), A.msaa, A.aniso,
         setting("VSync", 1.f) > 0.5f ? "sim" : "nao", static_cast<int>(depth_bits));
    gl_init_multitexture();
    return true;
}

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    A.inst = inst;
    A.exe = exe_dir_utf8();
    A.config_path = path_join(A.exe, "F3VB.ini");
    open_log();
    const std::vector<std::string> args = command_line_args();
    std::string argline;
    for (const std::string& a : args) argline += " \"" + a + "\"";
    LOGF("%s | exe em %s | argumentos:%s", kVersion, A.exe.c_str(), argline.empty() ? " (nenhum)" : argline.c_str());
    A.cfg.load(A.config_path);
    bool check = false, skip_menu = false, open_chargen = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--check") check = true;
        else if (args[i] == "--skip-menu") skip_menu = true;
        else if (args[i] == "--chargen") open_chargen = true;
        else if (args[i] == "--autoteste") { A.autotest = true; LOGF("modo autoteste"); }              // teste: abre direto a criacao de personagem
        else if (args[i] == "--map" && i + 1 < args.size()) A.start_map_override = args[++i];
    }
    if (!open_source(args)) {
        LOGF("nenhuma demo escolhida: encerrando");
        return 1;
    }
    LOGF("lendo de: %s (%s) | %zu recursos em %zu .grp", A.vfs.root().c_str(), A.vfs.demo_mode() ? "demo original, .grp inteiros" : "copia extraida", A.vfs.size(), A.vfs.packs().size());
    if (check) return run_check();

    // janela: 80% do monitor, qualquer proporcao (3D ocupa tudo; a interface 4:3 fica centrada)
    WNDCLASSW wc;
    std::memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.lpszClassName = L"F3VBGameWnd";
    wc.hIcon = LoadIconW(wc.hInstance ? wc.hInstance : GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));   // v21: logo do F3VB Engine (res/f3vb.rc)
    if (!RegisterClassW(&wc)) return 1;
    // v20: a janela cabe SEMPRE na area de trabalho do monitor (sem a barra de tarefas) e abre centralizada.
    // No v19/v20-teste a janela salva podia ser maior que a area util e a barra de acoes ficava cortada embaixo.
    RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    RECT frame = {0, 0, 0, 0};
    AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);                 // bordas + titulo (valores negativos/positivos)
    const int work_w = static_cast<int>(work.right - work.left), work_h = static_cast<int>(work.bottom - work.top);
    const int max_cw = work_w - static_cast<int>(frame.right - frame.left);
    const int max_ch = work_h - static_cast<int>(frame.bottom - frame.top);
    A.w = static_cast<int>(setting("WindowWidth", work_w * 0.8f));
    A.h = static_cast<int>(setting("WindowHeight", work_h * 0.8f));
    A.w = std::max(800, std::min(A.w, max_cw));
    A.h = std::max(600, std::min(A.h, max_ch));
    RECT r = {0, 0, A.w, A.h};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    const int ww = static_cast<int>(r.right - r.left), wh = static_cast<int>(r.bottom - r.top);
    const int wx = static_cast<int>(work.left) + std::max(0, (work_w - ww) / 2);
    const int wy = static_cast<int>(work.top) + std::max(0, (work_h - wh) / 2);
    A.hwnd = CreateWindowExW(0, wc.lpszClassName, L"F3VB Engine v21 - Van Buren (demo t\u00e9cnica)", WS_OVERLAPPEDWINDOW, wx, wy, ww, wh,
                             nullptr, nullptr, inst, nullptr);
    if (!A.hwnd) return 1;
    if (!setup_gl(inst)) { box("Não consegui configurar o OpenGL nesta placa de vídeo.", MB_OK | MB_ICONERROR); return 1; }
    A.gl = std::make_unique<GuiGL>(A.ui);
    ShowWindow(A.hwnd, show);
    UpdateWindow(A.hwnd);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); SwapBuffers(A.hdc);
    const DWORD t0 = GetTickCount();
    load_everything();
    LOGF("tudo carregado em %lu ms", static_cast<unsigned long>(GetTickCount() - t0));
    if (!scr("Mainmenu")) {
        box("A tela do menu (Mainmenu.int) não carregou desta pasta:\n" + A.vfs.root() + "\n\nVeja o log em " + path_join(A.exe, "logs") +
            ".\nPara escolher outra pasta, rode  F3VB.exe --escolher-pasta  (ou apague o F3VB.ini).", MB_OK | MB_ICONWARNING);
    }
    if (setting("Fullscreen", 0.f) > 0.5f) toggle_fullscreen();
    enter_mainmenu();
    // abertura como no original: logo da Interplay e da Black Isle (F3VB.ini: StartupMovies=0 desliga; o F3.ini da demo tem
    // "enable startup movies = 1")
    if (open_chargen) { skip_menu = false; enter_mainmenu(); new_character(); enter_chargen(false); }
    else if (!skip_menu && (A.autotest || setting("StartupMovies", 1.f) > 0.5f)) play_videos({"IPlogo.mve", "BISlogo.mve"}, [] { enter_mainmenu(); });
    if (skip_menu) { new_character(); start_loading(A.game_map_name); }
    QueryPerformanceFrequency(&A.freq);
    QueryPerformanceCounter(&A.last);
    MSG msg;
    bool run = true;
    while (run) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { run = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!run) break;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = static_cast<double>(now.QuadPart - A.last.QuadPart) / static_cast<double>(A.freq.QuadPart);
        A.last = now;
        if (dt > 0.25) dt = 0.25;
        A.t += dt; ++A.total_frames; A.heartbeat += dt; A.fps_acc += dt; ++A.frames;
        if (A.fps_acc >= 0.5) { A.fps = A.frames / A.fps_acc; A.fps_acc = 0; A.frames = 0; }
        if (A.heartbeat >= 30.0) {
            // v21: estado completo a cada 30 s: fps, pico do som que saiu, personagens animando/vivos, erros do OpenGL
            A.heartbeat = 0;
            int anim = 0, alive = 0;
            for (const auto& L : (A.mode == Mode::Game ? A.game_actors : A.menu_actors)) { if (L->a.clip_index[AN_IDLE] >= 0) ++anim; if (L->creature && !L->dead) ++alive; }
            const GLenum ge = glGetError();
            LOGF("estado | modo %d | fps %.0f | %s | som: pico %d | atores animados %d, vivos %d | OpenGL %s", static_cast<int>(A.mode), A.fps,
                 A.mode == Mode::Game ? A.game_map.map().name.c_str() : "-", A.audio.take_peak(), anim, alive, ge == GL_NO_ERROR ? "sem erro" : "ERRO");
        }
        update_hover();
        update(dt);
        render();
        if (setting("VSync", 1.f) < 0.5f) Sleep(0);
    }
    if (!A.fullscreen) { RECT wr; GetClientRect(A.hwnd, &wr); if (wr.right > 200 && wr.bottom > 150) { set_setting("WindowWidth", static_cast<float>(wr.right)); set_setting("WindowHeight", static_cast<float>(wr.bottom)); } }
    save_settings();
    A.audio.shutdown();
    A.gl->release();
    A.video.stop();
    A.video.release_gl();
    release_actors(A.menu_actors);
    release_actors(A.game_actors);
    A.menu_map.release();
    A.game_map.release();
    LOGF("sessao encerrada normalmente");
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(A.rc);
    ReleaseDC(A.hwnd, A.hdc);
    return 0;
}
