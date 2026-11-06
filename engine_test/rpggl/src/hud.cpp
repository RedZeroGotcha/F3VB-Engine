// =====================================================================
//  hud.cpp - interface 2D: HUD de jogo, compasso, menus, Diário, mapa,
//  dialogos, comercio, telas de save/load com miniatura.
//
//  Texto: fontes do proprio Windows via wglUseFontBitmaps (listas de
//  exibicao) -> nenhuma biblioteca externa.  O codigo-fonte e UTF-8;
//  na hora de desenhar converte para Latin-1 (CP1252) para que acentos
//  funcionem.  Coordenadas 2D: origem no canto superior esquerdo (px).
// =====================================================================
#include "gfx.h"
#include <ctime>

bool g_showHelp = false;

enum { F_SMALL, F_MED, F_BIG, F_HUGE, F_COUNT };
static GLuint fBase[F_COUNT]; static int fW[F_COUNT][256]; static int fPx[F_COUNT]; static int fReady = 0; static float uiScale = 1.0f;
static GLuint g_thumbTex = 0; static std::string g_thumbFor;

void ShutdownHUD() { if (fReady) for (int i = 0; i < F_COUNT; i++) glDeleteLists(fBase[i], 224); fReady = 0; }
void InitHUD(HDC hdc, int Hpx) {
    ShutdownHUD(); uiScale = Clampf((float)Hpx/900.0f, 0.7f, 2.2f);
    const int px[F_COUNT] = { 15, 20, 34, 80 };
    for (int i = 0; i < F_COUNT; i++) {
        fPx[i] = Maxi(10, (int)((float)px[i]*uiScale));
        HFONT f = CreateFontA(-fPx[i], 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
        HGDIOBJ old = SelectObject(hdc, f); fBase[i] = glGenLists(224); wglUseFontBitmapsA(hdc, 32, 224, fBase[i]);
        int w[256]; memset(w, 0, sizeof(w)); GetCharWidth32A(hdc, 0, 255, w);
        for (int c = 0; c < 256; c++) fW[i][c] = w[c] > 0 ? w[c] : fPx[i]/2;
        SelectObject(hdc, old); DeleteObject(f);
    }
    fReady = 1;
}

static std::string L1(const char* s) {                                         // UTF-8 -> Latin-1
    std::string o;
    for (const unsigned char* c = (const unsigned char*)s; *c; c++) {
        if (*c < 0x80) o += (char)*c;
        else if ((*c & 0xE0) == 0xC0 && c[1]) { unsigned cp = ((unsigned)(*c & 0x1F) << 6) | (unsigned)(c[1] & 0x3F); c++; o += cp < 256 ? (char)cp : '?'; }
        else if ((*c & 0xF0) == 0xE0 && c[1] && c[2]) { c += 2; o += '-'; }
        else o += '?';
    }
    return o;
}
static int TextWidth(int font, const std::string& l1) { int w = 0; for (size_t i = 0; i < l1.size(); i++) w += fW[font][(unsigned char)l1[i]]; return w; }

// ---- primitivas ----
static float S(float v) { return v*uiScale; }
static const float PGR = 0.95f, PGG = 0.74f, PGB = 0.42f;                      // tom areia/ferrugem do Diario

static void Text(int font, float x, float y, const char* utf8, float r, float g, float b, float a = 1.0f, int align = 0) {
    if (!fReady) return; std::string s = L1(utf8); int w = TextWidth(font, s);
    if (align == 1) x -= (float)w*0.5f; else if (align == 2) x -= (float)w;
    float base = y + (float)fPx[font]*0.82f; glListBase(fBase[font] - 32);
    glColor4f(0, 0, 0, a*0.85f); glRasterPos2f(x + 1.5f, base + 1.5f); glCallLists((GLsizei)s.size(), GL_UNSIGNED_BYTE, s.c_str());
    glColor4f(r, g, b, a); glRasterPos2f(x, base); glCallLists((GLsizei)s.size(), GL_UNSIGNED_BYTE, s.c_str());
}
static void TextS(int font, float x, float y, const std::string& s, float r, float g, float b, float a = 1.0f, int align = 0) { Text(font, x, y, s.c_str(), r, g, b, a, align); }
static float TW(int font, const char* s) { return (float)TextWidth(font, L1(s)); }
static void Rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a); glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
static void RectLine(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a); glBegin(GL_LINE_LOOP); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
static void Bar(float x, float y, float w, float h, float v, float r, float g, float b) {
    Rect(x, y, w, h, 0, 0, 0, 0.6f); Rect(x + 2, y + 2, (w - 4)*Sat(v), h - 4, r, g, b, 0.95f); RectLine(x, y, w, h, 1, 1, 1, 0.55f);
}
static void Diamond(float x, float y, float r, float cr, float cg, float cb, float a) {
    glColor4f(cr, cg, cb, a); glBegin(GL_QUADS); glVertex2f(x, y - r); glVertex2f(x + r, y); glVertex2f(x, y + r); glVertex2f(x - r, y); glEnd();
    glColor4f(0, 0, 0, a); glBegin(GL_LINE_LOOP); glVertex2f(x, y - r); glVertex2f(x + r, y); glVertex2f(x, y + r); glVertex2f(x - r, y); glEnd();
}
static void Begin2D(int sw, int sh) {
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_CULL_FACE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, sw, sh, 0, -1, 1); glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glLineWidth(1.5f);
}
static void End2D() { glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST); }

static const char* K(const char* kb, const char* pad) { return G.in.usingPad ? pad : kb; }

static void WrapText(int font, float x, float y, float maxW, const char* utf8, float r, float g, float b, float lineH, int maxLines = 99) {
    std::string s = utf8, line; size_t i = 0; int lines = 0;
    while (i < s.size() && lines < maxLines) {
        size_t j = s.find(' ', i); std::string word = s.substr(i, j == std::string::npos ? std::string::npos : j - i); i = (j == std::string::npos) ? s.size() : j + 1;
        std::string test = line.empty() ? word : line + " " + word;
        if (TW(font, test.c_str()) > maxW && !line.empty()) { Text(font, x, y, line.c_str(), r, g, b); y += lineH; lines++; line = word; } else line = test;
    }
    if (!line.empty() && lines < maxLines) Text(font, x, y, line.c_str(), r, g, b);
}

// ---------------------------------------------------------------------
//  HUD DE JOGO
// ---------------------------------------------------------------------
static void DrawCompass(int sw) {
    const Player& p = G.p; float cx = (float)sw*0.5f, y = S(14), w = S(520), h = S(26), fov = 1.3f;
    Rect(cx - w*0.5f, y, w, h, 0, 0, 0, 0.35f); RectLine(cx - w*0.5f, y, w, h, PGR, PGG, PGB, 0.5f);
    for (int d = 0; d < 360; d += 15) {                                                      // d = direcao em graus (0 N, anti-horario)
        float ang = WrapAngle((float)d*PI/180.0f - p.yaw); if (fabsf(ang) > fov) continue;
        float x = cx - ang/fov*(w*0.5f); bool major = (d % 90) == 0;
        glColor4f(PGR, PGG, PGB, major ? 0.9f : 0.5f); glBegin(GL_LINES); glVertex2f(x, y + h); glVertex2f(x, y + h - (major ? S(10) : S(5))); glEnd();
        if (major) { const char* L = d == 0 ? "N" : (d == 90 ? "O" : (d == 180 ? "S" : "L")); Text(F_SMALL, x, y + S(2), L, 1, 1, 1, 1, 1); }
    }
    for (int q = 0; q < Q_COUNT; q++) {
        Vec3 m; if (!QuestMarker(q, m)) continue; float ang = WrapAngle(YawOf(m.x - p.pos.x, m.z - p.pos.z) - p.yaw); float dist = Len2D(m - p.pos);
        float xx = cx - Clampf(ang, -fov, fov)/fov*(w*0.5f); bool edge = fabsf(ang) > fov;
        Diamond(xx, y + h*0.5f, S(7), 0.3f, 0.8f, 1.0f, edge ? 0.5f : 1.0f);
        if (!edge) { char b[32]; snprintf(b, sizeof(b), "%dm", (int)dist); Text(F_SMALL, xx, y + h + S(2), b, 0.5f, 0.9f, 1.0f, 1, 1); }
    }
    for (std::unordered_set<u64>::const_iterator it = G.discovered.begin(); it != G.discovered.end(); ++it) {
        int pcx = (int)(*it >> 32), pcz = (int)(*it & 0xFFFFFFFFu); float wx = (float)(pcx*CHUNK_SIZE + 32), wz = (float)(pcz*CHUNK_SIZE + 32);
        float ang = WrapAngle(YawOf(wx - p.pos.x, wz - p.pos.z) - p.yaw); if (fabsf(ang) > fov || Len2D(Vec3(wx - p.pos.x, 0, wz - p.pos.z)) > 700.0f) continue;
        Diamond(cx - ang/fov*(w*0.5f), y + h*0.5f, S(4), 0.9f, 0.85f, 0.3f, 0.8f);
    }
    glColor4f(1, 1, 1, 0.9f); glBegin(GL_TRIANGLES); glVertex2f(cx, y + h + S(1)); glVertex2f(cx - S(5), y + h + S(8)); glVertex2f(cx + S(5), y + h + S(8)); glEnd();
}

static void DrawVitals(int, int sh) {
    const Player& p = G.p; float x = S(24), w = S(280), h = S(20), y = (float)sh - S(150);
    Rect(x - S(10), y - S(12), w + S(20), S(138), 0, 0, 0, 0.35f);
    float maxHp = EffMaxHp(); Bar(x, y, w, h, p.hp/Maxf(maxHp, 1.0f), 0.85f, 0.15f, 0.15f);
    if (maxHp < p.maxhp - 0.5f) { float lim = maxHp/p.maxhp; Rect(x + 2 + (w - 4)*lim, y + 2, (w - 4)*(1 - lim), h - 4, 0.3f, 0.5f, 0.1f, 0.8f); }
    char b[64]; snprintf(b, sizeof(b), "VIDA  %d / %d", (int)p.hp, (int)p.maxhp); Text(F_SMALL, x + S(6), y + S(1), b, 1, 1, 1);
    float maxSt = 80.0f + 4.0f*(float)p.attr[AT_FOL]; Bar(x, y + S(26), w, S(14), p.stam/maxSt, 0.9f, 0.75f, 0.2f); Text(F_SMALL, x + S(6), y + S(25), "ESTAMINA", 1, 1, 1);
    Bar(x, y + S(46), w, S(10), p.rad/1000.0f, 0.4f, 0.9f, 0.2f); if (p.rad > 1) { snprintf(b, sizeof(b), "RAD %d", (int)p.rad); Text(F_SMALL, x + w + S(6), y + S(41), b, 0.5f, 1.0f, 0.3f); }
    int need = 80 + 45*p.level; Bar(x, y + S(62), w, S(8), (float)p.xp/(float)need, 0.3f, 0.6f, 1.0f);
    snprintf(b, sizeof(b), "NÍVEL %d%s", p.level, p.points > 0 ? "   (pontos de atributo: abra o Diário)" : ""); Text(F_SMALL, x, y + S(72), b, 0.6f, 0.85f, 1.0f);
    float lowT = p.thirst < 25.0f ? (0.6f + 0.4f*sinf(G.uiT*6.0f)) : 1.0f, lowH = p.hunger < 25.0f ? (0.6f + 0.4f*sinf(G.uiT*6.0f)) : 1.0f;
    Bar(x, y + S(94), w, S(10), p.thirst/100.0f, 0.3f*lowT, 0.65f*lowT, 1.0f*lowT); Text(F_SMALL, x + w + S(6), y + S(89), "SEDE", 0.5f, 0.8f, 1.0f);
    Bar(x, y + S(110), w, S(10), p.hunger/100.0f, 0.95f*lowH, 0.6f*lowH, 0.2f*lowH); Text(F_SMALL, x + w + S(6), y + S(105), "FOME", 1.0f, 0.75f, 0.4f);
}

static void DrawWeaponHud(int sw, int sh) {
    const Player& p = G.p; const ItemDef& w = ItemInfo(p.weapon); float x = (float)sw - S(24), y = (float)sh - S(100);
    Rect(x - S(260), y - S(10), S(270), S(90), 0, 0, 0, 0.35f);
    Text(F_MED, x, y, w.name, 1, 1, 1, 1, 2);
    if (w.type == ITY_GUN) {
        char b[48]; snprintf(b, sizeof(b), "%d / %d", p.mag[p.weapon], CountItem(w.ammo)); Text(F_BIG, x, y + S(26), b, p.mag[p.weapon] == 0 ? 1.0f : 0.95f, p.mag[p.weapon] == 0 ? 0.3f : 0.95f, p.mag[p.weapon] == 0 ? 0.3f : 0.5f, 1, 2);
        if (p.reloadT > 0) { float t = 1.0f - p.reloadT/Maxf(w.reload, 0.1f); Bar(x - S(240), y + S(66), S(240), S(8), t, 0.9f, 0.9f, 0.3f); Text(F_SMALL, x - S(244), y + S(60), "RECARREGANDO", 1, 1, 0.5f, 1, 0); }
    } else Text(F_MED, x, y + S(30), "Corpo a corpo", 0.8f, 0.8f, 0.8f, 1, 2);
    if (p.armor) { char b[48]; snprintf(b, sizeof(b), "RD %d", ItemInfo(p.armor).dr); Text(F_SMALL, x - S(250), y - S(4), b, 0.6f, 0.8f, 1.0f); }
}

static void DrawVehicleHud(int sw, int sh) {
    const Vehicle& v = *G.drive; const VehStat& vs = VehInfo(v.type); float x = (float)sw - S(24), y = (float)sh - S(150);
    Rect(x - S(270), y - S(12), S(280), S(138), 0, 0, 0, 0.4f);
    Text(F_MED, x, y, vs.name, 1, 1, 1, 1, 2);
    char b[48]; snprintf(b, sizeof(b), "%d km/h", (int)(fabsf(v.speed)*3.6f)); Text(F_HUGE, x, y + S(14), b, 1.0f, 0.9f, 0.6f, 1, 2);
    Bar(x - S(250), y + S(98), S(250), S(10), v.fuel/100.0f, v.fuel < 15.0f ? 1.0f : 0.95f, v.fuel < 15.0f ? 0.3f : 0.7f, 0.2f); Text(F_SMALL, x - S(254), y + S(80), "COMBUSTÍVEL", 1, 0.8f, 0.4f);
    Bar(x - S(250), y + S(118), S(250), S(8), v.hp/vs.hp, 0.8f, 0.2f, 0.2f);
    char t[96]; snprintf(t, sizeof(t), "[%s] sair     [%s] freio de mão     [V] câmera", K("E", "A"), K("Espaço", "B")); Text(F_SMALL, (float)sw*0.5f, (float)sh - S(60), t, 1, 1, 0.8f, 0.9f, 1);
    if (v.fuel <= 0.5f) Text(F_BIG, (float)sw*0.5f, (float)sh*0.3f, "SEM COMBUSTÍVEL", 1, 0.3f, 0.2f, 0.6f + 0.4f*sinf(G.uiT*6), 1);
}
static void DrawCrosshair(int sw, int sh) {
    const Player& p = G.p; float cx = (float)sw*0.5f, cy = (float)sh*0.5f; bool gun = ItemInfo(p.weapon).type == ITY_GUN;
    float gap = S(gun ? 6.0f + 14.0f*(1.0f - p.aimAmt) + p.recoil*14.0f + Sat(p.speed/4.0f)*6.0f : 8.0f), ln = S(7);
    if (G.cam.first && p.aimAmt > 0.8f) gap = S(3);
    float a = 0.85f; glColor4f(1, 1, 1, a); glBegin(GL_LINES);
    glVertex2f(cx - gap - ln, cy); glVertex2f(cx - gap, cy); glVertex2f(cx + gap, cy); glVertex2f(cx + gap + ln, cy);
    glVertex2f(cx, cy - gap - ln); glVertex2f(cx, cy - gap); glVertex2f(cx, cy + gap); glVertex2f(cx, cy + gap + ln); glEnd();
    Rect(cx - 1, cy - 1, 2, 2, 1, 1, 1, 0.9f);
    if (p.hitMarker > 0) { float k = S(10 + (0.35f - p.hitMarker)*20), al = Sat(p.hitMarker*5.0f); glColor4f(1, 0.2f, 0.2f, al); glBegin(GL_LINES);
        glVertex2f(cx - k, cy - k); glVertex2f(cx - k*0.5f, cy - k*0.5f); glVertex2f(cx + k, cy - k); glVertex2f(cx + k*0.5f, cy - k*0.5f);
        glVertex2f(cx - k, cy + k); glVertex2f(cx - k*0.5f, cy + k*0.5f); glVertex2f(cx + k, cy + k); glVertex2f(cx + k*0.5f, cy + k*0.5f); glEnd(); }
}

static void DrawMessages(int, int sh) {
    float y = (float)sh - S(180);
    for (int i = (int)G.msgs.size() - 1; i >= 0; i--) {
        const Msg& m = G.msgs[(size_t)i]; float a = Sat(m.t/1.2f); float r = 1, g = 1, b = 1;
        if (m.kind == 1) { r = 0.6f; g = 1; b = 0.6f; } else if (m.kind == 2) { r = 1; g = 0.9f; b = 0.4f; } else if (m.kind == 3) { r = 1; g = 0.45f; b = 0.4f; }
        Text(F_MED, S(24), y - S(24), m.text.c_str(), r, g, b, a); y -= S(24);
    }
}

static void DrawHudGame(int sw, int sh) {
    const Player& p = G.p;
    DrawCompass(sw); DrawVitals(sw, sh); DrawMessages(sw, sh);
    if (G.drive) DrawVehicleHud(sw, sh); else { DrawWeaponHud(sw, sh); if (!p.dead) DrawCrosshair(sw, sh); }
    if (G.locT > 0) { float a = Sat(G.locT/1.0f)*Sat((4.0f - G.locT)/0.5f + 0.2f); std::string u = G.location; for (size_t i = 0; i < u.size(); i++) if (u[i] >= 'a' && u[i] <= 'z') u[i] = (char)(u[i] - 32); Text(F_BIG, (float)sw*0.5f, S(100), u.c_str(), 1, 0.95f, 0.75f, a, 1); }
    if (G.tgt.type != 0 && !p.dead) {
        char b[128]; snprintf(b, sizeof(b), "[%s]  %s", K("E", "A"), G.tgt.prompt); float y = (float)sh*0.5f + S(60);
        float w = TW(F_MED, b); Rect((float)sw*0.5f - w*0.5f - S(12), y - S(4), w + S(24), S(30), 0, 0, 0, 0.45f); Text(F_MED, (float)sw*0.5f, y, b, 1, 1, 0.8f, 1, 1);
    }
    char t[64]; float hh = TimeOfDay(); snprintf(t, sizeof(t), "Dia %d   %02d:%02d", (int)(G.gameHours/24.0f) + 1, (int)hh, (int)((hh - floorf(hh))*60.0f)); Text(F_SMALL, (float)sw - S(20), S(14), t, 1, 1, 1, 0.9f, 2);
    if (G.saveBusy.load() > 0) Text(F_SMALL, (float)sw - S(20), S(36), "Salvando...", 1, 1, 0.5f, 0.9f, 2);
    if (G.p.sneak) Text(F_SMALL, (float)sw - S(20), S(58), "[ FURTIVO ]", 0.6f, 0.9f, 1.0f, 0.9f, 2);
    if (p.hurtT > 0) { float a = Sat(p.hurtT/0.45f)*0.45f; Rect(0, 0, (float)sw, (float)sh, 0.8f, 0, 0, a); }
    float lowK = Sat(1.0f - p.hp/Maxf(EffMaxHp(), 1.0f)); if (lowK > 0.6f && !p.dead) Rect(0, 0, (float)sw, (float)sh, 0.6f, 0, 0, (lowK - 0.6f)*0.5f*(0.6f + 0.4f*sinf(G.uiT*6.0f)));
    float rf = RadAt(p.pos.x, p.pos.z); if (rf > 0.05f) Rect(0, 0, (float)sw, (float)sh, 0.2f, 0.7f, 0.1f, rf*0.12f);
    if (p.rad > 600) Text(F_MED, (float)sw*0.5f, (float)sh - S(180), "RADIAÇÃO PERIGOSA: use Soro Antirradiação", 0.6f, 1, 0.3f, 0.6f + 0.4f*sinf(G.uiT*5), 1);
}

// ---------------------------------------------------------------------
//  MENUS
// ---------------------------------------------------------------------
static void Dim(int sw, int sh, float a) { Rect(0, 0, (float)sw, (float)sh, 0, 0, 0, a); }
static void Title(int sw, int sh, bool big) {
    float y = big ? (float)sh*0.12f : (float)sh*0.08f;
    Text(F_HUGE, (float)sw*0.5f + 3, y + 3, "RPGGL", 0, 0, 0, 0.7f, 1); Text(F_HUGE, (float)sw*0.5f, y, "RPGGL", 1.0f, 0.78f, 0.3f, 1, 1);
    Text(F_MED, (float)sw*0.5f, y + (float)fPx[F_HUGE] + S(4), "DEPOIS DA QUEDA DO CÉU", 0.85f, 0.85f, 0.8f, 0.9f, 1);
}
static void MenuList(int sw, int sh, const char** items, int n, int sel, const bool* enabled, float y0) {
    for (int i = 0; i < n; i++) {
        float y = y0 + (float)i*S(46); bool on = (i == sel); bool en = !enabled || enabled[i];
        if (on) { Rect((float)sw*0.5f - S(220), y - S(4), S(440), S(40), 1.0f, 0.78f, 0.3f, 0.2f); RectLine((float)sw*0.5f - S(220), y - S(4), S(440), S(40), 1.0f, 0.78f, 0.3f, 0.8f); }
        float c = en ? (on ? 1.0f : 0.85f) : 0.4f; Text(F_BIG, (float)sw*0.5f, y, items[i], c, c*(on ? 0.9f : 0.85f), c*(on ? 0.5f : 0.8f), 1, 1);
    }
    (void)sh;
}
static void HintBar(int sw, int sh, const char* s) { Text(F_SMALL, (float)sw*0.5f, (float)sh - S(34), s, 0.8f, 0.8f, 0.8f, 0.9f, 1); }

static void DrawMain(int sw, int sh) {
    Rect(0, 0, (float)sw, (float)sh, 0, 0, 0, 0.35f); Title(sw, sh, true);
    const char* it[5] = { "Continuar", "Novo Jogo", "Carregar", "Opções", "Sair" }; bool en[5] = { G.hasSaves, true, G.hasSaves, true, true };
    MenuList(sw, sh, it, 5, G.menuSel, en, (float)sh*0.45f);
    if (G.menuSel == 0 && G.hasSaves && !g_saveList.empty()) { const SaveInfo& s = g_saveList[0]; char b[160]; snprintf(b, sizeof(b), "%s  -  Nível %d  -  %s", s.name, s.level, s.location); Text(F_SMALL, (float)sw*0.5f, (float)sh*0.45f + S(240), b, 0.8f, 0.9f, 0.8f, 1, 1); }
    HintBar(sw, sh, K("Setas: navegar    Enter: confirmar", "D-Pad: navegar    A: confirmar"));
    Text(F_SMALL, S(16), (float)sh - S(30), "versão 0.2", 0.7f, 0.7f, 0.7f, 0.7f);
}
static void DrawNewGame(int sw, int sh) {
    Dim(sw, sh, 0.65f); Title(sw, sh, false);
    Text(F_BIG, (float)sw*0.5f, (float)sh*0.42f, "NOME DO PERSONAGEM", 1, 0.85f, 0.4f, 1, 1);
    float w = S(460), x = (float)sw*0.5f - w*0.5f, y = (float)sh*0.5f; Rect(x, y, w, S(52), 0, 0, 0, 0.6f); RectLine(x, y, w, S(52), 1, 0.78f, 0.3f, 0.9f);
    std::string s = G.nameBuf; if (fmodf(G.uiT, 1.0f) < 0.5f) s += "_"; Text(F_BIG, (float)sw*0.5f, y + S(8), s.empty() ? " " : s.c_str(), 1, 1, 1, 1, 1);
    if (G.nameBuf.empty()) Text(F_MED, (float)sw*0.5f, y + S(64), "(vazio = \"Andarilho\")", 0.7f, 0.7f, 0.7f, 1, 1);
    HintBar(sw, sh, "Digite o nome    Enter: começar    Esc: voltar");
}
static void DrawPause(int sw, int sh) {
    Dim(sw, sh, 0.55f); Title(sw, sh, false);
    const char* it[6] = { "Continuar", "Salvar", "Carregar", "Opções", "Menu Principal", "Sair do Jogo" };
    MenuList(sw, sh, it, 6, G.menuSel, 0, (float)sh*0.4f);
    HintBar(sw, sh, K("Setas: navegar    Enter: confirmar    Esc: voltar", "D-Pad: navegar    A: confirmar    B: voltar"));
}
static void DrawOptions(int sw, int sh) {
    Dim(sw, sh, 0.7f); Text(F_BIG, (float)sw*0.5f, (float)sh*0.12f, "OPÇÕES", 1, 0.85f, 0.4f, 1, 1);
    const Settings& s = G.set; char v[6][48];
    snprintf(v[0], 48, "%.1f", s.sens); snprintf(v[1], 48, "%d°", (int)s.fov); snprintf(v[2], 48, "%d%%", (int)(s.volume*100.0f + 0.5f)); snprintf(v[3], 48, "%s", s.invertY ? "Sim" : "Não");
    snprintf(v[4], 48, "%d chunks (%d m)", s.viewR, s.viewR*CHUNK_SIZE); snprintf(v[5], 48, "%s", s.showFps ? "Sim" : "Não");
    const char* nm[7] = { "Sensibilidade", "Campo de visão", "Volume", "Inverter eixo Y", "Distância de visão", "Mostrar FPS", "Voltar" };
    for (int i = 0; i < 7; i++) {
        float y = (float)sh*0.25f + (float)i*S(52); bool on = i == G.menuSel; float x0 = (float)sw*0.5f - S(300);
        if (on) { Rect(x0 - S(10), y - S(6), S(620), S(44), 1, 0.78f, 0.3f, 0.2f); RectLine(x0 - S(10), y - S(6), S(620), S(44), 1, 0.78f, 0.3f, 0.8f); }
        Text(F_BIG, x0, y, nm[i], on ? 1.0f : 0.8f, on ? 0.95f : 0.8f, on ? 0.6f : 0.8f);
        if (i < 6) Text(F_BIG, (float)sw*0.5f + S(300), y, v[i], 1, 1, 1, 1, 2);
    }
    HintBar(sw, sh, K("Setas: navegar / ajustar    Esc: voltar (salva)", "D-Pad: navegar / ajustar    B: voltar"));
}

static void FmtDate(u64 stamp, char* out, size_t n) { time_t t = (time_t)stamp; struct tm* lt = localtime(&t); if (lt) strftime(out, n, "%d/%m/%Y  %H:%M", lt); else snprintf(out, n, "-"); }

static void DrawSaveLoad(int sw, int sh, bool isSave) {
    Dim(sw, sh, 0.78f); Text(F_BIG, (float)sw*0.5f, S(40), isSave ? "SALVAR JOGO" : "CARREGAR JOGO", 1, 0.85f, 0.4f, 1, 1);
    int count = (int)g_saveList.size() + (isSave ? 1 : 0); int rows = Maxi(4, (int)(((float)sh - S(180))/S(40)));
    int first = Clampi(G.listSel - rows/2, 0, Maxi(0, count - rows)); float x0 = S(60), w = (float)sw*0.5f - S(80);
    Rect(x0 - S(10), S(100), w + S(20), (float)sh - S(190), 0, 0, 0, 0.4f); RectLine(x0 - S(10), S(100), w + S(20), (float)sh - S(190), PGR, PGG, PGB, 0.4f);
    if (count == 0) Text(F_MED, x0 + w*0.5f, S(140), "Nenhum save encontrado.", 0.8f, 0.8f, 0.8f, 1, 1);
    const SaveInfo* sel = 0;
    for (int r = 0; r < rows && first + r < count; r++) {
        int idx = first + r; float y = S(110) + (float)r*S(40); bool on = idx == G.listSel; int si = idx - (isSave ? 1 : 0);
        if (on) { Rect(x0 - S(6), y - S(3), w + S(12), S(36), 1, 0.78f, 0.3f, 0.22f); }
        if (isSave && idx == 0) { Text(F_MED, x0, y, "[ Novo salvamento ]", on ? 1.0f : 0.8f, 1.0f, 0.6f); if (on) sel = 0; continue; }
        const SaveInfo& s = g_saveList[(size_t)si]; if (on) sel = &s;
        const char* tag = s.kind == SK_QUICK ? "[Rápido] " : (s.kind == SK_AUTO ? "[Auto] " : ""); char b[200]; snprintf(b, sizeof(b), "%s%s  -  Nv %d  -  %s", tag, s.name, s.level, s.location);
        float c = on ? 1.0f : 0.78f; Text(F_MED, x0, y, b, c, c, c*0.85f);
    }
    // painel de detalhes
    float px = (float)sw*0.5f + S(10), pw = (float)sw*0.5f - S(70), ph = pw*9.0f/16.0f;
    Rect(px - S(6), S(100), pw + S(12), (float)sh - S(190), 0, 0, 0, 0.4f); RectLine(px - S(6), S(100), pw + S(12), (float)sh - S(190), PGR, PGG, PGB, 0.4f);
    if (sel) {
        std::string key = sel->path;
        if (g_thumbFor != key) {                                                  // envia a miniatura como textura (256x128, potencia de 2)
            if (!g_thumbTex) glGenTextures(1, &g_thumbTex); glBindTexture(GL_TEXTURE_2D, g_thumbTex); g_thumbFor = key;
            if (!sel->thumb.empty()) { glPixelStorei(GL_UNPACK_ALIGNMENT, 1); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sel->tw, sel->th, 0, GL_RGB, GL_UNSIGNED_BYTE, sel->thumb.data()); }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        }
        if (!sel->thumb.empty()) {
            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_thumbTex); glColor4f(1, 1, 1, 1);
            glBegin(GL_QUADS); glTexCoord2f(0, 0); glVertex2f(px, S(110)); glTexCoord2f(1, 0); glVertex2f(px + pw, S(110)); glTexCoord2f(1, 1); glVertex2f(px + pw, S(110) + ph); glTexCoord2f(0, 1); glVertex2f(px, S(110) + ph); glEnd();
            glDisable(GL_TEXTURE_2D); RectLine(px, S(110), pw, ph, 1, 1, 1, 0.6f);
        } else { Rect(px, S(110), pw, ph, 0.1f, 0.1f, 0.1f, 1); Text(F_MED, px + pw*0.5f, S(110) + ph*0.5f - S(10), "(sem imagem)", 0.6f, 0.6f, 0.6f, 1, 1); }
        float y = S(120) + ph; char b[200], d[48];
        snprintf(b, sizeof(b), "%s  -  Nível %d", sel->name, sel->level); Text(F_BIG, px, y, b, 1, 0.9f, 0.5f); y += S(42);
        snprintf(b, sizeof(b), "Local: %s", sel->location); Text(F_MED, px, y, b, 1, 1, 1); y += S(28);
        snprintf(b, sizeof(b), "Tempo de jogo: %dh %02dm", (int)(sel->playSeconds/3600.0f), (int)(sel->playSeconds/60.0f) % 60); Text(F_MED, px, y, b, 1, 1, 1); y += S(28);
        float hh = fmodf(sel->gameHours, 24.0f); snprintf(b, sizeof(b), "Dia %d, %02d:%02d no jogo", (int)(sel->gameHours/24.0f) + 1, (int)hh, (int)((hh - floorf(hh))*60.0f)); Text(F_MED, px, y, b, 1, 1, 1); y += S(28);
        FmtDate(sel->stamp, d, sizeof(d)); snprintf(b, sizeof(b), "Salvo em: %s", d); Text(F_MED, px, y, b, 0.8f, 0.8f, 0.8f); y += S(28);
        Text(F_SMALL, px, y, sel->file.c_str(), 0.6f, 0.6f, 0.6f);
    } else if (isSave && G.listSel == 0) Text(F_MED, px + pw*0.5f, S(140), "Cria um novo arquivo .sav na pasta \"saves\".", 0.85f, 0.85f, 0.85f, 1, 1);
    if (G.confirmDel) Text(F_MED, (float)sw*0.5f, (float)sh - S(70), "Pressione DEL/X novamente para APAGAR o save selecionado", 1, 0.4f, 0.3f, 1, 1);
    HintBar(sw, sh, K("Enter: confirmar    Del: apagar    Esc: voltar", "A: confirmar    X: apagar    B: voltar"));
}

// ---------------------------------------------------------------------
//  DIARIO DO ANDARILHO
// ---------------------------------------------------------------------
static void DrawMapTab(float x, float y, float w, float h, int sh) {
    const Player& p = G.p; static const float scales[3] = { 3.0f, 8.0f, 24.0f }; float sc = scales[Clampi(G.listSel, 0, 2)];
    Rect(x, y, w, h, 0.10f, 0.08f, 0.055f, 1); glEnable(GL_SCISSOR_TEST); glScissor((int)x, sh - (int)(y + h), (int)w, (int)h);
    float cx = x + w*0.5f, cy = y + h*0.5f; int step = 6;
    for (float yy = y; yy < y + h; yy += (float)step) for (float xx = x; xx < x + w; xx += (float)step) {
        float wx = p.pos.x + (xx + step*0.5f - cx)*sc, wz = p.pos.z + (yy + step*0.5f - cy)*sc;
        float rf = RoadFactor(wx, wz); if (rf > 0.45f) Rect(xx, yy, (float)step, (float)step, 0.62f, 0.52f, 0.36f, 0.9f);
        else if (RadAt(wx, wz) > 0.2f) Rect(xx, yy, (float)step, (float)step, 0.3f, 0.7f, 0.1f, 0.5f);
    }
    glColor4f(1, 0.3f, 0.2f, 0.8f); glBegin(GL_LINE_LOOP); float e0 = WORLD_EDGE, e1 = WORLD_SIZE - WORLD_EDGE;
    glVertex2f(cx + (e0 - p.pos.x)/sc, cy + (e0 - p.pos.z)/sc); glVertex2f(cx + (e1 - p.pos.x)/sc, cy + (e0 - p.pos.z)/sc); glVertex2f(cx + (e1 - p.pos.x)/sc, cy + (e1 - p.pos.z)/sc); glVertex2f(cx + (e0 - p.pos.x)/sc, cy + (e1 - p.pos.z)/sc); glEnd();
    std::vector<u64> keys(G.discovered.begin(), G.discovered.end()); keys.push_back(ChunkKey(BUNKER_CX, BUNKER_CZ)); keys.push_back(ChunkKey(HOPE_CX, HOPE_CZ));
    for (size_t i = 0; i < keys.size(); i++) {
        int pcx = (int)(keys[i] >> 32), pcz = (int)(keys[i] & 0xFFFFFFFFu); int poi = PoiAt(pcx, pcz); if (!poi) continue;
        float sx = cx + ((float)(pcx*CHUNK_SIZE + 32) - p.pos.x)/sc, sy = cy + ((float)(pcz*CHUNK_SIZE + 32) - p.pos.z)/sc; if (sx < x - 20 || sx > x + w + 20 || sy < y - 20 || sy > y + h + 20) continue;
        float r = poi == POI_SETTLEMENT || poi == POI_VILLAGE_HOPE ? 0.3f : (poi == POI_RAIDERS || poi == POI_RAIDER_BASE ? 1.0f : 0.95f), g = poi == POI_SETTLEMENT || poi == POI_VILLAGE_HOPE ? 0.9f : (poi == POI_RAIDERS || poi == POI_RAIDER_BASE ? 0.3f : 0.85f), b = poi == POI_SETTLEMENT || poi == POI_VILLAGE_HOPE ? 0.4f : 0.3f;
        Diamond(sx, sy, S(6), r, g, b, 1); if (sc < 10.0f) Text(F_SMALL, sx, sy + S(8), PoiName(poi, pcx, pcz), 0.85f, 1, 0.85f, 1, 1);
    }
    for (int q = 0; q < Q_COUNT; q++) { Vec3 m; if (!QuestMarker(q, m)) continue; float sx = cx + (m.x - p.pos.x)/sc, sy = cy + (m.z - p.pos.z)/sc; sx = Clampf(sx, x + 12, x + w - 12); sy = Clampf(sy, y + 12, y + h - 12); Diamond(sx, sy, S(8) + sinf(G.uiT*4)*S(2), 0.3f, 0.8f, 1.0f, 1); }
    Vec3 f = Facing(p.yaw); float fx = f.x, fz = f.z; glColor4f(1, 1, 1, 1); glBegin(GL_TRIANGLES);
    glVertex2f(cx + fx*S(12), cy + fz*S(12)); glVertex2f(cx - fx*S(7) - fz*S(7), cy - fz*S(7) + fx*S(7)); glVertex2f(cx - fx*S(7) + fz*S(7), cy - fz*S(7) - fx*S(7)); glEnd();
    glDisable(GL_SCISSOR_TEST); RectLine(x, y, w, h, PGR, PGG, PGB, 0.8f);
    char b[64]; snprintf(b, sizeof(b), "Zoom: %d/3 (cima/baixo)   Escala: %d m/pixel", G.listSel + 1, (int)sc); Text(F_SMALL, x + S(8), y + h - S(22), b, PGR, PGG, PGB);
    Text(F_SMALL, x + S(8), y + S(6), "N: norte para cima", PGR, PGG, PGB);
}

static void DrawDiary(int sw, int sh) {
    const Player& p = G.p; Dim(sw, sh, 0.5f);
    float x = S(60), y = S(50), w = (float)sw - S(120), h = (float)sh - S(100);
    Rect(x, y, w, h, 0.07f, 0.05f, 0.035f, 0.93f); RectLine(x, y, w, h, PGR, PGG, PGB, 0.9f); RectLine(x + 3, y + 3, w - 6, h - 6, PGR, PGG, PGB, 0.35f);
    const char* tabs[4] = { "STATUS", "ITENS", "MISSÕES", "MAPA" }; float tw = w/4.0f;
    for (int i = 0; i < 4; i++) {
        bool on = i == G.tab; if (on) Rect(x + tw*(float)i + 4, y + 4, tw - 8, S(40), PGR, PGG, PGB, 0.22f);
        Text(F_BIG, x + tw*((float)i + 0.5f), y + S(6), tabs[i], on ? 1.0f : PGR, on ? 0.92f : PGG*0.8f, on ? 0.7f : PGB*0.8f, 1, 1);
    }
    glColor4f(PGR, PGG, PGB, 0.8f); glBegin(GL_LINES); glVertex2f(x, y + S(48)); glVertex2f(x + w, y + S(48)); glEnd();
    float cy0 = y + S(62), ch = h - S(110); char b[200];
    if (G.tab == 0) {
        float lx = x + S(30); snprintf(b, sizeof(b), "%s", p.name); Text(F_BIG, lx, cy0, b, 1, 1, 1);
        snprintf(b, sizeof(b), "Nível %d", p.level); Text(F_BIG, lx, cy0 + S(40), b, PGR, PGG, PGB);
        int need = 80 + 45*p.level; Bar(lx, cy0 + S(86), S(320), S(14), (float)p.xp/(float)need, 0.3f, 0.6f, 1.0f); snprintf(b, sizeof(b), "XP %d / %d", p.xp, need); Text(F_SMALL, lx + S(330), cy0 + S(84), b, 0.7f, 0.85f, 1);
        float yy = cy0 + S(118); const char* ln[10]; char buf[10][64];
        snprintf(buf[0], 64, "Vida: %d / %d (máx. efetiva %d)", (int)p.hp, (int)p.maxhp, (int)EffMaxHp()); snprintf(buf[1], 64, "Radiação: %d / 1000", (int)p.rad); snprintf(buf[2], 64, "Resistência a dano: %d%s%s", ItemInfo(p.armor).dr, p.artifact ? "   Estilhaço: " : "", p.artifact ? ItemInfo(p.artifact).name : "");
        snprintf(buf[3], 64, "Fichas: %d", p.money); snprintf(buf[4], 64, "Peso: %.0f / %.0f", CarryWeight(), MaxCarry()); snprintf(buf[5], 64, "Karma: %d", G.karma);
        snprintf(buf[6], 64, "Inimigos abatidos: %d", G.kills); snprintf(buf[7], 64, "Tempo de jogo: %dh %02dm", (int)(G.playSeconds/3600.0f), (int)(G.playSeconds/60.0f) % 60);
        snprintf(buf[8], 64, "Sede: %d%%", (int)p.thirst); snprintf(buf[9], 64, "Fome: %d%%", (int)p.hunger);
        for (int i = 0; i < 10; i++) ln[i] = buf[i];
        for (int i = 0; i < 10; i++) { Text(F_MED, lx, yy, ln[i], 1.0f, 0.95f, 0.8f); yy += S(26); }
        float rx = x + w*0.55f; Text(F_BIG, rx, cy0, "ATRIBUTOS", PGR + 0.2f, PGG, PGB + 0.1f);
        static const char* desc[AT_COUNT] = { "Dano corpo a corpo e capacidade de carga.", "Dano e precisão dos tiros.", "Estamina máxima.", "Vida máxima.", "Preços melhores com mercadores.", "Absorve menos radiação das fissuras e zonas quentes." };
        for (int i = 0; i < AT_COUNT; i++) {
            float ry = cy0 + S(46) + (float)i*S(38); bool on = (i == G.listSel);
            if (on) { Rect(rx - S(8), ry - S(3), w*0.4f, S(34), PGR, PGG, PGB, 0.2f); }
            Text(F_MED, rx, ry, ATTR_NAMES[i], on ? 1.0f : 0.8f, 1.0f, on ? 0.8f : 0.7f); snprintf(b, sizeof(b), "%d", p.attr[i]); Text(F_BIG, rx + w*0.35f, ry - S(4), b, 1, 1, 1, 1, 2);
        }
        Text(F_SMALL, rx, cy0 + S(46) + (float)AT_COUNT*S(38) + S(6), desc[Clampi(G.listSel, 0, AT_COUNT - 1)], 0.8f, 0.95f, 0.8f);
        if (p.points > 0) { snprintf(b, sizeof(b), "Pontos disponíveis: %d  -  [%s] para aumentar o atributo selecionado", p.points, K("Enter", "A")); Text(F_MED, rx, cy0 + S(46) + (float)AT_COUNT*S(38) + S(34), b, 1, 0.9f, 0.3f); }
    } else if (G.tab == 1) {
        int rows = Maxi(4, (int)((ch - S(40))/S(30))); int first = Clampi(G.listSel - rows/2, 0, Maxi(0, (int)p.inv.size() - rows)); float lx = x + S(24), lw = w*0.5f;
        if (p.inv.empty()) Text(F_MED, lx, cy0, "Inventário vazio.", 0.8f, 0.9f, 0.8f);
        const char* secs[8] = { "ARMAS CORPO A CORPO", "ARMAS DE FOGO", "MUNIÇÃO", "CONSUMÍVEIS", "SUCATA", "ARMADURA", "DIVERSOS", "ESTILHAÇOS" }; int lastType = -1; float yy = cy0;
        for (int r = 0; r < rows && first + r < (int)p.inv.size(); r++) {
            int idx = first + r; const Slot& s = p.inv[(size_t)idx]; const ItemDef& d = ItemInfo(s.id); bool on = idx == G.listSel;
            if (d.type != lastType) { lastType = d.type; Text(F_SMALL, lx, yy, secs[Clampi(d.type, 0, 7)], PGR, PGG*0.7f, PGB*0.7f); yy += S(20); }
            if (on) Rect(lx - S(6), yy - S(2), lw, S(28), PGR, PGG, PGB, 0.22f);
            bool eq = (s.id == p.weapon || s.id == p.armor || s.id == p.artifact); snprintf(b, sizeof(b), "%s%s%s", eq ? "[E] " : "", d.name, s.count > 1 ? (" (x" + std::to_string(s.count) + ")").c_str() : "");
            Text(F_MED, lx, yy, b, on ? 1.0f : 0.8f, 1.0f, on ? 0.85f : 0.75f); yy += S(30);
        }
        if (!p.inv.empty()) {
            const Slot& s = p.inv[(size_t)Clampi(G.listSel, 0, (int)p.inv.size() - 1)]; const ItemDef& d = ItemInfo(s.id); float rx = x + w*0.55f, ry = cy0;
            Text(F_BIG, rx, ry, d.name, 1, 1, 1); ry += S(46); WrapText(F_MED, rx, ry, w*0.4f, d.desc, 0.85f, 1.0f, 0.85f, S(26)); ry += S(90);
            if (d.type == ITY_GUN || d.type == ITY_MELEE) { snprintf(b, sizeof(b), "Dano: %.0f%s", d.dmg, d.pellets > 1 ? (" x" + std::to_string(d.pellets)).c_str() : ""); Text(F_MED, rx, ry, b, 1, 0.8f, 0.6f); ry += S(26); }
            if (d.type == ITY_GUN) { snprintf(b, sizeof(b), "Carregador: %d   Alcance: %.0f m", d.mag, d.range); Text(F_MED, rx, ry, b, 1, 0.8f, 0.6f); ry += S(26); }
            if (d.type == ITY_ARMOR) { snprintf(b, sizeof(b), "Resistência a dano: %d", d.dr); Text(F_MED, rx, ry, b, 0.6f, 0.85f, 1); ry += S(26); }
            if (d.type == ITY_AID) { if (d.heal > 0) { snprintf(b, sizeof(b), "Cura: %.0f", d.heal); Text(F_MED, rx, ry, b, 0.6f, 1, 0.6f); ry += S(26); } if (d.rads != 0) { snprintf(b, sizeof(b), "Radiação: %+.0f", d.rads); Text(F_MED, rx, ry, b, 0.6f, 1, 0.3f); ry += S(26); } }
            snprintf(b, sizeof(b), "Peso: %.1f   Valor: %d", d.weight, d.value); Text(F_MED, rx, ry, b, 0.8f, 0.8f, 0.8f); ry += S(34);
            const char* act = (d.type == ITY_AID) ? "usar" : ((d.type == ITY_GUN || d.type == ITY_MELEE) ? "equipar" : (d.type == ITY_ARMOR ? "vestir/remover" : (d.type == ITY_ARTIFACT ? "equipar/remover" : (s.id == IT_FUEL ? "abastecer veículo" : ""))));
            if (act[0]) { snprintf(b, sizeof(b), "[%s] %s", K("Enter", "A"), act); Text(F_MED, rx, ry, b, 1, 0.9f, 0.4f); }
        }
        snprintf(b, sizeof(b), "Peso: %.0f / %.0f      Fichas: %d", CarryWeight(), MaxCarry(), p.money); Text(F_MED, x + S(24), y + h - S(44), b, CarryWeight() > MaxCarry() ? 1.0f : 0.9f, CarryWeight() > MaxCarry() ? 0.4f : 1.0f, 0.7f);
    } else if (G.tab == 2) {
        float lx = x + S(24); float yy = cy0;
        for (int q = 0; q < Q_COUNT; q++) {
            if (G.quests[q].state == 0) continue; bool on = (q == G.listSel); if (on) Rect(lx - S(6), yy - S(2), w*0.38f, S(32), PGR, PGG, PGB, 0.22f);
            snprintf(b, sizeof(b), "%s%s", QUEST_NAMES[q], G.quests[q].state == 2 ? "  (concluída)" : ""); Text(F_MED, lx, yy, b, G.quests[q].state == 2 ? 0.55f : (on ? 1.0f : 0.85f), G.quests[q].state == 2 ? 0.7f : 1.0f, G.quests[q].state == 2 ? 0.55f : 0.8f); yy += S(34);
        }
        int q = Clampi(G.listSel, 0, Q_COUNT - 1); float rx = x + w*0.45f;
        if (G.quests[q].state != 0) {
            Text(F_BIG, rx, cy0, QUEST_NAMES[q], 1, 0.9f, 0.5f); WrapText(F_MED, rx, cy0 + S(50), w*0.5f, QuestDesc(q), 0.85f, 0.95f, 0.85f, S(26));
            Text(F_MED, rx, cy0 + S(140), "OBJETIVO", PGR, PGG, PGB); WrapText(F_MED, rx, cy0 + S(168), w*0.5f, QuestObjective(q), 1, 1, 1, S(26));
        } else Text(F_MED, rx, cy0, "Nenhuma missão ativa neste item.", 0.7f, 0.7f, 0.7f);
        if (G.quests[Q_ARRIVE].state == 0 && G.quests[Q_RAIDERS].state == 0) Text(F_MED, lx, yy, "Sem missões.", 0.7f, 0.7f, 0.7f);
    } else DrawMapTab(x + S(20), cy0 - S(6), w - S(40), ch + S(30), sh);
    Text(F_SMALL, x + w*0.5f, y + h - S(24), K("Q/R ou Setas: trocar aba     Tab: fechar     Enter: usar", "LB/RB: trocar aba     Y: fechar     A: usar"), PGR, PGG, PGB, 0.9f, 1);
}

// ---------------------------------------------------------------------
//  DIALOGO / COMERCIO / MORTE / CARREGAMENTO
// ---------------------------------------------------------------------
static void DrawDialog(int sw, int sh) {
    float w = (float)sw*0.7f, h = S(190), x = ((float)sw - w)*0.5f, y = (float)sh - h - S(50);
    Rect(x, y, w, h, 0, 0, 0, 0.78f); RectLine(x, y, w, h, 1, 0.78f, 0.3f, 0.8f);
    Text(F_BIG, x + S(24), y + S(12), G.dlg.speaker.c_str(), 1, 0.85f, 0.4f);
    int pg = Clampi(G.dlg.page, 0, Maxi(0, (int)G.dlg.lines.size() - 1));
    if (!G.dlg.lines.empty()) WrapText(F_MED, x + S(24), y + S(60), w - S(48), G.dlg.lines[(size_t)pg].c_str(), 1, 1, 1, S(28), 4);
    char b[96]; snprintf(b, sizeof(b), "[%s] %s   (%d/%d)", K("Enter / E", "A"), pg + 1 >= (int)G.dlg.lines.size() ? "encerrar" : "continuar", pg + 1, (int)G.dlg.lines.size());
    Text(F_SMALL, x + w - S(24), y + h - S(28), b, 0.8f, 0.8f, 0.8f, 1, 2);
}
static void DrawBarter(int sw, int sh) {
    const Player& p = G.p; Dim(sw, sh, 0.7f); float x = S(60), y = S(50), w = (float)sw - S(120), h = (float)sh - S(100);
    Rect(x, y, w, h, 0.07f, 0.05f, 0.035f, 0.93f); RectLine(x, y, w, h, PGR, PGG, PGB, 0.9f);
    Text(F_BIG, x + w*0.5f, y + S(10), "COMÉRCIO", 1, 0.85f, 0.4f, 1, 1); char b[128]; snprintf(b, sizeof(b), "Fichas: %d", p.money); Text(F_BIG, x + w - S(24), y + S(10), b, 1, 0.9f, 0.4f, 1, 2);
    for (int col = 0; col < 2; col++) {
        float cx = x + S(24) + (float)col*w*0.5f, cw = w*0.5f - S(48); int n = col == 0 ? (int)G.shop.size() : (int)p.inv.size(); int sel = G.shopSel[col];
        Text(F_MED, cx, y + S(62), col == 0 ? "COMPRAR" : "VENDER", col == G.shopCol ? 1.0f : 0.6f, col == G.shopCol ? 0.95f : 0.7f, col == G.shopCol ? 0.5f : 0.6f);
        int rows = Maxi(4, (int)((h - S(190))/S(28))); int first = Clampi(sel - rows/2, 0, Maxi(0, n - rows));
        for (int r = 0; r < rows && first + r < n; r++) {
            int idx = first + r; float yy = y + S(92) + (float)r*S(28); bool on = (idx == sel && col == G.shopCol); int id, price; std::string nm;
            if (col == 0) { id = G.shop[(size_t)idx].id; price = G.shop[(size_t)idx].price; nm = ItemInfo(id).name; if (ItemInfo(id).type == ITY_AMMO) nm += " (x5)"; }
            else { id = p.inv[(size_t)idx].id; price = SellPrice(id); nm = ItemInfo(id).name; if (p.inv[(size_t)idx].count > 1) nm += " (x" + std::to_string(p.inv[(size_t)idx].count) + ")"; if (id == p.weapon || id == p.armor || id == p.artifact) nm = "[E] " + nm; }
            if (on) Rect(cx - S(6), yy - S(2), cw, S(26), PGR, PGG, PGB, 0.25f);
            Text(F_MED, cx, yy, nm.c_str(), on ? 1.0f : 0.8f, 1.0f, on ? 0.85f : 0.75f); snprintf(b, sizeof(b), "%d", price); Text(F_MED, cx + cw - S(10), yy, b, 1, 0.9f, 0.4f, 1, 2);
        }
    }
    Text(F_SMALL, x + w*0.5f, y + h - S(30), K("Setas: navegar / trocar coluna     Enter: comprar-vender     Esc: sair", "D-Pad: navegar / trocar coluna     A: comprar-vender     B: sair"), PGR, PGG, PGB, 0.9f, 1);
}
static void DrawDead(int sw, int sh) {
    Rect(0, 0, (float)sw, (float)sh, 0.35f, 0, 0, 0.7f); Text(F_HUGE, (float)sw*0.5f, (float)sh*0.18f, "VOCÊ MORREU", 0.95f, 0.15f, 0.12f, 1, 1);
    const char* it[2] = { "Carregar último save", "Menu Principal" }; MenuList(sw, sh, it, 2, G.menuSel, 0, (float)sh*0.48f);
}
static void DrawLoading(int sw, int sh) {
    Rect(0, 0, (float)sw, (float)sh, 0.02f, 0.02f, 0.02f, 1); Title(sw, sh, true);
    float k = Sat((float)::W.statReady/25.0f); Text(F_BIG, (float)sw*0.5f, (float)sh*0.55f, "CARREGANDO O MUNDO...", 1, 0.9f, 0.6f, 1, 1);
    Bar((float)sw*0.5f - S(250), (float)sh*0.55f + S(60), S(500), S(18), k, 1, 0.78f, 0.3f);
    static const char* tips[4] = { "Dica: o jogo salva em arquivos .sav na pasta \"saves\" ao lado do .exe.", "Dica: fissuras estalam antes de descarregar. Ouça, dê a volta e colete os estilhaços.", "Dica: carne crua mata a fome, mas é radioativa. Prefira rações.", "Dica: reabasteça antes do tanque secar: galões de combustível aparecem em saques." };
    Text(F_MED, (float)sw*0.5f, (float)sh*0.8f, tips[((int)(G.uiT/5.0f) + (int)G.kills) & 3], 0.8f, 0.8f, 0.8f, 1, 1);
}
static void DrawDebug(int, int) {
    char b[256]; float y = S(80); const Player& p = G.p;
    snprintf(b, sizeof(b), "FPS %.0f", G.fps); Text(F_SMALL, S(20), y, b, 1, 1, 0.4f); y += S(18);
    snprintf(b, sizeof(b), "Chunks: %d prontos / %d gerando | Threads de trabalho: %d | Fila de fundo: %d", ::W.statReady, ::W.statPending, G.pool.Workers(), G.pool.PendingLow()); Text(F_SMALL, S(20), y, b, 1, 1, 0.4f); y += S(18);
    snprintf(b, sizeof(b), "NPCs: %d carregados / %d ativos (IA paralela) | Ragdolls ativos: %d | Veículos: %d", (int)G.npcs.size(), G.npcActive, G.ragActive, (int)G.vehicles.size()); Text(F_SMALL, S(20), y, b, 1, 1, 0.4f); y += S(18);
    snprintf(b, sizeof(b), "Pos: %.0f, %.0f, %.0f  chunk (%d,%d)  hora %.2f", p.pos.x, p.pos.y, p.pos.z, ChunkOf(p.pos.x), ChunkOf(p.pos.z), TimeOfDay()); Text(F_SMALL, S(20), y, b, 1, 1, 0.4f); y += S(18);
    snprintf(b, sizeof(b), "Area: %s   Saves ativos: %d", AreaName(p.pos.x, p.pos.z), G.saveBusy.load()); Text(F_SMALL, S(20), y, b, 1, 1, 0.4f);
}
static void DrawHelp(int sw, int sh) {
    Dim(sw, sh, 0.8f); Text(F_BIG, (float)sw*0.5f, S(40), "CONTROLES", 1, 0.85f, 0.4f, 1, 1);
    const char* kb[] = { "WASD", "Mouse", "Botão esq. / dir.", "Shift", "Espaço", "C / Ctrl", "E", "R", "F", "H", "1-4 / Roda", "V", "Tab / I", "F5 / F9", "F3", "F11", "Esc", "W/S  A/D", "Espaço (no carro)" };
    const char* ds[] = { "mover", "olhar", "atirar / mirar", "correr", "pular", "agachar (furtivo)", "interagir (falar, abrir, revistar)", "recarregar", "coronhada", "usar injetor médico", "trocar de arma", "alternar 1ª / 3ª pessoa", "Diário (status, itens, missões, mapa)", "salvar rápido / carregar último", "informações técnicas", "tela cheia", "pausa", "veículo: acelerar-frear / esterçar", "freio de mão (E sai do carro)" };
    const char* pk[] = { "Analógico esq.", "Analógico dir.", "RT / LT", "Clique analógico esq.", "B", "D-Pad baixo", "A", "X", "D-Pad direita", "D-Pad cima", "LB / RB", "Clique analógico dir.", "Y", "D-Pad esquerda", "", "", "START", "RT/LT + analógico esq.", "B" };
    for (int i = 0; i < 19; i++) { float y = S(90) + (float)i*S(28); Text(F_MED, (float)sw*0.5f - S(330), y, kb[i], 1, 0.95f, 0.6f); Text(F_MED, (float)sw*0.5f - S(110), y, ds[i], 1, 1, 1); Text(F_MED, (float)sw*0.5f + S(330), y, pk[i], 0.5f, 0.9f, 1, 1, 2); }
    Text(F_SMALL, (float)sw*0.5f, (float)sh - S(40), "F1: fechar", 0.8f, 0.8f, 0.8f, 1, 1);
}

void DrawHUD(int sw, int sh) {
    Begin2D(sw, sh);
    switch (G.ui) {
    case UI_MAIN: DrawMain(sw, sh); break;
    case UI_NEWGAME: DrawNewGame(sw, sh); break;
    default: break;
    }
    if (G.state == GS_PLAYING) {
        if (G.ui == UI_NONE || G.ui == UI_PAUSE || G.ui == UI_SAVE || G.ui == UI_LOAD || G.ui == UI_OPTIONS || G.ui == UI_DEAD) DrawHudGame(sw, sh);
        switch (G.ui) {
        case UI_PAUSE: DrawPause(sw, sh); break;
        case UI_SAVE: DrawSaveLoad(sw, sh, true); break;
        case UI_LOAD: DrawSaveLoad(sw, sh, false); break;
        case UI_OPTIONS: DrawOptions(sw, sh); break;
        case UI_DIARY: DrawDiary(sw, sh); break;
        case UI_DIALOG: DrawMessages(sw, sh); DrawDialog(sw, sh); break;
        case UI_BARTER: DrawBarter(sw, sh); break;
        case UI_DEAD: DrawDead(sw, sh); break;
        default: break;
        }
    } else if (G.state == GS_MENU) {
        switch (G.ui) { case UI_LOAD: DrawSaveLoad(sw, sh, false); break; case UI_OPTIONS: DrawOptions(sw, sh); break; default: break; }
        DrawMessages(sw, sh);
    }
    if (G.state == GS_LOADING) DrawLoading(sw, sh);
    if (G.set.showFps && G.state != GS_LOADING) { char b[32]; snprintf(b, sizeof(b), "%.0f FPS", G.fps); Text(F_SMALL, S(20), S(14), b, 1, 1, 0.4f); }
    static bool dbg = false; if (G.in.debug) dbg = !dbg; if (dbg && G.state == GS_PLAYING) DrawDebug(sw, sh);
    if (g_showHelp) DrawHelp(sw, sh);
    End2D();
}
