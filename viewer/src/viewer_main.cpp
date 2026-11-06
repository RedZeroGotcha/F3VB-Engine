// F3VB Viewer: visualizador 3D simples (Win32 + OpenGL 1.x, sem bibliotecas externas, 64 bits).
// Abre modelos .f3m (gerados pelo extrator), permite ADICIONAR animacoes/esqueletos (.f3m) e mostra tudo com textura.
// Estilo igual ao do projeto Futebol3D: janela Win32, contexto WGL, fonte bitmap por wglUseFontBitmaps.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <GL/gl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <chrono>
#include <cstdint>

#include "analyze.h"
#include "log.h"
#include "scene.h"

using namespace f3v;

namespace {

enum Cmd {
    ID_OPEN = 100, ID_ADDANIM, ID_CLEARANIM, ID_QUIT, ID_TEX, ID_WIRE, ID_GRID, ID_BONES, ID_CULL, ID_RESETCAM,
    ID_PLAY, ID_NEXT, ID_PREV, ID_FASTER, ID_SLOWER, ID_BIND, ID_HELP,
    ID_LOGDUMP, ID_SCALE_AUTO, ID_SCALE_HYP, ID_SCALE_DOWN, ID_SCALE_UP, ID_STEP_BACK, ID_STEP_FWD
};

constexpr const char* kVersion = "F3VB Viewer v13";

struct App {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC rc = nullptr;
    int w = 1100, h = 720;
    Scene scene;
    std::vector<GLuint> gl_tex;
    std::vector<std::vector<float>> sp, sn;           // posicoes/normais apos o skinning
    int clip = -1;
    float time = 0.f, speed = 1.f;
    bool playing = true, tex = true, wire = false, grid = true, bones = false, cull = false, help = true;
    float yaw = 35.f, pitch = 20.f, dist = 5.f, cx = 0, cy = 0, cz = 0, radius = 1.f;
    bool ldown = false, rdown = false;
    int mx = 0, my = 0;
    GLuint font_base = 0;
    std::string status = "Arraste um arquivo .f3m para a janela (ou use o menu Arquivo).";
    double fps = 0;
    LARGE_INTEGER freq{}, last{};
    int frames = 0;
    double fps_acc = 0;
    // v12: log, pose de bind alternavel, escala da malha
    Log log;
    bool bind_mode = false;
    int saved_clip = -1;
    double est_scale = 0, uptime = 0, heartbeat = 0;
    std::uint64_t total_frames = 0;
    std::string last_file;
} A;

#define LOGF(...) A.log.linef(__VA_ARGS__)

// ------------------------------------------------------------------------------------------------ texto
void build_font() {
    HFONT f = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
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

// ------------------------------------------------------------------------------------------------ log
std::string exe_dir() {
    char b[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, b, MAX_PATH);
    const std::string s = b;
    const std::size_t p = s.find_last_of("\\/");
    return p == std::string::npos ? std::string(".") : s.substr(0, p);
}

void open_log() {
    const std::string dir = exe_dir() + "\\logs";
    CreateDirectoryA(dir.c_str(), nullptr);                       // ja existir nao e erro
    if (!A.log.open(dir) && !A.log.open(exe_dir())) {
        char tmp[MAX_PATH] = {0};
        GetTempPathA(MAX_PATH, tmp);
        A.log.open(tmp);
    }
}

const char* cmd_name(int id) {
    switch (id) {
        case ID_OPEN: return "abrir";
        case ID_ADDANIM: return "adicionar";
        case ID_CLEARANIM: return "limpar-anim";
        case ID_QUIT: return "sair";
        case ID_TEX: return "texturas(T)";
        case ID_WIRE: return "wireframe(W)";
        case ID_GRID: return "grade(G)";
        case ID_BONES: return "ossos(K)";
        case ID_CULL: return "culling(C)";
        case ID_RESETCAM: return "camera(R)";
        case ID_PLAY: return "tocar(Espaco)";
        case ID_NEXT: return "proxima( ] )";
        case ID_PREV: return "anterior( [ )";
        case ID_FASTER: return "mais-rapido";
        case ID_SLOWER: return "mais-devagar";
        case ID_BIND: return "pose-bind(P)";
        case ID_HELP: return "ajuda(F1)";
        case ID_LOGDUMP: return "analise(F2)";
        case ID_SCALE_AUTO: return "escala-auto(N)";
        case ID_SCALE_HYP: return "escala-1/10.24(M)";
        case ID_SCALE_DOWN: return "escala-menor( , )";
        case ID_SCALE_UP: return "escala-maior( . )";
        case ID_STEP_BACK: return "tempo-atras";
        case ID_STEP_FWD: return "tempo-frente";
        default: return "?";
    }
}

void log_pose() { A.log.block(analyze_pose(A.scene, A.clip, A.time, A.bind_mode)); }

void log_help_block() {
    A.log.block(
        "TECLAS: O abrir | A adicionar animacoes/esqueleto | X limpar animacoes | Espaco tocar/pausar | [ ] animacao anterior/proxima\n"
        "        setas esq/dir tempo | cima/baixo velocidade | P pose de bind liga/desliga | K ossos (lista no log) | T texturas | W wireframe\n"
        "        G grade | C culling | R camera | F1 ajuda | F2 analise completa no log | N escala extra estimada | M escala 1/10.24 liga/desliga\n"
        "        , e . ajuste fino da escala (x0.98 / x1.02) | Esc sair. Mouse: esquerdo gira, direito/meio move, roda zoom.\n");
}

void log_analysis(const char* why) {
    const auto t0 = std::chrono::steady_clock::now();
    LOGF("ANALISE COMPLETA (%s)", why);
    A.log.block(analyze_scene(A.scene, A.last_file));
    double en = 0, eb = 0;
    int ns = 0;
    A.est_scale = estimate_mesh_scale(A.scene, &en, &eb, &ns);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    LOGF("analise concluida em %.0f ms; escala extra estimada f=%.4f (atual x%.5f)", ms, A.est_scale, static_cast<double>(A.scene.mesh_scale));
    if (A.est_scale > 0 && (A.est_scale < 0.85 || A.est_scale > 1.18)) {
        char b[160];
        std::snprintf(b, sizeof b, " | AVISO: malha fora da escala do esqueleto (tecla N aplica x%.4f)", A.est_scale);
        A.status += b;
    }
}

// ------------------------------------------------------------------------------------------------ carga
void free_textures() {
    if (!A.gl_tex.empty()) glDeleteTextures(static_cast<GLsizei>(A.gl_tex.size()), A.gl_tex.data());
    A.gl_tex.clear();
}

void upload_textures() {
    free_textures();
    A.gl_tex.assign(A.scene.textures.size(), 0);
    if (!A.gl_tex.empty()) glGenTextures(static_cast<GLsizei>(A.gl_tex.size()), A.gl_tex.data());
    for (std::size_t i = 0; i < A.scene.textures.size(); ++i) {
        std::vector<std::uint8_t> px;
        std::uint32_t w = 0, h = 0;
        resize_pow2(A.scene.textures[i], px, w, h);
        glBindTexture(GL_TEXTURE_2D, A.gl_tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    }
}

void fit_camera() {
    float mn[3], mx[3];
    if (A.scene.bounds(mn, mx)) {
        A.cx = (mn[0] + mx[0]) / 2; A.cy = (mn[1] + mx[1]) / 2; A.cz = (mn[2] + mx[2]) / 2;
        const float dx = mx[0] - mn[0], dy = mx[1] - mn[1], dz = mx[2] - mn[2];
        A.radius = std::max(0.05f, 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz));
    } else {
        A.cx = A.cy = A.cz = 0; A.radius = 1.f;
    }
    A.dist = A.radius * 2.6f;
}

void set_title() {
    std::string t = kVersion;
    if (!A.scene.model_name.empty()) t += " - " + A.scene.model_name;
    SetWindowTextA(A.hwnd, t.c_str());
}

bool load_path(const std::string& path) {
    F3MFile f;
    std::string err;
    LOGF("ABRIR: %s", path.c_str());
    const auto t0 = std::chrono::steady_clock::now();
    if (!load_f3m_file(path, f, err)) { A.status = "ERRO: " + err + " (" + path + ")"; LOGF("ERRO ao abrir: %s", err.c_str()); return false; }
    LOGF("arquivo lido: flags=%u nome=\"%s\" texturas=%zu materiais=%zu malhas=%zu juntas=%zu animacoes=%zu", f.flags, f.name.c_str(), f.textures.size(), f.materials.size(), f.meshes.size(), f.joints.size(), f.clips.size());
    LoadReport r = A.scene.add(std::move(f));
    LOGF("cena: %s (modelo substituido=%d, esqueleto adotado=%d, esqueleto substituido=%d, animacoes adicionadas=%d, canais sem osso=%d) em %.0f ms", r.message.c_str(), r.replaced_model, r.adopted_skeleton, r.replaced_skeleton, r.clips_added, r.channels_unmatched,
         std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    if (r.replaced_model) { A.scene.mesh_scale = 1.f; upload_textures(); fit_camera(); set_title(); A.last_file = path; }
    A.sp.assign(A.scene.meshes.size(), {});
    A.sn.assign(A.scene.meshes.size(), {});
    if (A.clip < 0 && !A.scene.clips.empty() && !A.bind_mode) { A.clip = 0; A.time = 0; }
    if (A.clip >= static_cast<int>(A.scene.clips.size())) A.clip = A.scene.clips.empty() ? -1 : 0;
    const std::size_t slash = path.find_last_of("\\/");
    A.status = r.message + " [" + (slash == std::string::npos ? path : path.substr(slash + 1)) + "]";
    return true;
}

void load_paths(const std::vector<std::string>& files) {
    bool any = false;
    for (const std::string& p : files) any = load_path(p) || any;
    if (any) log_analysis("apos carregar arquivo(s)");
}

std::string open_dialog(bool multi, std::vector<std::string>& out) {
    char buf[8192] = {0};
    OPENFILENAMEA ofn;
    std::memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = A.hwnd;
    ofn.lpstrFilter = "Modelos/animacoes F3M (*.f3m)\0*.f3m\0Todos os arquivos\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof buf;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR | (multi ? OFN_ALLOWMULTISELECT : 0);
    if (!GetOpenFileNameA(&ofn)) return "";
    // varios arquivos: "pasta\0arq1\0arq2\0\0"; um so: caminho completo
    std::string first = buf;
    const char* p = buf + first.size() + 1;
    if (*p == 0) { out.push_back(first); return first; }
    while (*p) { out.push_back(first + "\\" + p); p += std::strlen(p) + 1; }
    return first;
}

void cmd_open() { std::vector<std::string> v; open_dialog(false, v); load_paths(v); }
void cmd_add() { std::vector<std::string> v; open_dialog(true, v); load_paths(v); }

void select_clip(int delta) {
    const int n = static_cast<int>(A.scene.clips.size());
    if (n == 0) return;
    if (A.bind_mode) { A.bind_mode = false; A.clip = A.saved_clip >= 0 && A.saved_clip < n ? A.saved_clip : 0; }
    A.clip = (A.clip + delta + n) % n;
    A.time = 0;
    A.status = "Animacao: " + A.scene.clips[static_cast<std::size_t>(A.clip)].name;
}

// ------------------------------------------------------------------------------------------------ desenho
void draw_grid() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    const float step = std::pow(10.f, std::floor(std::log10(std::max(0.01f, A.radius))));
    const int n = 10;
    glBegin(GL_LINES);
    for (int i = -n; i <= n; ++i) {
        glColor4f(0.35f, 0.35f, 0.4f, 1.f);
        if (i == 0) glColor4f(0.6f, 0.6f, 0.65f, 1.f);
        glVertex3f(i * step, 0, -n * step); glVertex3f(i * step, 0, n * step);
        glVertex3f(-n * step, 0, i * step); glVertex3f(n * step, 0, i * step);
    }
    glEnd();
    glBegin(GL_LINES);                                   // eixos: X vermelho, Y verde, Z azul
    glColor4f(1, 0.2f, 0.2f, 1); glVertex3f(0, 0, 0); glVertex3f(step * 2, 0, 0);
    glColor4f(0.2f, 1, 0.2f, 1); glVertex3f(0, 0, 0); glVertex3f(0, step * 2, 0);
    glColor4f(0.3f, 0.5f, 1, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, step * 2);
    glEnd();
}

void draw_bones() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glColor4f(1.f, 0.85f, 0.2f, 1.f);
    glBegin(GL_LINES);
    for (std::size_t j = 0; j < A.scene.joints.size(); ++j) {
        const int p = A.scene.joints[j].parent;
        if (p < 0 || p >= static_cast<int>(A.scene.joint_world.size())) continue;
        const Mat4& a = A.scene.joint_world[static_cast<std::size_t>(p)];
        const Mat4& b = A.scene.joint_world[j];
        glVertex3f(a.m[12], a.m[13], a.m[14]);
        glVertex3f(b.m[12], b.m[13], b.m[14]);
    }
    glEnd();
    glPointSize(5.f);
    glBegin(GL_POINTS);
    for (const Mat4& m : A.scene.joint_world) glVertex3f(m.m[12], m.m[13], m.m[14]);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

void draw_meshes() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glEnable(GL_NORMALIZE);
    glPolygonMode(GL_FRONT_AND_BACK, A.wire ? GL_LINE : GL_FILL);
    if (A.cull) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW); } else glDisable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t i = 0; i < A.scene.meshes.size(); ++i) {
            const Mesh& m = A.scene.meshes[i];
            const Material* mat = (m.material >= 0 && m.material < static_cast<int>(A.scene.materials.size())) ? &A.scene.materials[static_cast<std::size_t>(m.material)] : nullptr;
            const int mode = mat ? mat->alpha_mode : 0;
            if ((mode != 0) != (pass == 1)) continue;
            if (mat) glColor4f(mat->color[0], mat->color[1], mat->color[2], mode == 0 ? 1.f : mat->color[3]);
            else glColor4f(0.8f, 0.8f, 0.85f, 1.f);
            const bool use_tex = A.tex && mat && mat->texture >= 0 && mat->texture < static_cast<int>(A.gl_tex.size());
            if (use_tex) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, A.gl_tex[static_cast<std::size_t>(mat->texture)]); glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); }
            else glDisable(GL_TEXTURE_2D);
            if (mode == 0) { glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDepthMask(GL_TRUE); }
            else {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, mode == 2 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.02f);
                glDepthMask(GL_FALSE);
            }
            const std::vector<float>& P = (i < A.sp.size() && !A.sp[i].empty()) ? A.sp[i] : m.pos;
            const std::vector<float>& N = (i < A.sn.size() && !A.sn[i].empty()) ? A.sn[i] : m.nrm;
            glVertexPointer(3, GL_FLOAT, 0, P.data());
            glNormalPointer(GL_FLOAT, 0, N.data());
            glTexCoordPointer(2, GL_FLOAT, 0, m.uv.data());
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m.idx.size()), GL_UNSIGNED_INT, m.idx.data());
        }
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
}

void draw_hud() {
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, A.w, 0, A.h, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glColor4f(1.f, 1.f, 1.f, 1.f);
    float y = static_cast<float>(A.h) - 18.f;
    char b[256];
    std::size_t nv = 0, nt = 0;
    for (const Mesh& m : A.scene.meshes) { nv += m.nv(); nt += m.idx.size() / 3; }
    std::snprintf(b, sizeof b, "%s | %zu malhas, %zu vertices, %zu triangulos, %zu ossos, %.0f FPS",
                  A.scene.model_name.empty() ? "(sem modelo)" : A.scene.model_name.c_str(), A.scene.meshes.size(), nv, nt, A.scene.joints.size(), A.fps);
    text(8, y, b); y -= 16;
    if (A.clip >= 0 && A.clip < static_cast<int>(A.scene.clips.size())) {
        const Clip& c = A.scene.clips[static_cast<std::size_t>(A.clip)];
        std::snprintf(b, sizeof b, "Animacao %d/%zu: %s  %.2f/%.2f s  vel %.2fx  %s", A.clip + 1, A.scene.clips.size(), c.name.c_str(), A.time, c.duration, A.speed, A.playing ? "tocando" : "pausada");
    } else std::snprintf(b, sizeof b, "Sem animacao (pose de bind). %zu animacoes carregadas.", A.scene.clips.size());
    if (A.bind_mode) std::snprintf(b, sizeof b, "POSE DE BIND (P volta para a animacao). %zu animacoes carregadas.", A.scene.clips.size());
    text(8, y, b); y -= 16;
    std::snprintf(b, sizeof b, "escala extra da malha x%.5f%s | log: %s", static_cast<double>(A.scene.mesh_scale), A.est_scale > 0 && (A.est_scale < 0.85 || A.est_scale > 1.18) ? "  (AVISO: fora da escala do esqueleto, tecla N)" : "",
                  A.log.is_open() ? A.log.path().c_str() : "(sem log)");
    text(8, y, b); y -= 16;
    text(8, y, A.status); y -= 16;
    if (A.help) {
        const char* lines[] = {"O abrir modelo | A adicionar animacoes/esqueleto | X limpar animacoes | arraste arquivos .f3m para a janela",
                               "Espaco tocar/pausar | [ ] animacao anterior/proxima | setas esq/dir: tempo | cima/baixo: velocidade | P pose de bind liga/desliga",
                               "T texturas | W wireframe | G grade | K ossos | C culling | R camera | F1 esconde esta ajuda | F2 analise completa no log",
                               "Escala da malha: N estimada | M 1/10.24 | , . ajuste fino   (tudo o que voce faz e a analise ficam no log)",
                               "Mouse: esquerdo gira, direito/meio move, roda zoom"};
        for (const char* l : lines) { y -= 2; text(8, y, l); y -= 16; }
    }
    glEnable(GL_DEPTH_TEST);
}

void render() {
    glViewport(0, 0, A.w, A.h);
    glClearColor(0.11f, 0.12f, 0.15f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    const float aspect = A.h > 0 ? static_cast<float>(A.w) / static_cast<float>(A.h) : 1.f;
    const float zn = std::max(0.001f, A.dist * 0.02f), zf = std::max(100.f, A.dist * 200.f);
    const float top = zn * std::tan(0.5f * 45.f * 3.14159265f / 180.f);
    glFrustum(-top * aspect, top * aspect, -top, top, zn, zf);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glTranslatef(0, 0, -A.dist);
    glRotatef(A.pitch, 1, 0, 0);
    glRotatef(A.yaw, 0, 1, 0);
    glTranslatef(-A.cx, -A.cy, -A.cz);
    const GLfloat lpos[4] = {0.4f, 0.9f, 0.6f, 0.f}, amb[4] = {0.35f, 0.35f, 0.38f, 1.f}, dif[4] = {0.9f, 0.9f, 0.85f, 1.f};
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    if (A.grid) draw_grid();
    draw_meshes();
    if (A.bones) draw_bones();
    draw_hud();
    SwapBuffers(A.hdc);
}

void update(double dt) {
    if (!A.scene.joints.empty()) {
        if (A.clip >= 0 && A.clip < static_cast<int>(A.scene.clips.size())) {
            const float dur = A.scene.clips[static_cast<std::size_t>(A.clip)].duration;
            if (A.playing && dur > 0) { A.time += static_cast<float>(dt) * A.speed; A.time = std::fmod(A.time, dur); if (A.time < 0) A.time += dur; }
        }
        A.scene.evaluate(A.clip, A.time);
        A.sp.resize(A.scene.meshes.size());
        A.sn.resize(A.scene.meshes.size());
    }
    A.sp.resize(A.scene.meshes.size());
    A.sn.resize(A.scene.meshes.size());
    const bool scaled = A.scene.mesh_scale != 1.f;
    for (std::size_t i = 0; i < A.scene.meshes.size(); ++i) {
        if (A.scene.meshes[i].has_skin || scaled) A.scene.skin(static_cast<int>(i), A.sp[i], A.sn[i]);
        else { A.sp[i].clear(); A.sn[i].clear(); }
    }
}

void toggle(bool& v, const char* name) { v = !v; A.status = std::string(name) + (v ? ": ligado" : ": desligado"); }

void set_mesh_scale(float v) {
    A.scene.mesh_scale = std::max(1e-4f, std::min(1e4f, v));
    char b[160];
    std::snprintf(b, sizeof b, "Escala extra da malha: x%.5f", static_cast<double>(A.scene.mesh_scale));
    A.status = b;
}

void on_command(int id) {
    switch (id) {
        case ID_OPEN: cmd_open(); break;
        case ID_ADDANIM: cmd_add(); break;
        case ID_CLEARANIM: A.scene.clear_clips(); A.clip = -1; A.time = 0; A.bind_mode = false; A.status = "Animacoes removidas"; break;
        case ID_QUIT: LOGF("comando sair"); DestroyWindow(A.hwnd); return;
        case ID_TEX: toggle(A.tex, "Texturas"); break;
        case ID_WIRE: toggle(A.wire, "Wireframe"); break;
        case ID_GRID: toggle(A.grid, "Grade"); break;
        case ID_BONES: toggle(A.bones, "Ossos"); break;
        case ID_CULL: toggle(A.cull, "Culling de faces traseiras"); break;
        case ID_RESETCAM: A.yaw = 35.f; A.pitch = 20.f; fit_camera(); A.status = "Camera resetada"; break;
        case ID_PLAY: A.playing = !A.playing; A.status = A.playing ? "Animacao tocando" : "Animacao pausada"; break;
        case ID_NEXT: select_clip(+1); break;
        case ID_PREV: select_clip(-1); break;
        case ID_FASTER: A.speed = std::min(8.f, A.speed * 1.25f); A.status = "Velocidade " + std::to_string(A.speed).substr(0, 4) + "x"; break;
        case ID_SLOWER: A.speed = std::max(0.05f, A.speed / 1.25f); A.status = "Velocidade " + std::to_string(A.speed).substr(0, 4) + "x"; break;
        case ID_STEP_BACK: A.playing = false; A.time = std::max(0.f, A.time - 1.f / 30.f); A.status = "Tempo " + std::to_string(A.time).substr(0, 5) + " s (pausado)"; break;
        case ID_STEP_FWD: A.playing = false; A.time += 1.f / 30.f; A.status = "Tempo " + std::to_string(A.time).substr(0, 5) + " s (pausado)"; break;
        case ID_BIND:
            if (!A.bind_mode) {
                A.bind_mode = true;
                A.saved_clip = A.clip;
                A.clip = -1;
                A.status = "Pose de bind LIGADA (P volta para a animacao)";
            } else {
                A.bind_mode = false;
                A.clip = A.saved_clip >= 0 && A.saved_clip < static_cast<int>(A.scene.clips.size()) ? A.saved_clip : (A.scene.clips.empty() ? -1 : 0);
                A.status = "Pose de bind DESLIGADA (animacao " + std::to_string(A.clip + 1) + ")";
            }
            break;
        case ID_HELP: A.help = !A.help; A.status = A.help ? "Ajuda ligada" : "Ajuda desligada"; break;
        case ID_LOGDUMP: A.status = "Analise completa gravada no log"; break;
        case ID_SCALE_AUTO: {
            double en = 0, eb = 0;
            int ns = 0;
            const double f = estimate_mesh_scale(A.scene, &en, &eb, &ns);
            if (f <= 0) A.status = "Escala: sem dados suficientes para estimar (precisa de malha com pele e esqueleto)";
            else if (f > 0.98 && f < 1.02) { char b[120]; std::snprintf(b, sizeof b, "Escala ja correta (f estimado %.4f)", f); A.status = b; }
            else { set_mesh_scale(A.scene.mesh_scale * static_cast<float>(f)); fit_camera(); }
            LOGF("escala automatica: f estimado=%.4f (erro %.4f -> %.4f, %d vertices)", f, en, eb, ns);
            break;
        }
        case ID_SCALE_HYP:
            set_mesh_scale(std::fabs(A.scene.mesh_scale - 1.f) < 1e-6f ? static_cast<float>(kMeshScaleHypothesis) : 1.f);
            fit_camera();
            break;
        case ID_SCALE_DOWN: set_mesh_scale(A.scene.mesh_scale / 1.02f); break;
        case ID_SCALE_UP: set_mesh_scale(A.scene.mesh_scale * 1.02f); break;
        default: break;
    }
    LOGF("comando %-18s | %s", cmd_name(id), A.status.c_str());
    switch (id) {
        case ID_BIND: case ID_NEXT: case ID_PREV: case ID_CLEARANIM:
        case ID_SCALE_AUTO: case ID_SCALE_HYP: case ID_SCALE_DOWN: case ID_SCALE_UP:
            if (!A.scene.meshes.empty()) log_pose();
            break;
        case ID_BONES:
            if (A.bones) { log_pose(); A.log.block(dump_bones(A.scene, A.clip, A.time)); }
            break;
        case ID_LOGDUMP:
            log_analysis("pedido pelo usuario (F2)");
            log_pose();
            A.log.block(dump_bones(A.scene, A.clip, A.time));
            break;
        default: break;
    }
}

HMENU make_menu() {
    HMENU bar = CreateMenu(), file = CreatePopupMenu(), view = CreatePopupMenu(), anim = CreatePopupMenu();
    AppendMenuA(file, MF_STRING, ID_OPEN, "Abrir modelo...\tO");
    AppendMenuA(file, MF_STRING, ID_ADDANIM, "Adicionar animacoes / esqueleto...\tA");
    AppendMenuA(file, MF_STRING, ID_CLEARANIM, "Limpar animacoes\tX");
    AppendMenuA(file, MF_STRING, ID_LOGDUMP, "Gravar analise completa no log\tF2");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, ID_QUIT, "Sair\tEsc");
    AppendMenuA(view, MF_STRING, ID_TEX, "Texturas\tT");
    AppendMenuA(view, MF_STRING, ID_WIRE, "Wireframe\tW");
    AppendMenuA(view, MF_STRING, ID_GRID, "Grade\tG");
    AppendMenuA(view, MF_STRING, ID_BONES, "Ossos\tK");
    AppendMenuA(view, MF_STRING, ID_CULL, "Culling de faces traseiras\tC");
    AppendMenuA(view, MF_STRING, ID_RESETCAM, "Resetar camera\tR");
    AppendMenuA(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(view, MF_STRING, ID_SCALE_AUTO, "Escala da malha: aplicar estimada\tN");
    AppendMenuA(view, MF_STRING, ID_SCALE_HYP, "Escala da malha: alternar 1/10.24\tM");
    AppendMenuA(view, MF_STRING, ID_SCALE_DOWN, "Escala da malha: menor (x0.98)\t,");
    AppendMenuA(view, MF_STRING, ID_SCALE_UP, "Escala da malha: maior (x1.02)\t.");
    AppendMenuA(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(view, MF_STRING, ID_HELP, "Mostrar/ocultar ajuda\tF1");
    AppendMenuA(anim, MF_STRING, ID_PLAY, "Tocar / pausar\tEspaco");
    AppendMenuA(anim, MF_STRING, ID_PREV, "Animacao anterior\t[");
    AppendMenuA(anim, MF_STRING, ID_NEXT, "Proxima animacao\t]");
    AppendMenuA(anim, MF_STRING, ID_FASTER, "Mais rapido\tSeta cima");
    AppendMenuA(anim, MF_STRING, ID_SLOWER, "Mais devagar\tSeta baixo");
    AppendMenuA(anim, MF_STRING, ID_BIND, "Pose de bind (liga/desliga)\tP");
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), "Arquivo");
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(view), "Exibir");
    AppendMenuA(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(anim), "Animacao");
    return bar;
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: A.w = std::max(1, static_cast<int>(LOWORD(lp))); A.h = std::max(1, static_cast<int>(HIWORD(lp))); return 0;
        case WM_COMMAND: on_command(static_cast<int>(LOWORD(wp))); return 0;
        case WM_DROPFILES: {
            HDROP d = reinterpret_cast<HDROP>(wp);
            const UINT n = DragQueryFileA(d, 0xFFFFFFFFu, nullptr, 0);
            std::vector<std::string> files;
            for (UINT i = 0; i < n; ++i) { char buf[MAX_PATH] = {0}; DragQueryFileA(d, i, buf, MAX_PATH); files.push_back(buf); }
            DragFinish(d);
            std::sort(files.begin(), files.end());
            load_paths(files);
            return 0;
        }
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
            A.dist = std::max(A.radius * 0.1f, std::min(A.radius * 60.f, A.dist * std::pow(0.88f, d)));
            return 0;
        }
        case WM_KEYDOWN: {
            switch (wp) {
                case VK_ESCAPE: DestroyWindow(hwnd); break;
                case VK_SPACE: on_command(ID_PLAY); break;
                case VK_OEM_4: on_command(ID_PREV); break;
                case VK_OEM_6: on_command(ID_NEXT); break;
                case VK_UP: on_command(ID_FASTER); break;
                case VK_DOWN: on_command(ID_SLOWER); break;
                case VK_LEFT: on_command(ID_STEP_BACK); break;
                case VK_RIGHT: on_command(ID_STEP_FWD); break;
                case VK_F1: on_command(ID_HELP); break;
                case VK_F2: on_command(ID_LOGDUMP); break;
                case 'N': on_command(ID_SCALE_AUTO); break;
                case 'M': on_command(ID_SCALE_HYP); break;
                case VK_OEM_COMMA: on_command(ID_SCALE_DOWN); break;
                case VK_OEM_PERIOD: on_command(ID_SCALE_UP); break;
                case 'O': on_command(ID_OPEN); break;
                case 'A': on_command(ID_ADDANIM); break;
                case 'X': on_command(ID_CLEARANIM); break;
                case 'T': on_command(ID_TEX); break;
                case 'W': on_command(ID_WIRE); break;
                case 'G': on_command(ID_GRID); break;
                case 'K': on_command(ID_BONES); break;
                case 'C': on_command(ID_CULL); break;
                case 'R': on_command(ID_RESETCAM); break;
                case 'P': on_command(ID_BIND); break;
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

std::vector<std::string> split_args(const std::string& cmd) {
    std::vector<std::string> out;
    std::string cur;
    bool q = false, any = false;
    for (char c : cmd) {
        if (c == '"') { q = !q; any = true; }
        else if (c == ' ' && !q) { if (any || !cur.empty()) out.push_back(cur); cur.clear(); any = false; }
        else cur.push_back(c);
    }
    if (any || !cur.empty()) out.push_back(cur);
    return out;
}

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmdline, int show) {
    open_log();
    LOGF("%s (compilado em %s %s)", kVersion, __DATE__, __TIME__);
    LOGF("executavel: %s\\F3VBViewer.exe | linha de comando: %s", exe_dir().c_str(), cmdline && *cmdline ? cmdline : "(vazia)");
    LOGF("arquivo de log: %s", A.log.is_open() ? A.log.path().c_str() : "(NAO foi possivel criar o log)");
    log_help_block();
    WNDCLASSA wc;
    std::memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = "F3VBViewerWnd";
    if (!RegisterClassA(&wc)) return 1;
    RECT r = {0, 0, A.w, A.h};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, TRUE);
    A.hwnd = CreateWindowExA(WS_EX_ACCEPTFILES, wc.lpszClassName, "F3VB Viewer", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top, nullptr, make_menu(), inst, nullptr);
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
    if (!fmt || !SetPixelFormat(A.hdc, fmt, &pfd)) { MessageBoxA(nullptr, "Nao consegui configurar o OpenGL.", "F3VB Viewer", MB_OK | MB_ICONERROR); return 1; }
    A.rc = wglCreateContext(A.hdc);
    if (!A.rc || !wglMakeCurrent(A.hdc, A.rc)) { MessageBoxA(nullptr, "Nao consegui criar o contexto OpenGL.", "F3VB Viewer", MB_OK | MB_ICONERROR); return 1; }
    build_font();
    {
        const auto gs = [](GLenum e) { const GLubyte* p = glGetString(e); return p ? reinterpret_cast<const char*>(p) : "?"; };
        LOGF("OpenGL: fornecedor=%s | renderizador=%s | versao=%s | janela %dx%d", gs(GL_VENDOR), gs(GL_RENDERER), gs(GL_VERSION), A.w, A.h);
    }
    DragAcceptFiles(A.hwnd, TRUE);
    ShowWindow(A.hwnd, show);
    UpdateWindow(A.hwnd);
    load_paths(split_args(cmdline ? cmdline : ""));
    if (A.log.is_open() && A.scene.meshes.empty()) A.status = "Log desta sessao: " + A.log.path();

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
        A.uptime += dt; ++A.total_frames;
        A.heartbeat += dt;
        if (A.heartbeat >= 30.0) {
            A.heartbeat = 0;
            LOGF("estado | modelo=%s malhas=%zu juntas=%zu animacao=%d t=%.2f %s bind=%d ossos=%d escala=%.5f fps=%.0f", A.scene.model_name.empty() ? "(nenhum)" : A.scene.model_name.c_str(), A.scene.meshes.size(), A.scene.joints.size(), A.clip, A.time,
                 A.playing ? "tocando" : "pausada", A.bind_mode, A.bones, static_cast<double>(A.scene.mesh_scale), A.fps);
        }
        A.fps_acc += dt; ++A.frames;
        if (A.fps_acc >= 0.5) { A.fps = A.frames / A.fps_acc; A.fps_acc = 0; A.frames = 0; }
        update(std::min(dt, 0.1));
        render();
        Sleep(1);
    }
    LOGF("sessao encerrada normalmente");
    free_textures();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(A.rc);
    ReleaseDC(A.hwnd, A.hdc);
    return 0;
}
