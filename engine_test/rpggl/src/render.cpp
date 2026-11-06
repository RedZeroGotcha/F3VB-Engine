// =====================================================================
//  render.cpp - renderizacao 3D (OpenGL 1.x, pipeline fixo, sem GLU)
//
//  Ordem do quadro:
//    1. ambiente (hora do dia -> cores do ceu, neblina, luz do sol/lua)
//    2. ceu (domo em faixas, estrelas, sol, lua)  -- sem profundidade
//    3. chunks (vertex arrays prontos pelos workers) + objetos dinamicos
//    4. personagens articulados (jogador, NPCs), sombras, fogueiras
//    5. tiros (tracers), particulas, marcadores de missao
//    6. viewmodel da arma em 1a pessoa
//
//  Convencoes: personagens olham para -Z local; yaw usa glRotatef(Y).
//  Nada aqui altera o estado do jogo.
// =====================================================================
#include "gfx.h"

static GLuint dlCube, dlOcta, dlWheel;
static std::vector<Vec3> g_stars;
static float T() { return (float)GetTickCount()/1000.0f; }

static void BuildCube() {
    dlCube = glGenLists(1); glNewList(dlCube, GL_COMPILE); glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);  glVertex3f(-.5f, -.5f, .5f);  glVertex3f(.5f, -.5f, .5f);  glVertex3f(.5f, .5f, .5f);  glVertex3f(-.5f, .5f, .5f);
    glNormal3f(0, 0, -1); glVertex3f(.5f, -.5f, -.5f);  glVertex3f(-.5f, -.5f, -.5f); glVertex3f(-.5f, .5f, -.5f); glVertex3f(.5f, .5f, -.5f);
    glNormal3f(1, 0, 0);  glVertex3f(.5f, -.5f, .5f);   glVertex3f(.5f, -.5f, -.5f);  glVertex3f(.5f, .5f, -.5f);  glVertex3f(.5f, .5f, .5f);
    glNormal3f(-1, 0, 0); glVertex3f(-.5f, -.5f, -.5f); glVertex3f(-.5f, -.5f, .5f);  glVertex3f(-.5f, .5f, .5f);  glVertex3f(-.5f, .5f, -.5f);
    glNormal3f(0, 1, 0);  glVertex3f(-.5f, .5f, .5f);   glVertex3f(.5f, .5f, .5f);    glVertex3f(.5f, .5f, -.5f);  glVertex3f(-.5f, .5f, -.5f);
    glNormal3f(0, -1, 0); glVertex3f(-.5f, -.5f, -.5f); glVertex3f(.5f, -.5f, -.5f);  glVertex3f(.5f, -.5f, .5f);  glVertex3f(-.5f, -.5f, .5f);
    glEnd(); glEndList();
}
static void BuildExtras() {
    dlOcta = glGenLists(1); glNewList(dlOcta, GL_COMPILE); glBegin(GL_TRIANGLES);
    const float v[6][3] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    const int f[8][3] = { { 0, 4, 2 }, { 0, 2, 5 }, { 0, 5, 3 }, { 0, 3, 4 }, { 1, 2, 4 }, { 1, 5, 2 }, { 1, 3, 5 }, { 1, 4, 3 } };
    for (int i = 0; i < 8; i++) { float nx = v[f[i][0]][0] + v[f[i][1]][0] + v[f[i][2]][0], ny = v[f[i][0]][1] + v[f[i][1]][1] + v[f[i][2]][1], nz = v[f[i][0]][2] + v[f[i][1]][2] + v[f[i][2]][2]; glNormal3f(nx, ny, nz); for (int k = 0; k < 3; k++) glVertex3fv(v[f[i][k]]); }
    glEnd(); glEndList();
    dlWheel = glGenLists(1); glNewList(dlWheel, GL_COMPILE);                   // cilindro de eixo X, diametro 1, largura 1
    glBegin(GL_QUAD_STRIP); for (int i = 0; i <= 12; i++) { float a = 2*PI*(float)i/12; glNormal3f(0, cosf(a), sinf(a)); glVertex3f(-0.5f, 0.5f*cosf(a), 0.5f*sinf(a)); glVertex3f(0.5f, 0.5f*cosf(a), 0.5f*sinf(a)); } glEnd();
    glBegin(GL_TRIANGLE_FAN); glNormal3f(1, 0, 0); glVertex3f(0.5f, 0, 0); for (int i = 0; i <= 12; i++) { float a = 2*PI*(float)i/12; glVertex3f(0.5f, 0.5f*cosf(a), 0.5f*sinf(a)); } glEnd();
    glBegin(GL_TRIANGLE_FAN); glNormal3f(-1, 0, 0); glVertex3f(-0.5f, 0, 0); for (int i = 12; i >= 0; i--) { float a = 2*PI*(float)i/12; glVertex3f(-0.5f, 0.5f*cosf(a), 0.5f*sinf(a)); } glEnd();
    glEndList();
}
static void Cube(float cx, float cy, float cz, float sx, float sy, float sz, float r, float g, float b) {
    glPushMatrix(); glTranslatef(cx, cy, cz); glScalef(sx, sy, sz); glColor3f(r, g, b); glCallList(dlCube); glPopMatrix();
}

void InitRender() {
    BuildCube(); BuildExtras();
    Rng r(777); for (int i = 0; i < 260; i++) { Vec3 v(r.r(-1, 1), r.r(0.05f, 1), r.r(-1, 1)); g_stars.push_back(Norm(v)); }
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glEnable(GL_NORMALIZE); glShadeModel(GL_SMOOTH);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); glHint(GL_FOG_HINT, GL_NICEST);
}

// ---------------------------------------------------------------------
//  AMBIENTE
// ---------------------------------------------------------------------
struct Env { float zen[3], hor[3], sunDir[3], moonDir[3], sunCol[3], moonCol[3], amb[3]; float day, dusk; };
static void Mix3(float* o, const float* a, const float* b, float t) { for (int i = 0; i < 3; i++) o[i] = a[i] + (b[i] - a[i])*t; }

static Env ComputeEnv(const Vec3& eye) {
    Env e; float h = TimeOfDay(), a = (h - 6.0f)/12.0f*PI; Vec3 sd = Norm(Vec3(cosf(a), sinf(a), 0.35f));
    e.sunDir[0] = sd.x; e.sunDir[1] = sd.y; e.sunDir[2] = sd.z; e.moonDir[0] = -sd.x; e.moonDir[1] = -sd.y; e.moonDir[2] = -sd.z + 0.2f;
    e.day = Smoothstep(-0.12f, 0.25f, sd.y); e.dusk = 1.0f - Smoothstep(0.0f, 0.4f, fabsf(sd.y));
    const float dz[3] = { 0.36f, 0.50f, 0.68f }, dh[3] = { 0.80f, 0.72f, 0.58f }, nz[3] = { 0.01f, 0.015f, 0.05f }, nh[3] = { 0.05f, 0.06f, 0.11f }, dk[3] = { 0.92f, 0.46f, 0.22f };
    Mix3(e.zen, nz, dz, e.day); Mix3(e.hor, nh, dh, e.day);
    float k = e.dusk*(0.25f + 0.5f*e.day); float t1[3]; Mix3(t1, e.hor, dk, k); for (int i = 0; i < 3; i++) e.hor[i] = t1[i]; Mix3(t1, e.zen, dk, e.dusk*0.15f); for (int i = 0; i < 3; i++) e.zen[i] = t1[i];
    float rf = RadAt(eye.x, eye.z); if (rf > 0.01f) { const float rg[3] = { 0.42f, 0.62f, 0.22f }; Mix3(t1, e.hor, rg, rf*0.55f*Maxf(0.35f, e.day)); for (int i = 0; i < 3; i++) e.hor[i] = t1[i]; }
    for (int i = 0; i < 3; i++) { e.sunCol[i] = (i == 0 ? 1.0f : (i == 1 ? 0.92f - 0.2f*e.dusk : 0.78f - 0.35f*e.dusk))*e.day*1.05f; }
    const float mc[3] = { 0.13f, 0.16f, 0.30f }; for (int i = 0; i < 3; i++) e.moonCol[i] = mc[i]*(1.0f - e.day);
    const float an[3] = { 0.07f, 0.08f, 0.15f }, ad[3] = { 0.36f, 0.34f, 0.32f }; Mix3(e.amb, an, ad, e.day);
    return e;
}

static void SkyVertex(const Env& e, const Vec3& dir, float elev) {
    float s = powf(Sat(sinf(Maxf(elev, 0.0f))), 0.55f); float c[3]; Mix3(c, e.hor, e.zen, s);
    float sg = Maxf(0.0f, dir.x*e.sunDir[0] + dir.y*e.sunDir[1] + dir.z*e.sunDir[2]);
    float glow = powf(sg, 10.0f)*0.55f*e.day + powf(sg, 3.0f)*0.25f*e.dusk;
    glColor3f(Sat(c[0] + glow*1.0f), Sat(c[1] + glow*0.75f), Sat(c[2] + glow*0.45f));
    glVertex3f(dir.x*600.0f, dir.y*600.0f, dir.z*600.0f);
}
static void DrawSky(const Env& e) {
    const int R = 9, S = 28;
    for (int i = -1; i < R; i++) {
        float e0 = (i < 0) ? -0.35f : (PI*0.5f)*(float)i/(float)R, e1 = (PI*0.5f)*(float)(i + 1)/(float)R;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= S; j++) {
            float a = 2*PI*(float)j/(float)S;
            Vec3 d0(cosf(e0)*cosf(a), sinf(e0), cosf(e0)*sinf(a)), d1(cosf(e1)*cosf(a), sinf(e1), cosf(e1)*sinf(a));
            SkyVertex(e, d0, e0); SkyVertex(e, d1, e1);
        }
        glEnd();
    }
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float night = 1.0f - e.day;
    if (night > 0.02f) {
        glPointSize(2.0f); glBegin(GL_POINTS);
        for (size_t i = 0; i < g_stars.size(); i++) { float tw = 0.7f + 0.3f*sinf(T()*2.0f + (float)i); glColor4f(1, 1, 0.95f, night*tw); glVertex3f(g_stars[i].x*580.0f, g_stars[i].y*580.0f, g_stars[i].z*580.0f); }
        glEnd();
    }
    struct Disc { const float* d; float rad; float r, g, b, a; } discs[2] = { { e.sunDir, 30.0f, 1.0f, 0.93f, 0.75f, 1.0f }, { e.moonDir, 20.0f, 0.88f, 0.9f, 1.0f, night } };
    for (int k = 0; k < 2; k++) {
        Vec3 d = Norm(Vec3(discs[k].d[0], discs[k].d[1], discs[k].d[2])); if (d.y < -0.15f || discs[k].a < 0.02f) continue;
        Vec3 r = Norm(Cross(d, Vec3(0, 1, 0))), u = Cross(r, d); Vec3 c = d*560.0f;
        glBegin(GL_TRIANGLE_FAN); glColor4f(discs[k].r, discs[k].g, discs[k].b, discs[k].a); glVertex3f(c.x, c.y, c.z);
        for (int i = 0; i <= 20; i++) { float a = 2*PI*(float)i/20; Vec3 p = c + r*(cosf(a)*discs[k].rad) + u*(sinf(a)*discs[k].rad); glColor4f(discs[k].r, discs[k].g, discs[k].b, discs[k].a*(k == 0 ? 0.0f : 0.9f)); glVertex3f(p.x, p.y, p.z); }
        glEnd();
        if (k == 0) {                                                                 // halo do sol
            glBegin(GL_TRIANGLE_FAN); glColor4f(1, 0.9f, 0.6f, 0.9f*e.day); glVertex3f(c.x, c.y, c.z);
            for (int i = 0; i <= 20; i++) { float a = 2*PI*(float)i/20; Vec3 p = c + r*(cosf(a)*discs[k].rad*0.8f) + u*(sinf(a)*discs[k].rad*0.8f); glColor4f(1, 0.9f, 0.6f, 0.9f*e.day); glVertex3f(p.x, p.y, p.z); }
            glEnd();
        }
    }
    glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------
//  PERSONAGENS
// ---------------------------------------------------------------------
struct Look { float skin[3], shirt[3], pants[3], hair[3]; int hat; bool plates, pack, robe; float lean, reach; };
struct Pose { float walk, amp, aim, pitch, swing, dead, crouch, hurt; int weapon; };

static void TintC(float* o, const float* c, float hurt) { o[0] = c[0] + (1.0f - c[0])*hurt*0.7f; o[1] = c[1]*(1.0f - hurt*0.6f); o[2] = c[2]*(1.0f - hurt*0.6f); }
#define COL3(c) c[0], c[1], c[2]

static void DrawWeaponModel(int w) {                                            // origem = mao; cano para -Y; +Z = embaixo
    switch (w) {
    case IT_PIPE: Cube(0, -0.38f, 0, 0.07f, 0.95f, 0.07f, 0.42f, 0.42f, 0.44f); Cube(0, 0.08f, 0, 0.09f, 0.12f, 0.09f, 0.2f, 0.2f, 0.2f); break;
    case IT_PISTOL: Cube(0, -0.15f, 0, 0.05f, 0.3f, 0.07f, 0.15f, 0.15f, 0.17f); Cube(0, -0.03f, 0.07f, 0.05f, 0.07f, 0.12f, 0.28f, 0.2f, 0.12f); break;
    case IT_SHOTGUN: Cube(0, -0.36f, -0.01f, 0.06f, 0.8f, 0.06f, 0.22f, 0.22f, 0.24f); Cube(0, -0.28f, 0.05f, 0.08f, 0.45f, 0.05f, 0.4f, 0.28f, 0.16f); Cube(0, 0.25f, 0.04f, 0.07f, 0.36f, 0.1f, 0.4f, 0.28f, 0.16f); break;
    case IT_RIFLE: Cube(0, -0.42f, 0, 0.05f, 0.98f, 0.06f, 0.2f, 0.2f, 0.22f); Cube(0, 0.28f, 0.03f, 0.07f, 0.42f, 0.11f, 0.38f, 0.26f, 0.15f); Cube(0, -0.1f, -0.07f, 0.05f, 0.22f, 0.05f, 0.1f, 0.1f, 0.12f); break;
    default: break;
    }
}

static void DrawHumanoid(const Vec3& pos, float yaw, float scale, const Look& L, const Pose& P) {
    float sk[3], sh[3], pn[3]; TintC(sk, L.skin, P.hurt); TintC(sh, L.shirt, P.hurt); TintC(pn, L.pants, P.hurt);
    glPushMatrix(); glTranslatef(pos.x, pos.y, pos.z); glRotatef(yaw*57.29578f, 0, 1, 0); glScalef(scale, scale, scale);
    if (P.dead > 0) { glTranslatef(0, 0.14f*P.dead, 0); glRotatef(90.0f*P.dead, 1, 0, 0); }
    float hipY = 0.92f - 0.30f*P.crouch, legLen = hipY, a = sinf(P.walk)*P.amp*42.0f;
    // pernas
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix(); glTranslatef(0.13f*(float)s, hipY, 0); glRotatef((float)s*a, 1, 0, 0); glTranslatef(0, -legLen*0.5f, 0);
        glPushMatrix(); glScalef(0.2f, legLen, 0.22f); glColor3f(COL3(pn)); glCallList(dlCube); glPopMatrix();
        glPopMatrix();
        glPushMatrix(); glTranslatef(0.13f*(float)s, 0.06f, -0.04f); glRotatef(0, 1, 0, 0); glScalef(0.22f, 0.12f, 0.34f); glColor3f(0.14f, 0.12f, 0.1f); glCallList(dlCube); glPopMatrix();
    }
    // tronco
    glPushMatrix(); glTranslatef(0, hipY, 0); glRotatef(-(L.lean + P.amp*6.0f + P.crouch*14.0f), 1, 0, 0);
    Cube(0, 0.34f, 0, 0.5f, 0.64f, 0.28f, COL3(sh));
    if (L.robe) Cube(0, -0.1f, 0, 0.56f, 0.5f, 0.34f, COL3(sh));
    if (L.plates) { Cube(0, 0.4f, -0.01f, 0.54f, 0.46f, 0.32f, 0.36f, 0.26f, 0.16f); Cube(-0.31f, 0.58f, 0, 0.16f, 0.12f, 0.2f, 0.3f, 0.22f, 0.14f); Cube(0.31f, 0.58f, 0, 0.16f, 0.12f, 0.2f, 0.3f, 0.22f, 0.14f); }
    if (L.pack) Cube(0, 0.36f, 0.24f, 0.4f, 0.46f, 0.2f, 0.4f, 0.3f, 0.18f);
    Cube(0, 0.82f, 0, 0.26f, 0.27f, 0.26f, COL3(sk));                           // cabeca
    Cube(-0.06f, 0.85f, -0.135f, 0.045f, 0.04f, 0.015f, 0.05f, 0.04f, 0.04f); Cube(0.06f, 0.85f, -0.135f, 0.045f, 0.04f, 0.015f, 0.05f, 0.04f, 0.04f);
    switch (L.hat) {
    case 1: Cube(0, 1.0f, 0, 0.06f, 0.14f, 0.3f, 0.75f, 0.08f, 0.08f); break;                       // moicano
    case 2: Cube(0, 0.97f, 0, 0.38f, 0.05f, 0.38f, COL3(L.hair)); Cube(0, 1.03f, 0, 0.24f, 0.1f, 0.24f, COL3(L.hair)); break;   // chapeu
    case 3: Cube(0, 0.96f, 0, 0.31f, 0.16f, 0.31f, 0.42f, 0.44f, 0.46f); break;                     // capacete
    case 4: Cube(0, 0.97f, 0, 0.28f, 0.1f, 0.28f, 0.85f, 0.85f, 0.85f); Cube(0, 0.72f, -0.12f, 0.2f, 0.2f, 0.06f, 0.88f, 0.88f, 0.88f); break;   // cabelo/barba brancos
    case 5: Cube(0, 0.96f, 0.0f, 0.28f, 0.1f, 0.28f, COL3(L.hair)); break;                          // cabelo
    default: break;
    }
    // bracos
    bool gun = (P.weapon == IT_PISTOL || P.weapon == IT_SHOTGUN || P.weapon == IT_RIFLE);
    float reach = L.reach, pitchDeg = P.pitch*57.29578f;
    for (int s = -1; s <= 1; s += 2) {                                          // s=+1: direito do personagem? (x+ local = direita visual de quem olha de costas)
        float ang = -(float)s*sinf(P.walk)*P.amp*34.0f, angZ = (float)s*4.0f;
        bool right = (s == 1);
        if (reach > 0.5f) ang = 62.0f + (float)(-s)*sinf(P.walk)*8.0f;          // cinzento: bracos esticados
        else if (gun) { float low = 38.0f; float up = 90.0f + pitchDeg; ang = Lerpf(right ? low : low + 8.0f, up, P.aim); if (!right) angZ = Lerpf(angZ, -18.0f, P.aim); }
        else if (right && P.weapon == IT_PIPE) { ang = P.swing > 0 ? (-70.0f + 190.0f*P.swing) : 18.0f + ang*0.4f; }
        glPushMatrix(); glTranslatef(0.31f*(float)s, 0.58f, 0); glRotatef(ang, 1, 0, 0); glRotatef(angZ*(float)s, 0, 0, 1);
        glPushMatrix(); glTranslatef(0, -0.3f, 0); glScalef(0.13f, 0.6f, 0.13f); glColor3f(COL3(sh)); glCallList(dlCube); glPopMatrix();
        Cube(0, -0.64f, 0, 0.12f, 0.12f, 0.12f, COL3(sk));
        if (right && P.weapon != IT_NONE) { glTranslatef(0, -0.62f, 0); DrawWeaponModel(P.weapon); }
        glPopMatrix();
    }
    glPopMatrix();
    glPopMatrix();
}

static void DrawDog(const Vec3& pos, float yaw, float scale, float walk, float amp, float dead, float hurt, bool angry) {
    float fur[3] = { 0.45f, 0.34f, 0.22f }, f2[3]; TintC(f2, fur, hurt);
    glPushMatrix(); glTranslatef(pos.x, pos.y, pos.z); glRotatef(yaw*57.29578f, 0, 1, 0); glScalef(scale, scale, scale);
    if (dead > 0) { glTranslatef(0, 0.2f*dead, 0); glRotatef(90.0f*dead, 0, 0, 1); }
    Cube(0, 0.58f, 0, 0.34f, 0.34f, 0.85f, COL3(f2));
    Cube(0, 0.74f, -0.52f, 0.27f, 0.27f, 0.3f, COL3(f2)); Cube(0, 0.68f, -0.74f, 0.14f, 0.12f, 0.2f, f2[0]*0.7f, f2[1]*0.7f, f2[2]*0.7f);
    Cube(-0.09f, 0.9f, -0.5f, 0.06f, 0.1f, 0.06f, f2[0]*0.6f, f2[1]*0.6f, f2[2]*0.6f); Cube(0.09f, 0.9f, -0.5f, 0.06f, 0.1f, 0.06f, f2[0]*0.6f, f2[1]*0.6f, f2[2]*0.6f);
    if (angry) { Cube(-0.07f, 0.78f, -0.675f, 0.04f, 0.04f, 0.02f, 1.0f, 0.15f, 0.1f); Cube(0.07f, 0.78f, -0.675f, 0.04f, 0.04f, 0.02f, 1.0f, 0.15f, 0.1f); }
    glPushMatrix(); glTranslatef(0, 0.66f, 0.42f); glRotatef(-35.0f + sinf(walk*0.5f)*amp*20.0f, 1, 0, 0); glTranslatef(0, 0, 0.15f); glScalef(0.07f, 0.07f, 0.34f); glColor3f(COL3(f2)); glCallList(dlCube); glPopMatrix();
    const float lx[4] = { -0.12f, 0.12f, -0.12f, 0.12f }, lz[4] = { -0.3f, -0.3f, 0.3f, 0.3f };
    for (int i = 0; i < 4; i++) {
        float ph = walk + ((i == 0 || i == 3) ? 0.0f : PI), ang = sinf(ph)*amp*42.0f;
        glPushMatrix(); glTranslatef(lx[i], 0.42f, lz[i]); glRotatef(ang, 1, 0, 0); glTranslatef(0, -0.21f, 0); glScalef(0.1f, 0.42f, 0.1f); glColor3f(f2[0]*0.9f, f2[1]*0.9f, f2[2]*0.9f); glCallList(dlCube); glPopMatrix();
    }
    glPopMatrix();
}

static void SetLook(Look& L, int kind) {
    memset(&L, 0, sizeof(L)); const float skin[3] = { 0.86f, 0.68f, 0.54f }; memcpy(L.skin, skin, sizeof(skin));
    switch (kind) {
    case NK_SETTLER: { const float s[3] = { 0.55f, 0.46f, 0.32f }, p[3] = { 0.30f, 0.28f, 0.26f }, h[3] = { 0.5f, 0.4f, 0.25f }; memcpy(L.shirt, s, 12); memcpy(L.pants, p, 12); memcpy(L.hair, h, 12); L.hat = 2; break; }
    case NK_MERCHANT: { const float s[3] = { 0.55f, 0.2f, 0.18f }, p[3] = { 0.25f, 0.22f, 0.2f }, h[3] = { 0.2f, 0.15f, 0.1f }; memcpy(L.shirt, s, 12); memcpy(L.pants, p, 12); memcpy(L.hair, h, 12); L.hat = 5; L.pack = true; break; }
    case NK_RAIDER: { const float s[3] = { 0.16f, 0.14f, 0.14f }, p[3] = { 0.22f, 0.2f, 0.18f }, h[3] = { 0.1f, 0.1f, 0.1f }; memcpy(L.shirt, s, 12); memcpy(L.pants, p, 12); memcpy(L.hair, h, 12); L.hat = 1; L.plates = true; break; }
    case NK_ASHEN: { const float k[3] = { 0.72f, 0.67f, 0.58f }, s[3] = { 0.52f, 0.47f, 0.40f }, p[3] = { 0.26f, 0.24f, 0.2f }; memcpy(L.skin, k, 12); memcpy(L.shirt, s, 12); memcpy(L.pants, p, 12); L.lean = 24.0f; L.reach = 1.0f; break; }
    case NK_GUARD: { const float s[3] = { 0.40f, 0.42f, 0.44f }, p[3] = { 0.24f, 0.26f, 0.28f }; memcpy(L.shirt, s, 12); memcpy(L.pants, p, 12); L.hat = 3; L.plates = true; break; }
    case NK_BRUTE: { const float k[3] = { 0.58f, 0.52f, 0.46f }, p[3] = { 0.24f, 0.2f, 0.17f }; memcpy(L.skin, k, 12); memcpy(L.shirt, k, 12); memcpy(L.pants, p, 12); L.lean = 20.0f; L.reach = 1.0f; break; }
    case NK_MECHANIC: { const float s[3] = { 0.27f, 0.36f, 0.50f }, p[3] = { 0.24f, 0.30f, 0.42f }, h[3] = { 0.55f, 0.18f, 0.12f }; memcpy(L.shirt, s, 12); memcpy(L.pants, p, 12); memcpy(L.hair, h, 12); L.hat = 2; L.pack = true; break; }
    case NK_ELDER: { const float s[3] = { 0.42f, 0.36f, 0.5f }, p[3] = { 0.3f, 0.26f, 0.3f }; memcpy(L.shirt, s, 12); memcpy(L.pants, p, 12); L.hat = 4; L.robe = true; break; }
    }
}


static void DrawCrawler(const Vec3& pos, float yaw, float walk, float amp, float dead, float hurt, bool angry) {
    float c0[3] = { 0.16f, 0.2f, 0.14f }, c[3]; TintC(c, c0, hurt);
    glPushMatrix(); glTranslatef(pos.x, pos.y, pos.z); glRotatef(yaw*57.29578f, 0, 1, 0);
    if (dead > 0) { glTranslatef(0, 0.12f*dead, 0); glRotatef(180.0f*dead, 0, 0, 1); glTranslatef(0, -0.3f*dead, 0); }
    Cube(0, 0.32f, 0, 0.46f, 0.2f, 0.95f, COL3(c)); Cube(0, 0.36f, -0.55f, 0.3f, 0.18f, 0.3f, c[0]*1.3f, c[1]*1.3f, c[2]*1.3f);
    Cube(0, 0.4f, 0.55f, 0.08f, 0.08f, 0.5f, c[0]*0.7f, c[1]*0.7f, c[2]*0.7f);
    float eg = angry ? 1.0f : 0.5f; Cube(-0.07f, 0.4f, -0.71f, 0.05f, 0.05f, 0.03f, eg, eg*0.9f, 0.1f); Cube(0.07f, 0.4f, -0.71f, 0.05f, 0.05f, 0.03f, eg, eg*0.9f, 0.1f);
    for (int i = 0; i < 6; i++) {
        float side = (i & 1) ? 1.0f : -1.0f, z = -0.3f + 0.3f*(float)(i/2), ph = walk*1.4f + (float)i*1.2f, ang = sinf(ph)*amp*35.0f;
        glPushMatrix(); glTranslatef(0.2f*side, 0.3f, z); glRotatef(side*55.0f, 0, 0, 1); glRotatef(ang, 1, 0, 0); glTranslatef(0, -0.22f, 0); glScalef(0.06f, 0.46f, 0.06f); glColor3f(c[0]*0.8f, c[1]*0.8f, c[2]*0.8f); glCallList(dlCube); glPopMatrix();
    }
    glPopMatrix();
}

static void Wheel(float x, float y, float z, float r, float w, float spin, float steer, bool burnt) {
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(-steer*57.29578f, 0, 1, 0); glRotatef(-spin*57.29578f, 1, 0, 0);
    glPushMatrix(); glScalef(w, r*2.0f, r*2.0f); glColor3f(burnt ? 0.05f : 0.09f, burnt ? 0.05f : 0.09f, burnt ? 0.05f : 0.1f); glCallList(dlWheel); glPopMatrix();
    glPushMatrix(); glTranslatef(w*0.04f*(x > 0 ? 1.0f : -1.0f), 0, 0); glScalef(w*1.02f, r*0.9f, r*0.9f); glColor3f(0.5f, 0.5f, 0.52f); glCallList(dlWheel); glPopMatrix();
    glPushMatrix(); glScalef(w*1.1f, r*0.12f, r*1.7f); glColor3f(0.3f, 0.3f, 0.32f); glCallList(dlCube); glPopMatrix();      // "raio" que mostra o giro
    glPopMatrix();
}

static void DrawVehicle(const Vehicle& v, bool lightsOn, bool driverVisible) {
    bool burnt = v.state == 2; float tm = T();
    glPushMatrix(); glTranslatef(v.pos.x, v.pos.y, v.pos.z); glRotatef(v.yaw*57.29578f, 0, 1, 0); glRotatef(v.pitch*57.29578f, 1, 0, 0); glRotatef(v.roll*57.29578f, 0, 0, 1);
    if (v.type == VT_BUGGY) {
        float body[3] = { 0.78f, 0.38f, 0.14f }; if (burnt) { body[0] = body[1] = body[2] = 0.1f; }
        Cube(0, 0.62f, 0.1f, 1.7f, 0.35f, 3.5f, COL3(body)); Cube(0, 0.82f, -1.5f, 1.5f, 0.26f, 1.1f, body[0]*1.1f, body[1]*1.1f, body[2]*1.1f);
        Cube(0, 1.05f, 1.15f, 1.2f, 0.7f, 0.9f, 0.22f, 0.22f, 0.24f); Cube(-0.38f, 0.98f, -0.1f, 0.5f, 0.1f, 0.5f, 0.15f, 0.15f, 0.15f); Cube(0.38f, 0.98f, -0.1f, 0.5f, 0.1f, 0.5f, 0.15f, 0.15f, 0.15f);
        for (int i = 0; i < 4; i++) { float px = (i & 1) ? 0.78f : -0.78f, pz = (i & 2) ? 0.7f : -0.55f; Cube(px, 1.25f, pz, 0.08f, 1.0f, 0.08f, 0.72f, 0.72f, 0.74f); }
        Cube(0, 1.78f, 0.075f, 1.64f, 0.08f, 0.08f, 0.72f, 0.72f, 0.74f); Cube(0, 1.78f, -0.55f, 1.64f, 0.08f, 0.08f, 0.72f, 0.72f, 0.74f); Cube(0, 1.78f, 0.7f, 1.64f, 0.08f, 0.08f, 0.72f, 0.72f, 0.74f);
        Cube(-0.45f, 1.0f, 1.65f, 0.12f, 0.12f, 0.5f, 0.3f, 0.3f, 0.3f); Cube(0.45f, 1.0f, 1.65f, 0.12f, 0.12f, 0.5f, 0.3f, 0.3f, 0.3f);
        Wheel(-1.0f, 0.48f, -1.25f, 0.48f, 0.4f, v.wheelRot, v.steer, burnt); Wheel(1.0f, 0.48f, -1.25f, 0.48f, 0.4f, v.wheelRot, v.steer, burnt);
        Wheel(-1.05f, 0.5f, 1.25f, 0.5f, 0.46f, v.wheelRot, 0, burnt); Wheel(1.05f, 0.5f, 1.25f, 0.5f, 0.46f, v.wheelRot, 0, burnt);
        if (driverVisible) { Cube(-0.38f, 1.25f, -0.1f, 0.4f, 0.55f, 0.3f, 0.36f, 0.4f, 0.26f); Cube(-0.38f, 1.7f, -0.1f, 0.24f, 0.24f, 0.24f, 0.86f, 0.68f, 0.54f); }
        if (lightsOn || !burnt) { glDisable(GL_LIGHTING); float k = (lightsOn && !burnt) ? 1.0f : 0.55f; Cube(-0.55f, 0.95f, -2.08f, 0.26f, 0.2f, 0.06f, k, k*0.95f, k*0.7f); Cube(0.55f, 0.95f, -2.08f, 0.26f, 0.2f, 0.06f, k, k*0.95f, k*0.7f); if (v.throttle < -0.05f || v.speed < -0.5f) Cube(0, 0.8f, 1.9f, 0.5f, 0.12f, 0.05f, 1, 0.1f, 0.1f); glEnable(GL_LIGHTING); }
    } else {
        float body[3] = { 0.5f, 0.27f, 0.2f }; if (burnt) { body[0] = body[1] = body[2] = 0.1f; }
        Cube(0, 0.7f, 0.0f, 2.0f, 0.4f, 4.9f, 0.2f, 0.2f, 0.22f);
        Cube(0, 1.65f, -0.55f, 1.9f, 1.3f, 1.7f, COL3(body)); Cube(0, 1.0f, -2.0f, 1.85f, 0.7f, 1.3f, body[0]*1.1f, body[1]*1.1f, body[2]*1.1f);
        Cube(0, 1.95f, -0.62f, 1.92f, 0.5f, 1.4f, 0.1f, 0.14f, 0.16f);
        Cube(0, 1.0f, 1.45f, 2.0f, 0.5f, 2.0f, body[0]*0.85f, body[1]*0.85f, body[2]*0.85f); Cube(-0.98f, 1.5f, 1.45f, 0.06f, 0.5f, 2.0f, COL3(body)); Cube(0.98f, 1.5f, 1.45f, 0.06f, 0.5f, 2.0f, COL3(body)); Cube(0, 1.5f, 2.43f, 2.0f, 0.5f, 0.06f, COL3(body));
        Cube(0, 0.65f, -2.7f, 2.0f, 0.22f, 0.2f, 0.25f, 0.25f, 0.27f);
        Wheel(-1.05f, 0.5f, -1.6f, 0.5f, 0.42f, v.wheelRot, v.steer, burnt); Wheel(1.05f, 0.5f, -1.6f, 0.5f, 0.42f, v.wheelRot, v.steer, burnt);
        Wheel(-1.05f, 0.5f, 1.6f, 0.52f, 0.46f, v.wheelRot, 0, burnt); Wheel(1.05f, 0.5f, 1.6f, 0.52f, 0.46f, v.wheelRot, 0, burnt);
        if (driverVisible) { Cube(-0.42f, 1.7f, -0.6f, 0.3f, 0.5f, 0.3f, 0.36f, 0.4f, 0.26f); Cube(-0.42f, 2.1f, -0.6f, 0.24f, 0.24f, 0.24f, 0.86f, 0.68f, 0.54f); }
        if (!burnt) { glDisable(GL_LIGHTING); float k = lightsOn ? 1.0f : 0.5f; Cube(-0.65f, 1.05f, -2.66f, 0.3f, 0.22f, 0.06f, k, k*0.95f, k*0.7f); Cube(0.65f, 1.05f, -2.66f, 0.3f, 0.22f, 0.06f, k, k*0.95f, k*0.7f); glEnable(GL_LIGHTING); }
    }
    glPopMatrix();
    if (burnt && v.smoke < 90.0f) {
        glDisable(GL_LIGHTING);
        for (int i = 0; i < 3; i++) { float fl = 0.7f + 0.3f*sinf(tm*13.0f + (float)i*2.0f); Cube(v.pos.x + (float)(i - 1)*0.6f, v.pos.y + 1.2f + 0.3f*fl, v.pos.z + 0.3f*(float)(i & 1), 0.5f, 0.9f*fl, 0.5f, 1.0f, 0.5f + 0.2f*fl, 0.08f); }
        glEnable(GL_LIGHTING);
    }
}

static void DrawFissure(const Anomaly& a, const Vec3& eye, float night) {
    float d = Len2D(Vec3(a.x, 0, a.z) - Vec3(eye.x, 0, eye.z)); float vis = Sat(1.0f - (d - 20.0f)/55.0f); vis = Maxf(vis, d < 20.0f ? 1.0f : 0.0f); if (vis < 0.02f) return;
    float tm = T(), pulse = 0.6f + 0.4f*sinf(tm*4.0f + a.x), glow = (0.5f + 0.5f*night)*vis; bool el = a.type == 0;
    float cr = el ? 0.35f : 1.0f, cg = el ? 0.85f : 0.5f, cb = el ? 1.0f : 0.15f;
    glPushMatrix(); glTranslatef(a.x, a.y + 1.1f, a.z); glRotatef(tm*50.0f, 0, 1, 0);
    glColor4f(cr, cg, cb, 0.10f*glow*pulse + 0.03f); glPushMatrix(); glScalef(a.r, a.r*0.9f, a.r); glCallList(dlOcta); glPopMatrix();
    glRotatef(35.0f, 1, 0, 1); glColor4f(cr, cg, cb, 0.20f*glow*pulse); glPushMatrix(); glScalef(a.r*0.55f, a.r*0.55f, a.r*0.55f); glCallList(dlOcta); glPopMatrix();
    glPopMatrix();
    glColor4f(cr, cg, cb, 0.35f*vis); glBegin(GL_LINE_LOOP); for (int i = 0; i < 24; i++) { float an = 2*PI*(float)i/24; glVertex3f(a.x + cosf(an)*a.r, a.y + 0.06f, a.z + sinf(an)*a.r); } glEnd();
    if (fmodf(tm*3.0f + a.z, 1.0f) < 0.35f && d < 45.0f) {                                      // faiscas
        glBegin(GL_LINES); float h = (float)((int)(tm*6.0f)); for (int i = 0; i < 4; i++) { float ang = Hash01((int)a.x + i, (int)h, 5)*2*PI, ln = a.r*(0.6f + 0.6f*Hash01(i, (int)h, 9)); glColor4f(cr, cg, cb, 0.9f*vis); glVertex3f(a.x, a.y + 1.0f, a.z); glColor4f(1, 1, 1, 0.0f); glVertex3f(a.x + cosf(ang)*ln, a.y + 0.4f + 1.8f*Hash01(i, (int)h, 3), a.z + sinf(ang)*ln); } glEnd();
    }
}

static void DrawGarden(const Chunk* c) {
    float tm = T(); float x0 = c->cenx - 10.0f, z0 = c->cenz - 7.0f; float y = TerrainH(x0 + 3.5f, z0 + 2.5f);
    Cube(x0 + 3.5f, y + 0.02f, z0 + 2.5f, 8.0f, 0.08f, 6.0f, 0.22f, 0.15f, 0.1f);
    for (int j = 0; j < 4; j++) for (int i = 0; i < 8; i++) {
        float px = x0 + 0.6f + (float)i*1.0f, pz = z0 + 0.6f + (float)j*1.4f, h = 0.55f + 0.25f*sinf((float)(i*3 + j*5)) + 0.04f*sinf(tm*2.0f + (float)i), py = TerrainH(px, pz);
        Cube(px, py + h*0.5f, pz, 0.07f, h, 0.07f, 0.28f, 0.6f, 0.2f); Cube(px - 0.12f, py + h*0.7f, pz, 0.22f, 0.05f, 0.1f, 0.34f, 0.7f, 0.24f); Cube(px + 0.12f, py + h*0.55f, pz, 0.22f, 0.05f, 0.1f, 0.34f, 0.7f, 0.24f);
        if ((i + j) % 3 == 0) Cube(px, py + h + 0.06f, pz, 0.14f, 0.14f, 0.14f, 0.95f, 0.75f, 0.2f);
    }
}


// ---------------------------------------------------------------------
//  RAGDOLL: cada corpo rigido (osso) e desenhado como uma malha de capsula
// ---------------------------------------------------------------------
static void BoneBox(const Vec3& a, const Vec3& b, const Vec3& side, float w, float d, const float* c, float extend = 0.0f) {
    Vec3 y = b - a; float len = Len(y); if (len < 1e-4f) return; y = y*(1.0f/len);
    Vec3 x = side - y*Dot(side, y); float xl = Len(x); if (xl < 1e-4f) { x = Cross(y, Vec3(0, 0, 1)); xl = Len(x); if (xl < 1e-4f) x = Vec3(1, 0, 0); } x = Norm(x);
    Vec3 z = Cross(x, y), m = (a + b)*0.5f; float M[16] = { x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, m.x, m.y, m.z, 1 };
    glPushMatrix(); glMultMatrixf(M); glScalef(w, len + extend, d); glColor3f(c[0], c[1], c[2]); glCallList(dlCube); glPopMatrix();
}
// malha de capsula (eixo local Y): hemisferio + cilindro + hemisferio, com normais suaves
static void CapsuleMesh(float r, float hl, float sx, float sz) {
    const int SL = 10, ST = 4, N = 2*(ST + 1); float py[N], pr[N], pny[N], pnr[N]; int n = 0;
    for (int k = 0; k <= ST; k++) { float a = (PI*0.5f)*(1.0f - (float)k/(float)ST); py[n] = hl + r*sinf(a); pr[n] = r*cosf(a); pny[n] = sinf(a); pnr[n] = cosf(a); n++; }
    for (int k = 0; k <= ST; k++) { float a = -(PI*0.5f)*((float)k/(float)ST); py[n] = -hl + r*sinf(a); pr[n] = r*cosf(a); pny[n] = sinf(a); pnr[n] = cosf(a); n++; }
    glPushMatrix(); glScalef(sx, 1.0f, sz);
    for (int i = 0; i < n - 1; i++) {
        glBegin(GL_QUAD_STRIP);
        for (int s = 0; s <= SL; s++) { float th = 2*PI*(float)s/(float)SL, c = cosf(th), sn = sinf(th);
            glNormal3f(pnr[i]*c, pny[i], pnr[i]*sn); glVertex3f(pr[i]*c, py[i], pr[i]*sn); glNormal3f(pnr[i + 1]*c, pny[i + 1], pnr[i + 1]*sn); glVertex3f(pr[i + 1]*c, py[i + 1], pr[i + 1]*sn); }
        glEnd();
    }
    glPopMatrix();
}
static void DrawBody(const RBody& B, const float* col, float grow = 1.0f) {                // corpo rigido -> malha de capsula orientada
    Vec3 x = Rotate(B.q, Vec3(1, 0, 0)), y = Rotate(B.q, Vec3(0, 1, 0)), z = Rotate(B.q, Vec3(0, 0, 1));
    float M[16] = { x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, B.x.x, B.x.y, B.x.z, 1 };
    glPushMatrix(); glMultMatrixf(M); glColor3f(col[0], col[1], col[2]); CapsuleMesh(B.r*grow, B.hl, B.sx, B.sz); glPopMatrix();
}
static void DrawRagdoll(const Ragdoll& R, int kind) {
    float s = R.scale;
    if (R.type == RAG_HUMAN) {
        Look L; SetLook(L, kind); const float dark[3] = { 0.14f, 0.12f, 0.10f }, brown[3] = { 0.36f, 0.26f, 0.16f }, eye[3] = { 0.05f, 0.04f, 0.04f };
        for (int i = 0; i < R.nb; i++) {
            const RBody& B = R.b[i]; const float* c = L.shirt;
            switch (B.part) { case RP_PELVIS: case RP_THIGH: case RP_SHIN: c = L.pants; break; case RP_HEAD: c = L.skin; break; case RP_FARM: c = L.skin; break; case RP_FOOT: c = dark; break; default: c = L.shirt; }
            DrawBody(B, c);
            if (B.part == RP_TORSO && L.plates) DrawBody(B, brown, 1.12f);
            if (B.part == RP_TORSO && L.pack) { Vec3 back = Rotate(B.q, R.Dw(Vec3(0, 0, 1)))*(0.22f*s), up = Rotate(B.q, Vec3(0, 1, 0)), right = Rotate(B.q, R.Dw(Vec3(1, 0, 0))); BoneBox(B.x + back - up*(0.15f*s), B.x + back + up*(0.12f*s), right, 0.38f*s, 0.2f*s, brown); }
            if (B.part == RP_HEAD) {
                Vec3 up = Rotate(B.q, Vec3(0, 1, 0)), right = Rotate(B.q, R.Dw(Vec3(1, 0, 0))), fwd = Rotate(B.q, R.Dw(Vec3(0, 0, -1))), hp = B.x + up*(0.02f*s);
                BoneBox(hp + fwd*(0.125f*s) - right*(0.06f*s) + up*(0.02f*s), hp + fwd*(0.135f*s) - right*(0.06f*s) + up*(0.04f*s), right, 0.045f*s, 0.04f*s, eye);
                BoneBox(hp + fwd*(0.125f*s) + right*(0.06f*s) + up*(0.02f*s), hp + fwd*(0.135f*s) + right*(0.06f*s) + up*(0.04f*s), right, 0.045f*s, 0.04f*s, eye);
                switch (L.hat) {
                case 1: BoneBox(hp + up*(0.11f*s), hp + up*(0.25f*s), right, 0.06f*s, 0.3f*s, L.hair); break;
                case 2: BoneBox(hp + up*(0.11f*s), hp + up*(0.16f*s), right, 0.38f*s, 0.38f*s, L.hair); BoneBox(hp + up*(0.16f*s), hp + up*(0.26f*s), right, 0.24f*s, 0.24f*s, L.hair); break;
                case 3: BoneBox(hp + up*(0.09f*s), hp + up*(0.22f*s), right, 0.31f*s, 0.31f*s, dark); break;
                case 4: case 5: BoneBox(hp + up*(0.09f*s), hp + up*(0.18f*s), right, 0.28f*s, 0.28f*s, L.hat == 4 ? L.skin : L.hair); break;
                default: break;
                }
            }
        }
    } else {
        const bool crawler = (kind == NK_CRAWLER); const float fur[3] = { crawler ? 0.16f : 0.45f, crawler ? 0.20f : 0.34f, crawler ? 0.14f : 0.22f }, fur2[3] = { fur[0]*0.78f, fur[1]*0.78f, fur[2]*0.78f };
        for (int i = 0; i < R.nb; i++) DrawBody(R.b[i], (R.b[i].part == RP_LEG || R.b[i].part == RP_TAIL) ? fur2 : fur);
    }
}

static void DrawShadow(const Vec3& p, float r, float a) {
    glBegin(GL_TRIANGLE_FAN); glColor4f(0, 0, 0, a); glVertex3f(p.x, p.y + 0.04f, p.z);
    for (int i = 0; i <= 12; i++) { float an = 2*PI*(float)i/12; glColor4f(0, 0, 0, 0); glVertex3f(p.x + cosf(an)*r, TerrainH(p.x + cosf(an)*r, p.z + sinf(an)*r) + 0.04f, p.z + sinf(an)*r); }
    glEnd();
}

static Vec3 MuzzleWorld() {
    const Player& p = G.p;
    if (G.cam.first) return Vec3(p.pos.x, p.pos.y + 1.55f, p.pos.z) + Facing(p.yaw)*0.7f;
    return Vec3(p.pos.x, p.pos.y + 1.4f, p.pos.z) + Facing(p.bodyYaw)*0.8f + Vec3(cosf(p.bodyYaw), 0, -sinf(p.bodyYaw))*0.3f;
}

// ---------------------------------------------------------------------
//  CAMERA
// ---------------------------------------------------------------------
static void ApplyCamera(int W, int H) {
    const Camera& c = G.cam; float aspect = (float)W/(float)Maxi(H, 1);
    float hf = c.fov*PI/180.0f, vf = 2.0f*atanf(tanf(hf*0.5f)/aspect); float nr = 0.1f, fr = 1500.0f, top = nr*tanf(vf*0.5f), right = top*aspect;
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glFrustum(-right, right, -top, top, nr, fr);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    Vec3 f = Norm(c.fwd), r = Norm(Cross(f, Vec3(0, 1, 0))), u = Cross(r, f);
    float m[16] = { r.x, u.x, -f.x, 0, r.y, u.y, -f.y, 0, r.z, u.z, -f.z, 0, 0, 0, 0, 1 };
    glMultMatrixf(m);
}

// ---------------------------------------------------------------------
//  QUADRO
// ---------------------------------------------------------------------
void RenderFrame(int W, int H) {
    const Camera& cam = G.cam; Env env = ComputeEnv(cam.eye);
    float viewDist = (float)(G.state == GS_MENU ? 3 : G.set.viewR)*CHUNK_SIZE;
    glViewport(0, 0, W, H);
    glClearColor(env.hor[0], env.hor[1], env.hor[2], 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    ApplyCamera(W, H);

    // ---- ceu (sem translacao da camera) ----
    glDepthMask(GL_FALSE); glDisable(GL_DEPTH_TEST); DrawSky(env); glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);

    glTranslatef(-cam.eye.x, -cam.eye.y, -cam.eye.z);

    // ---- luz + neblina ----
    glEnable(GL_LIGHTING);
    GLfloat amb[4] = { env.amb[0], env.amb[1], env.amb[2], 1 }; glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    GLfloat l0p[4] = { env.sunDir[0], env.sunDir[1], env.sunDir[2], 0 }, l0d[4] = { env.sunCol[0], env.sunCol[1], env.sunCol[2], 1 }, zero[4] = { 0, 0, 0, 1 };
    GLfloat l1p[4] = { env.moonDir[0], env.moonDir[1], env.moonDir[2], 0 }, l1d[4] = { env.moonCol[0], env.moonCol[1], env.moonCol[2], 1 };
    glEnable(GL_LIGHT0); glLightfv(GL_LIGHT0, GL_POSITION, l0p); glLightfv(GL_LIGHT0, GL_DIFFUSE, l0d); glLightfv(GL_LIGHT0, GL_AMBIENT, zero); glLightfv(GL_LIGHT0, GL_SPECULAR, zero);
    glEnable(GL_LIGHT1); glLightfv(GL_LIGHT1, GL_POSITION, l1p); glLightfv(GL_LIGHT1, GL_DIFFUSE, l1d); glLightfv(GL_LIGHT1, GL_AMBIENT, zero); glLightfv(GL_LIGHT1, GL_SPECULAR, zero);
    if (G.drive && env.day < 0.5f) {
        const Vehicle& v = *G.drive; Vec3 f = Facing(v.yaw); GLfloat lp[4] = { v.pos.x + f.x*2.0f, v.pos.y + 0.95f, v.pos.z + f.z*2.0f, 1 }, ld[3] = { f.x, -0.16f, f.z }, dif[4] = { 1.5f, 1.4f, 1.1f, 1 };
        glEnable(GL_LIGHT2); glLightfv(GL_LIGHT2, GL_POSITION, lp); glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, ld); glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, 38.0f); glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, 6.0f);
        glLightfv(GL_LIGHT2, GL_DIFFUSE, dif); glLightfv(GL_LIGHT2, GL_AMBIENT, zero); glLightfv(GL_LIGHT2, GL_SPECULAR, zero); glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 1.0f); glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.025f);
    } else glDisable(GL_LIGHT2);
    glEnable(GL_FOG); glFogi(GL_FOG_MODE, GL_LINEAR); glFogf(GL_FOG_START, viewDist*0.30f); glFogf(GL_FOG_END, viewDist*0.97f);
    GLfloat fc[4] = { env.hor[0], env.hor[1], env.hor[2], 1 }; glFogfv(GL_FOG_COLOR, fc);

    // ---- chunks ----
    float aspect = (float)W/(float)Maxi(H, 1), tanH = tanf(cam.fov*PI/360.0f), tanV = tanH/aspect, halfDiag = atanf(sqrtf(tanH*tanH + tanV*tanV));
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    std::vector<const Chunk*> vis; vis.reserve(W == 0 ? 0 : 160);
    for (std::unordered_map<u64, Chunk*>::const_iterator it = ::W.map.begin(); it != ::W.map.end(); ++it) {
        const Chunk* c = it->second; if (c->state.load() != 1 || c->vcount == 0) continue;
        Vec3 v(c->cenx - cam.eye.x, (c->minY + c->maxY)*0.5f - cam.eye.y, c->cenz - cam.eye.z); float d = Len(v);
        if (d > viewDist + 70.0f) continue;
        if (d > 110.0f) { float ca = Dot(v*(1.0f/d), cam.fwd); float ang = acosf(Clampf(ca, -1, 1)) - asinf(Minf(1.0f, 60.0f/d)); if (ang > halfDiag + 0.05f) continue; }
        glVertexPointer(3, GL_FLOAT, 0, c->pos.data()); glNormalPointer(GL_FLOAT, 0, c->nrm.data()); glColorPointer(3, GL_UNSIGNED_BYTE, 0, c->col.data());
        glDrawArrays(GL_TRIANGLES, 0, c->vcount); vis.push_back(c);
    }
    glDisableClientState(GL_VERTEX_ARRAY); glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_COLOR_ARRAY);

    float tm = T();
    // ---- objetos dinamicos dos chunks: containers, fogueiras, farol ----
    for (size_t ci = 0; ci < vis.size(); ci++) {
        const Chunk* c = vis[ci];
        for (size_t i = 0; i < c->conts.size(); i++) {
            const Container& k = c->conts[i]; if (Sqr(k.x - cam.eye.x) + Sqr(k.z - cam.eye.z) > Sqr(90.0f)) continue;
            bool looted = (k.kind != CK_TERMINAL) && IsContainerLooted(k.id); float dm = looted ? 0.55f : 1.0f; bool tgt = (G.tgt.type == 2 && G.tgt.id == k.id);
            glPushMatrix(); glTranslatef(k.x, k.y, k.z); glRotatef(k.yaw*57.29578f, 0, 1, 0);
            switch (k.kind) {
            case CK_CRATE: Cube(0, 0.4f, 0, 0.85f, 0.8f, 0.85f, 0.5f*dm, 0.36f*dm, 0.2f*dm); Cube(0, 0.4f, 0, 0.9f, 0.1f, 0.9f, 0.34f*dm, 0.24f*dm, 0.12f*dm); break;
            case CK_LOCKER: Cube(0, 0.95f, 0, 0.6f, 1.9f, 0.5f, 0.32f*dm, 0.42f*dm, 0.5f*dm); Cube(0.0f, 1.1f, -0.26f, 0.5f, 0.04f, 0.02f, 0.1f, 0.1f, 0.1f); break;
            case CK_SAFE: Cube(0, 0.4f, 0, 0.8f, 0.8f, 0.7f, 0.2f*dm, 0.2f*dm, 0.22f*dm); Cube(0, 0.45f, -0.36f, 0.2f, 0.2f, 0.03f, 0.75f*dm, 0.7f*dm, 0.3f*dm); break;
            case CK_BODY: glRotatef(90, 0, 1, 0); Cube(0, 0.1f, 0, 1.7f, 0.2f, 0.5f, 0.35f*dm, 0.28f*dm, 0.22f*dm); Cube(0.95f, 0.14f, 0, 0.26f, 0.26f, 0.26f, 0.75f*dm, 0.6f*dm, 0.48f*dm); break;
            case CK_ARTIFACT: {
                if (looted) break; float bob = 0.12f*sinf(tm*2.4f + k.x), r = (k.id % 3 == 0) ? 1.0f : ((k.id % 3 == 1) ? 1.0f : 1.0f), gcol[3] = { 1.0f, 0.3f, 0.25f };
                if (k.id % 3 == 1) { gcol[0] = 1.0f; gcol[1] = 0.9f; gcol[2] = 0.3f; } else if (k.id % 3 == 2) { gcol[0] = 1.0f; gcol[1] = 0.65f; gcol[2] = 0.15f; } (void)r;
                glDisable(GL_LIGHTING); glPushMatrix(); glTranslatef(0, bob, 0); glRotatef(tm*70.0f, 0, 1, 0); glColor3f(gcol[0], gcol[1], gcol[2]); glPushMatrix(); glScalef(0.18f, 0.42f, 0.18f); glCallList(dlOcta); glPopMatrix();
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE); glColor4f(gcol[0], gcol[1], gcol[2], 0.25f); glPushMatrix(); glScalef(0.5f, 0.8f, 0.5f); glCallList(dlOcta); glPopMatrix(); glDepthMask(GL_TRUE); glDisable(GL_BLEND);
                glPopMatrix(); glEnable(GL_LIGHTING); break; }
            case CK_TERMINAL: Cube(0, 0.5f, 0, 0.7f, 1.0f, 0.5f, 0.22f, 0.24f, 0.22f); glDisable(GL_LIGHTING); glColor3f(0.2f, 0.95f, 0.4f*(0.7f + 0.3f*sinf(tm*3))); glPushMatrix(); glTranslatef(0, 0.78f, -0.26f); glScalef(0.5f, 0.3f, 0.02f); glCallList(dlCube); glPopMatrix(); glEnable(GL_LIGHTING); break;
            default: break;
            }
            if (tgt) { glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE); glColor4f(1.0f, 0.85f, 0.3f, 0.28f + 0.1f*sinf(tm*6)); glPushMatrix(); glTranslatef(0, k.kind == CK_LOCKER ? 0.95f : 0.45f, 0); glScalef(1.15f, k.kind == CK_LOCKER ? 2.1f : 1.0f, 1.15f); glCallList(dlCube); glPopMatrix(); glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_LIGHTING); }
            glPopMatrix();
        }
    }
    for (size_t ci = 0; ci < vis.size(); ci++) { const Chunk* c = vis[ci]; if (c->poi == POI_VILLAGE_HOPE && G.quests[Q_SEEDS].state == 2) DrawGarden(c); }
    glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    for (size_t ci = 0; ci < vis.size(); ci++) { const Chunk* c = vis[ci]; for (size_t i = 0; i < c->anoms.size(); i++) DrawFissure(c->anoms[i], cam.eye, 1.0f - env.day); }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    for (size_t ci = 0; ci < vis.size(); ci++) {
        const Chunk* c = vis[ci];
        for (size_t i = 0; i < c->fires.size(); i++) {
            const Vec3& f = c->fires[i]; if (Sqr(f.x - cam.eye.x) + Sqr(f.z - cam.eye.z) > Sqr(160.0f)) continue;
            float fl = 0.8f + 0.2f*sinf(tm*17.0f + f.x), fl2 = 0.7f + 0.3f*sinf(tm*23.0f + f.z);
            Cube(f.x, f.y + 0.28f*fl, f.z, 0.34f, 0.56f*fl, 0.34f, 1.0f, 0.45f, 0.08f); Cube(f.x + 0.04f, f.y + 0.34f*fl2, f.z - 0.03f, 0.2f, 0.7f*fl2, 0.2f, 1.0f, 0.85f, 0.2f);
        }
        if (c->hasBeacon) { float b = (fmodf(tm, 1.6f) < 0.35f) ? 1.0f : 0.15f; Cube(c->beacon.x, c->beacon.y, c->beacon.z, 0.9f, 0.9f, 0.9f, b, 0.05f, 0.05f); }
    }
    glEnable(GL_LIGHTING);

    // ---- sombras ----
    glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    bool showPlayer = (G.state == GS_PLAYING || G.state == GS_LOADING) && !G.cam.first && !G.drive;
    if (showPlayer) DrawShadow(G.p.pos, 0.55f, 0.42f);
    for (size_t i = 0; i < G.npcs.size(); i++) {
        const Npc* n = G.npcs[i]; if (Sqr(n->pos.x - cam.eye.x) + Sqr(n->pos.z - cam.eye.z) > Sqr(viewDist)) continue;
        if (n->alive) DrawShadow(n->pos, (n->kind == NK_DOG || n->kind == NK_CRAWLER) ? 0.6f : 0.5f*n->scale, 0.38f);
        else if (n->rag) DrawShadow(n->pos, 0.75f*n->scale, 0.28f);
    }
    for (size_t i = 0; i < G.vehicles.size(); i++) { const Vehicle* v = G.vehicles[i]; if (Sqr(v->pos.x - cam.eye.x) + Sqr(v->pos.z - cam.eye.z) < Sqr(viewDist)) DrawShadow(Vec3(v->pos.x, TerrainH(v->pos.x, v->pos.z), v->pos.z), VehInfo(v->type).length*0.55f, 0.45f); }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glEnable(GL_LIGHTING);

    // ---- veiculos ----
    bool night = env.day < 0.45f;
    for (size_t i = 0; i < G.vehicles.size(); i++) {
        const Vehicle* v = G.vehicles[i]; float dx = v->pos.x - cam.eye.x, dz = v->pos.z - cam.eye.z; if (dx*dx + dz*dz > Sqr(viewDist*0.97f)) continue;
        bool mine = (v == G.drive); DrawVehicle(*v, mine && night, mine && !cam.first);
    }

    // ---- NPCs ----
    for (size_t i = 0; i < G.npcs.size(); i++) {
        const Npc* n = G.npcs[i]; float dx = n->pos.x - cam.eye.x, dz = n->pos.z - cam.eye.z, d2 = dx*dx + dz*dz;
        if (d2 > Sqr(viewDist*0.95f)) continue;
        if (d2 > 900.0f) { float ca = (dx*cam.fwd.x + dz*cam.fwd.z)/sqrtf(d2*(cam.fwd.x*cam.fwd.x + cam.fwd.z*cam.fwd.z) + 1e-6f); if (ca < cosf(halfDiag + 0.15f)) continue; }
        if (!n->alive && n->rag) { DrawRagdoll(*n->rag, n->kind); continue; }               // corpo simulado por PBD
        float dead = n->alive ? 0.0f : Sat(n->deadT/0.5f), amp = Sat(n->speed/4.5f), hurt = Sat(n->hurtT/0.25f);
        if (n->kind == NK_DOG) { DrawDog(n->pos, n->yaw, n->scale, n->animPhase, amp, dead, hurt, n->state == NS_COMBAT); continue; }
        if (n->kind == NK_CRAWLER) { DrawCrawler(n->pos, n->yaw, n->animPhase, amp, dead, hurt, n->state == NS_COMBAT); continue; }
        Look L; SetLook(L, n->kind); Pose P; memset(&P, 0, sizeof(P)); P.walk = n->animPhase; P.amp = amp; P.dead = dead; P.hurt = hurt;
        if (n->kind == NK_RAIDER) { P.weapon = IT_PISTOL; P.aim = (n->state == NS_COMBAT) ? 1.0f : 0.0f; }
        else if (n->kind == NK_GUARD) { P.weapon = IT_RIFLE; P.aim = 0.0f; }
        else if (n->kind == NK_ASHEN && n->state == NS_COMBAT && n->cd > 0.8f && n->cd < 1.4f) P.amp = 1.0f;
        DrawHumanoid(n->pos, n->yaw, n->scale, L, P);
    }

    // ---- jogador (3a pessoa) ----
    const Player& p = G.p;
    if (showPlayer) {
        Look L; memset(&L, 0, sizeof(L)); const float sk[3] = { 0.86f, 0.68f, 0.54f }, sh[3] = { 0.36f, 0.40f, 0.27f }, pn[3] = { 0.28f, 0.22f, 0.16f }, hr[3] = { 0.25f, 0.17f, 0.1f };
        memcpy(L.skin, sk, 12); memcpy(L.shirt, sh, 12); memcpy(L.pants, pn, 12); memcpy(L.hair, hr, 12); L.hat = 5; L.plates = p.armor != IT_NONE;
        Pose P; memset(&P, 0, sizeof(P)); P.walk = p.animPhase; P.amp = Sat(p.speed/5.0f); P.crouch = p.sneak ? 1.0f : 0.0f; P.pitch = p.pitch; P.weapon = p.weapon; P.hurt = Sat(p.hurtT/0.45f);
        bool recent = (G.simTime - G.shotT) < 1.5f; P.aim = (p.aimAmt > 0.1f || G.in.fire || recent) ? 1.0f : 0.0f; if (ItemInfo(p.weapon).type != ITY_GUN) P.aim = 0;
        if (p.swingT > 0) P.swing = 1.0f - p.swingT/0.32f;
        if (p.dead) P.dead = 1.0f;
        DrawHumanoid(p.pos, p.bodyYaw, 1.0f, L, P);
    }

    // ---- tiros / particulas / marcadores ----
    glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glLineWidth(2.0f); glBegin(GL_LINES);
    for (size_t i = 0; i < G.tracers.size(); i++) { const Tracer& t = G.tracers[i]; float a = Sat(t.t/0.08f); float tr = t.col == 1 ? 0.5f : 1.0f, tg = t.col == 1 ? 0.9f : (t.col == 2 ? 0.6f : 0.85f), tb = t.col == 1 ? 1.0f : (t.col == 2 ? 0.2f : 0.5f); glColor4f(tr, tg, tb, a); glVertex3f(t.a.x, t.a.y, t.a.z); glColor4f(tr, tg, tb, a*0.4f); glVertex3f(t.b.x, t.b.y, t.b.z); }
    glEnd();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE);
    for (size_t i = 0; i < G.puffs.size(); i++) {
        const Puff& f = G.puffs[i]; float k = 0.5f - f.t, sz = 0.08f + k*(f.kind ? 0.5f : 0.9f), al = Sat(f.t*2.0f)*0.8f;
        if (f.kind >= 2) { glBlendFunc(GL_SRC_ALPHA, GL_ONE); glColor4f(f.kind == 2 ? 0.4f : 1.0f, f.kind == 2 ? 0.9f : 0.55f, f.kind == 2 ? 1.0f : 0.15f, al); sz *= 2.2f; }
        else { glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glColor4f(f.kind ? 0.6f : 0.72f, f.kind ? 0.04f : 0.64f, f.kind ? 0.04f : 0.5f, al); }
        glPushMatrix(); glTranslatef(f.p.x, f.p.y + k*0.5f, f.p.z); glScalef(sz, sz, sz); glCallList(dlCube); glPopMatrix();
    }
    if (p.fireCd > ItemInfo(p.weapon).rate - 0.06f && ItemInfo(p.weapon).type == ITY_GUN && !G.cam.first && (G.state == GS_PLAYING)) {
        Vec3 m = MuzzleWorld(); glColor4f(1.0f, 0.85f, 0.4f, 0.95f); glPushMatrix(); glTranslatef(m.x, m.y, m.z); glScalef(0.22f, 0.22f, 0.22f); glCallList(dlCube); glPopMatrix();
    }
    if (G.state == GS_PLAYING) for (int q = 0; q < Q_COUNT; q++) {                               // coluna de luz nos objetivos
        Vec3 m; if (!QuestMarker(q, m)) continue; float dd = Len2D(m - cam.eye); if (dd < 35.0f || dd > 900.0f) continue;
        float w = 1.2f + dd*0.004f, a = 0.22f + 0.08f*sinf(tm*3.0f); glColor4f(0.3f, 0.8f, 1.0f, a);
        for (int k = 0; k < 2; k++) { glPushMatrix(); glTranslatef(m.x, m.y, m.z); glRotatef(90.0f*(float)k, 0, 1, 0); glBegin(GL_QUADS); glVertex3f(-w, 0, 0); glVertex3f(w, 0, 0); glVertex3f(w, 120, 0); glVertex3f(-w, 120, 0); glEnd(); glPopMatrix(); }
    }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);

    // ---- viewmodel (1a pessoa) ----
    if (G.cam.first && G.state == GS_PLAYING && !p.dead && !G.drive) {
        glClear(GL_DEPTH_BUFFER_BIT); glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glEnable(GL_LIGHTING);
        GLfloat vp[4] = { 0.3f, 0.8f, 0.6f, 0 }; glLightfv(GL_LIGHT0, GL_POSITION, vp); GLfloat vd[4] = { 0.9f, 0.9f, 0.9f, 1 }; glLightfv(GL_LIGHT0, GL_DIFFUSE, vd);
        GLfloat va[4] = { 0.55f, 0.55f, 0.55f, 1 }; glLightModelfv(GL_LIGHT_MODEL_AMBIENT, va); glDisable(GL_LIGHT1);
        const ItemDef& wd = ItemInfo(p.weapon); float ba = sinf(p.animPhase)*0.012f*Sat(p.speed/4.0f), bb = fabsf(cosf(p.animPhase))*0.01f*Sat(p.speed/4.0f);
        float ax = Lerpf(0.2f, 0.0f, p.aimAmt) + ba, ay = Lerpf(-0.22f, -0.13f, p.aimAmt) + bb, az = Lerpf(-0.45f, -0.36f, p.aimAmt) + p.recoil*0.05f;
        float ang = 90.0f - p.recoil*7.0f; if (p.reloadT > 0) { float s = sinf(PI*Sat(1.0f - p.reloadT/Maxf(wd.reload, 0.1f))); ang -= 38.0f*s; ay -= 0.1f*s; }
        if (wd.type == ITY_MELEE) { ang = 30.0f; ax = 0.22f; ay = -0.18f; az = -0.5f; if (p.swingT > 0) { float s = 1.0f - p.swingT/0.32f; ang = -60.0f + 150.0f*s; ax -= 0.2f*s; } }
        glPushMatrix(); glTranslatef(ax, ay, az); glRotatef(ang, 1, 0, 0);
        float sh2[3] = { 0.36f, 0.40f, 0.27f }, sk2[3] = { 0.86f, 0.68f, 0.54f };
        glPushMatrix(); glTranslatef(0.0f, 0.42f, 0.04f); glScalef(0.12f, 0.8f, 0.12f); glColor3f(COL3(sh2)); glCallList(dlCube); glPopMatrix();
        Cube(0, 0.0f, 0.0f, 0.11f, 0.11f, 0.11f, COL3(sk2));
        DrawWeaponModel(p.weapon);
        if (wd.type == ITY_GUN) Cube(-0.05f, -0.25f, 0.06f, 0.09f, 0.09f, 0.09f, COL3(sk2));
        if (wd.type == ITY_GUN && p.fireCd > wd.rate - 0.05f) { glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); float bl = (p.weapon == IT_PISTOL) ? 0.3f : (p.weapon == IT_SHOTGUN ? 0.8f : 0.95f); Cube(0, -bl - 0.1f, 0, 0.16f, 0.16f, 0.16f, 1.0f, 0.8f, 0.3f); glDisable(GL_BLEND); }
        glPopMatrix();
    }
    glDisable(GL_FOG); glDisable(GL_LIGHT1); glDisable(GL_LIGHT2);
}

// ---------------------------------------------------------------------
//  MINIATURA PARA O SAVE (lida do back buffer, ANTES do HUD)
// ---------------------------------------------------------------------
bool CaptureThumb(int W, int H, std::vector<u8>& out, int& tw, int& th) {
    tw = 256; th = 128; if (W < 16 || H < 16) return false;
    std::vector<u8> px((size_t)W*(size_t)H*3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadBuffer(GL_BACK); glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    out.assign((size_t)tw*(size_t)th*3, 0);
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
        int sum[3] = { 0, 0, 0 };
        for (int sy = 0; sy < 2; sy++) for (int sx = 0; sx < 2; sx++) {
            int px0 = Mini(W - 1, (int)(((float)x + 0.25f + 0.5f*(float)sx)*(float)W/(float)tw)), py0 = Mini(H - 1, (int)(((float)(th - 1 - y) + 0.25f + 0.5f*(float)sy)*(float)H/(float)th));
            const u8* s = &px[((size_t)py0*(size_t)W + (size_t)px0)*3]; sum[0] += s[0]; sum[1] += s[1]; sum[2] += s[2];
        }
        u8* d = &out[((size_t)y*(size_t)tw + (size_t)x)*3]; d[0] = (u8)(sum[0]/4); d[1] = (u8)(sum[1]/4); d[2] = (u8)(sum[2]/4);
    }
    return true;
}
