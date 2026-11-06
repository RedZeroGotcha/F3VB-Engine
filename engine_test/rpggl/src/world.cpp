// =====================================================================
//  world.cpp - terreno procedural, POIs, geracao de chunks (worker
//  threads), streaming e colisao.
// =====================================================================
#include "world.h"
#include <map>

World W;
static u32 g_seed = 20772207u;
void WorldSetSeed(u32 s) { g_seed = s; }
u32  WorldSeed() { return g_seed; }

// ---------------------------------------------------------------------
//  POIs  (funcoes puras)
// ---------------------------------------------------------------------
static int RawPoi(int cx, int cz) {
    if (cx < 3 || cz < 3 || cx >= WORLD_CHUNKS - 3 || cz >= WORLD_CHUNKS - 3) return POI_NONE;
    u32 h = HashXZ(cx, cz, g_seed ^ 0xA5A5u) % 1000u;
    if (h < 22) return POI_RUINS;
    if (h < 30) return POI_CAMP;
    if (h < 35) return POI_RAIDERS;
    if (h < 38) return POI_SETTLEMENT;
    if (h < 43) return POI_GAS;
    if (h < 47) return POI_RADZONE;
    if (h < 53) return POI_WRECKS;
    if (h < 55) return POI_BUNKER;
    return POI_NONE;
}
static int FixedPoi(int cx, int cz) {
    if (cx == BUNKER_CX && cz == BUNKER_CZ) return POI_BUNKER;
    if (cx == HOPE_CX && cz == HOPE_CZ) return POI_VILLAGE_HOPE;
    if (cx == RAIDER_CX && cz == RAIDER_CZ) return POI_RAIDER_BASE;
    if (cx == RADIO_CX && cz == RADIO_CZ) return POI_RADIO;
    if (cx == GASFIX_CX && cz == GASFIX_CZ) return POI_GAS;
    if (cx == ANOM_CX && cz == ANOM_CZ) return POI_RADZONE;
    return -1;
}
int PoiAt(int cx, int cz) {
    if (cx < 0 || cz < 0 || cx >= WORLD_CHUNKS || cz >= WORLD_CHUNKS) return POI_NONE;
    int f = FixedPoi(cx, cz); if (f >= 0) return f;
    if (abs(cx - 128) < 9 && abs(cz - 126) < 9) {                       // area inicial: so os POIs da historia
        return POI_NONE;
    }
    int r = RawPoi(cx, cz); if (!r) return POI_NONE;
    // sem POIs vizinhos (evita sobrepor areas aplainadas)
    static const int nb[4][2] = { { -1, 0 }, { 0, -1 }, { -1, -1 }, { 1, -1 } };
    for (int i = 0; i < 4; i++) { if (RawPoi(cx + nb[i][0], cz + nb[i][1])) return POI_NONE; }
    return r;
}

const char* PoiName(int poi, int cx, int cz) {
    static const char* ruins[8] = { "Ruínas do Centro", "Cidade Fantasma", "Bairro Queimado", "Ruínas do Mercado", "Vila Abandonada", "Escola em Ruínas", "Fábrica Tombada", "Ruínas do Cais" };
    static const char* camp[6] = { "Acampamento Solitário", "Abrigo de Viajantes", "Fogueira do Andarilho", "Pouso da Poeira", "Acampamento Velho", "Parada do Eremita" };
    static const char* raid[6] = { "Covil dos Dentes", "Fortaleza do Sucateiro", "Toca da Víbora", "Posto dos Saqueadores", "Campo da Caveira", "Ninho do Corvo" };
    static const char* sett[6] = { "Vila Poeirinha", "Povoado Ferro-Velho", "Vila Cisterna", "Refúgio do Poço", "Vila Sol Fraco", "Povoado Três Árvores" };
    static const char* gas[5] = { "Posto Sete Estradas", "Posto Ferrugem Azul", "Parada do Caminhoneiro", "Posto Sem Nome", "Posto da Encruzilhada" };
    static const char* rad[4] = { "Cratera Verde", "Zona Quente", "Poço Brilhante", "Campo Cintilante" };
    static const char* wrk[4] = { "Cemitério de Carros", "Engarrafamento Final", "Ferro-Velho Grande", "Pátio Abandonado" };
    static const char* bun[3] = { "Silo 41 (selado)", "Silo 09 (aberto)", "Silo 66" };
    u32 h = HashXZ(cx, cz, 777);
    switch (poi) {
    case POI_RUINS: return ruins[h % 8];
    case POI_CAMP: return camp[h % 6];
    case POI_RAIDERS: return raid[h % 6];
    case POI_SETTLEMENT: return sett[h % 6];
    case POI_GAS: return (cx == GASFIX_CX && cz == GASFIX_CZ) ? "Posto Ferrugem" : gas[h % 5];
    case POI_RADZONE: return (cx == ANOM_CX && cz == ANOM_CZ) ? "Campo de Fissuras" : rad[h % 4];
    case POI_WRECKS: return wrk[h % 4];
    case POI_BUNKER: return (cx == BUNKER_CX && cz == BUNKER_CZ) ? "Silo Ômega" : bun[h % 3];
    case POI_RADIO: return "Torre de Rádio Alfa";
    case POI_VILLAGE_HOPE: return "Esperança";
    case POI_RAIDER_BASE: return "Acampamento Presa-Negra";
    }
    return "Ermos";
}
const char* AreaName(float x, float z) {
    int cx = ChunkOf(x), cz = ChunkOf(z);
    int p = PoiAt(cx, cz);
    if (p) {
        float dx = x - (float)(cx*CHUNK_SIZE + 32), dz = z - (float)(cz*CHUNK_SIZE + 32);
        if (dx*dx + dz*dz < 52.0f*52.0f) return PoiName(p, cx, cz);
    }
    if (RoadFactor(x, z) > 0.3f) return "Rodovia Rachada";
    if (RadAt(x, z) > 0.1f) return "Zona Radioativa";
    return "Ermos";
}

// ---------------------------------------------------------------------
//  TERRENO
// ---------------------------------------------------------------------
static float H0(float x, float z) {
    u32 s = g_seed;
    float cdx = x - WORLD_SIZE*0.5f, cdz = z - WORLD_SIZE*0.5f, cd = sqrtf(cdx*cdx + cdz*cdz);
    float calm = Lerpf(0.22f, 1.0f, Smoothstep(350.0f, 2600.0f, cd));
    float h = (Fbm(x*0.00055f, z*0.00055f, 4, s + 11) - 0.5f)*130.0f;
    h += (Fbm(x*0.004f, z*0.004f, 3, s + 23) - 0.5f)*26.0f;
    h += (Fbm(x*0.035f, z*0.035f, 2, s + 37) - 0.5f)*2.4f;
    float m = Fbm(x*0.0009f + 37.0f, z*0.0009f - 11.0f, 3, s + 51);
    if (m > 0.58f) { float t = m - 0.58f; h += t*t*900.0f; }
    h *= calm;
    float e = Minf(Minf(x, z), Minf(WORLD_SIZE - x, WORLD_SIZE - z));
    if (e < 500.0f) h += (500.0f - e)*(500.0f - e)*0.00045f;               // muralha de montanhas na borda
    return h;
}

float TerrainH(float x, float z) {
    float h = H0(x, z);
    int cx = ChunkOf(x), cz = ChunkOf(z);
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
        int p = PoiAt(cx + dx, cz + dz); if (!p) continue;
        float px = (float)((cx + dx)*CHUNK_SIZE + 32), pz = (float)((cz + dz)*CHUNK_SIZE + 32);
        float d = sqrtf(Sqr(x - px) + Sqr(z - pz)); if (d >= 46.0f) continue;
        float hc = H0(px, pz);
        if (p == POI_RADZONE) h = Lerpf(h, hc - 6.0f, Smoothstep(46.0f, 14.0f, d));
        else h = Lerpf(h, hc, Smoothstep(46.0f, 30.0f, d));
    }
    return h;
}
Vec3 TerrainNormal(float x, float z) {
    float e = 1.0f, hx = TerrainH(x + e, z) - TerrainH(x - e, z), hz = TerrainH(x, z + e) - TerrainH(x, z - e);
    return Norm(Vec3(-hx, 2.0f*e, -hz));
}
float RoadFactor(float x, float z) {
    float best = 0;
    float zc = floorf((z + 1024.0f)/2048.0f)*2048.0f + (Fbm(x*0.002f, 5.0f, 2, g_seed + 71) - 0.5f)*80.0f;
    float d = fabsf(z - zc); best = Maxf(best, 1.0f - Smoothstep(4.0f, 6.5f, d));
    float xc = floorf((x + 1024.0f)/2048.0f)*2048.0f + (Fbm(7.0f, z*0.002f, 2, g_seed + 73) - 0.5f)*80.0f;
    d = fabsf(x - xc); best = Maxf(best, 1.0f - Smoothstep(4.0f, 6.5f, d));
    return best;
}
float RadAt(float x, float z) {
    int cx = ChunkOf(x), cz = ChunkOf(z); float best = 0;
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
        if (PoiAt(cx + dx, cz + dz) != POI_RADZONE) continue;
        float px = (float)((cx + dx)*CHUNK_SIZE + 32), pz = (float)((cz + dz)*CHUNK_SIZE + 32);
        best = Maxf(best, Smoothstep(46.0f, 18.0f, sqrtf(Sqr(x - px) + Sqr(z - pz))));
    }
    return best;
}

// ---------------------------------------------------------------------
//  MALHA
// ---------------------------------------------------------------------
struct MeshB {
    std::vector<float>& p; std::vector<float>& n; std::vector<u8>& c;
    MeshB(Chunk& C) : p(C.pos), n(C.nrm), c(C.col) {}
    void V(const Vec3& v, const Vec3& nr, Col k) {
        p.push_back(v.x); p.push_back(v.y); p.push_back(v.z);
        n.push_back(nr.x); n.push_back(nr.y); n.push_back(nr.z);
        c.push_back(k.r); c.push_back(k.g); c.push_back(k.b);
    }
    void TriC(const Vec3& a, const Vec3& b, const Vec3& d, Col ca, Col cb, Col cd) {
        Vec3 nr = Norm(Cross(b - a, d - a)); V(a, nr, ca); V(b, nr, cb); V(d, nr, cd);
    }
    void Tri(const Vec3& a, const Vec3& b, const Vec3& d, Col k) { TriC(a, b, d, k, k, k); }
    void Quad(const Vec3& a, const Vec3& b, const Vec3& d, const Vec3& e, Col k) { Tri(a, b, d, k); Tri(a, d, e, k); }
    static Vec3 L2W(float cx, float cz, float yaw, float lx, float lz, float y) {
        float c = cosf(yaw), s = sinf(yaw); return Vec3(cx + lx*c + lz*s, y, cz - lx*s + lz*c);
    }
    // caixa com a BASE em y0, centro (cx,cz), rotacionada por yaw
    void Box(float cx, float y0, float cz, float sx, float sy, float sz, float yaw, Col k) {
        float hx = sx*0.5f, hz = sz*0.5f; Vec3 b[4], t[4];
        const float lx[4] = { -hx, hx, hx, -hx }, lz[4] = { -hz, -hz, hz, hz };
        for (int i = 0; i < 4; i++) { b[i] = L2W(cx, cz, yaw, lx[i], lz[i], y0); t[i] = L2W(cx, cz, yaw, lx[i], lz[i], y0 + sy); }
        Quad(t[0], t[1], t[2], t[3], Shade(k, 1.10f));
        for (int i = 0; i < 4; i++) { int j = (i + 1) & 3; Quad(b[i], b[j], t[j], t[i], Shade(k, (i & 1) ? 0.86f : 0.96f)); }
    }
    void Cyl(float x, float y0, float z, float r0, float r1, float h, int sides, Col k) {
        for (int i = 0; i < sides; i++) {
            float a0 = 2*PI*(float)i/sides, a1 = 2*PI*(float)(i + 1)/sides;
            Vec3 b0(x + cosf(a0)*r0, y0, z + sinf(a0)*r0), b1(x + cosf(a1)*r0, y0, z + sinf(a1)*r0);
            Vec3 t0(x + cosf(a0)*r1, y0 + h, z + sinf(a0)*r1), t1(x + cosf(a1)*r1, y0 + h, z + sinf(a1)*r1);
            Quad(b0, b1, t1, t0, Shade(k, 0.85f + 0.2f*(float)(i & 1)));
            Tri(Vec3(x, y0 + h, z), t0, t1, Shade(k, 1.1f));
        }
    }
    void Pyr(float cx, float y0, float cz, float sx, float sz, float h, float yaw, Col k) {
        float hx = sx*0.5f, hz = sz*0.5f; Vec3 b[4]; const float lx[4] = { -hx, hx, hx, -hx }, lz[4] = { -hz, -hz, hz, hz };
        for (int i = 0; i < 4; i++) b[i] = L2W(cx, cz, yaw, lx[i], lz[i], y0);
        Vec3 top(cx, y0 + h, cz);
        for (int i = 0; i < 4; i++) Tri(b[i], b[(i + 1) & 3], top, Shade(k, 0.8f + 0.12f*(float)i));
    }
};

// ---------------------------------------------------------------------
//  GERADOR DE CHUNK
// ---------------------------------------------------------------------
struct Gen {
    Chunk& C; Rng rng; MeshB M;
    float ox, oz, mx, mz, baseY;
    int nSpawn, nCont, nVeh;
    std::vector<Vec3> used;                      // (x, z, raio)
    Gen(Chunk& c) : C(c), rng(HashXZ(c.cx, c.cz, g_seed ^ 0x5151u)), M(c),
        ox((float)(c.cx*CHUNK_SIZE)), oz((float)(c.cz*CHUNK_SIZE)), mx(ox + 32), mz(oz + 32), baseY(0), nSpawn(0), nCont(0), nVeh(0) {
        baseY = TerrainH(mx, mz);
    }
    bool Free(float x, float z, float r) {
        if (x < ox + r + 1 || x > ox + CHUNK_SIZE - r - 1 || z < oz + r + 1 || z > oz + CHUNK_SIZE - r - 1) return false;
        for (size_t i = 0; i < used.size(); i++) { float d = Sqr(x - used[i].x) + Sqr(z - used[i].y); if (d < Sqr(r + used[i].z)) return false; }
        return true;
    }
    void Use(float x, float z, float r) { used.push_back(Vec3(x, z, r)); }
    bool Place(float r, float minD, float maxD, float& x, float& z) {
        for (int t = 0; t < 40; t++) {
            float a = rng.r(0, 2*PI), d = rng.r(minD, maxD); x = mx + cosf(a)*d; z = mz + sinf(a)*d;
            if (Free(x, z, r)) { Use(x, z, r); return true; }
        }
        return false;
    }
    Col Var(Col c, float a) { float k = 1.0f + rng.r(-a, a); return Shade(c, k); }

    // ---- colisores ----
    void AddBox(float x, float z, float hx, float hz, float yaw, float y0, float y1) {
        Collider c; c.x = x; c.z = z; c.hx = hx; c.hz = hz; c.c = cosf(yaw); c.s = sinf(yaw); c.y0 = y0; c.y1 = y1; c.circle = false; C.cols.push_back(c);
    }
    void AddCircle(float x, float z, float r, float y0, float y1) {
        Collider c; c.x = x; c.z = z; c.hx = r; c.hz = r; c.c = 1; c.s = 0; c.y0 = y0; c.y1 = y1; c.circle = true; C.cols.push_back(c);
    }
    // ---- entidades ----
    void Spawn(int kind, float x, float z) {
        SpawnPt s; s.id = MakeId(C.cx, C.cz, 1, nSpawn++); s.kind = kind; s.x = x; s.z = z; s.yaw = rng.r(-PI, PI); C.spawns.push_back(s);
    }
    void Cont(int kind, float x, float z, float yaw, int tier, float y = -1000.0f) {
        Container k; k.id = MakeId(C.cx, C.cz, 2, nCont++); k.kind = kind; k.x = x; k.z = z; k.yaw = yaw; k.tier = tier;
        k.y = (y < -999.0f) ? TerrainH(x, z) : y; C.conts.push_back(k);
        if (kind != CK_TRUNK && kind != CK_ARTIFACT) AddCircle(x, z, 0.42f, k.y, k.y + (kind == CK_LOCKER ? 1.9f : 0.9f));
    }
    void Vehicle(int type, float x, float z, float yaw, float fuel, bool locked = false) {
        VehiclePt v; v.id = MakeId(C.cx, C.cz, 3, nVeh++); v.type = type; v.x = x; v.z = z; v.yaw = yaw; v.fuel = fuel; v.locked = locked; C.vehicles.push_back(v);
    }
    void Anom(float x, float z, int type, bool artifact) {
        Anomaly a; a.x = x; a.z = z; a.y = TerrainH(x, z); a.r = 2.4f; a.type = type; C.anoms.push_back(a);
        if (artifact) Cont(CK_ARTIFACT, x, z, 0, 4, a.y + 0.9f);
    }

    // ---- props ----
    void Tree(float x, float z) {
        float y = TerrainH(x, z), h = rng.r(2.6f, 5.4f); Col bark = Var(Col(84, 66, 48), 0.18f);
        M.Cyl(x, y - 0.3f, z, 0.30f, 0.12f, h + 0.3f, 6, bark);
        int nb = 2 + rng.i(3);
        for (int i = 0; i < nb; i++) {
            float by = y + h*rng.r(0.45f, 0.95f), a = rng.r(0, 2*PI), len = rng.r(0.9f, 1.9f);
            M.Box(x + cosf(a)*len*0.5f, by, z + sinf(a)*len*0.5f, len, 0.10f, 0.10f, -a, bark);
        }
        AddCircle(x, z, 0.32f, y, y + h);
    }
    void Shrub(float x, float z) {
        float y = TerrainH(x, z); Col k = Var(Col(112, 100, 58), 0.2f);
        M.Pyr(x, y - 0.05f, z, rng.r(0.8f, 1.5f), rng.r(0.8f, 1.5f), rng.r(0.5f, 1.1f), rng.r(0, PI), k);
    }
    void Rock(float x, float z, float r) {
        float y = TerrainH(x, z) - r*0.25f; Col k = Var(Col(112, 104, 96), 0.15f);
        Vec3 top(x + rng.r(-0.2f, 0.2f)*r, y + r*rng.r(0.9f, 1.4f), z + rng.r(-0.2f, 0.2f)*r), bot(x, y - 0.1f, z);
        Vec3 ring[5]; float off = rng.r(0, 2*PI);
        for (int i = 0; i < 5; i++) { float a = off + 2*PI*(float)i/5 + rng.r(-0.3f, 0.3f), rr = r*rng.r(0.8f, 1.25f); ring[i] = Vec3(x + cosf(a)*rr, y + r*rng.r(0.0f, 0.5f), z + sinf(a)*rr); }
        for (int i = 0; i < 5; i++) { int j = (i + 1) % 5; M.Tri(top, ring[i], ring[j], Shade(k, 1.0f + 0.07f*(float)i)); M.Tri(bot, ring[j], ring[i], Shade(k, 0.7f)); }
        AddCircle(x, z, r*0.85f, y, y + r*1.1f);
    }
    void Barrel(float x, float z, Col k) {
        float y = TerrainH(x, z); M.Cyl(x, y, z, 0.32f, 0.32f, 0.95f, 8, k); M.Cyl(x, y + 0.45f, z, 0.34f, 0.34f, 0.06f, 8, Shade(k, 0.6f));
        AddCircle(x, z, 0.34f, y, y + 0.95f);
    }
    void Car(float x, float z, float yaw, Col body, bool trunk) {
        float y = TerrainH(x, z); float c = cosf(yaw), s = sinf(yaw);
        M.Box(x, y + 0.28f, z, 4.3f, 0.75f, 1.85f, yaw, body);
        M.Box(x - 0.3f*c, y + 1.03f, z + 0.3f*s, 2.3f, 0.62f, 1.65f, yaw, Shade(body, 0.8f));
        const float wl[4][2] = { { 1.4f, 0.95f }, { 1.4f, -0.95f }, { -1.4f, 0.95f }, { -1.4f, -0.95f } };
        for (int i = 0; i < 4; i++) { Vec3 w = MeshB::L2W(x, z, yaw, wl[i][0], wl[i][1], y); M.Box(w.x, y, w.z, 0.75f, 0.62f, 0.28f, yaw, Col(26, 26, 28)); }
        AddBox(x, z, 2.15f, 0.95f, yaw, y, y + 1.65f);
        if (trunk) { Vec3 t = MeshB::L2W(x, z, yaw, 2.5f, 0, y + 0.7f); Cont(CK_TRUNK, t.x, t.z, yaw, 1, y + 0.7f); }
    }
    void Tent(float x, float z, float yaw, Col k) {
        float y = TerrainH(x, z); M.Pyr(x, y, z, 4.2f, 3.4f, 2.5f, yaw, k); AddBox(x, z, 1.9f, 1.4f, yaw, y, y + 1.5f);
    }
    void Fire(float x, float z) {
        float y = TerrainH(x, z);
        for (int i = 0; i < 6; i++) { float a = 2*PI*(float)i/6; M.Box(x + cosf(a)*0.55f, y, z + sinf(a)*0.55f, 0.28f, 0.22f, 0.28f, a, Col(70, 66, 62)); }
        M.Box(x, y, z, 0.5f, 0.12f, 0.5f, 0.5f, Col(24, 20, 18));
        C.fires.push_back(Vec3(x, y + 0.2f, z)); AddCircle(x, z, 0.7f, y, y + 0.3f);
    }
    void Pole(float x, float z) {
        float y = TerrainH(x, z), a = rng.r(0, PI); M.Box(x, y, z, 0.28f, 8.0f, 0.28f, 0, Col(88, 70, 54)); M.Box(x, y + 7.0f, z, 2.6f, 0.14f, 0.14f, a, Col(80, 62, 48)); AddCircle(x, z, 0.2f, y, y + 8);
    }
    void Watchtower(float x, float z) {
        float y = TerrainH(x, z); Col k(96, 78, 58);
        for (int i = 0; i < 4; i++) { float lx = (i & 1) ? 1.6f : -1.6f, lz = (i & 2) ? 1.6f : -1.6f; M.Box(x + lx, y, z + lz, 0.3f, 5.0f, 0.3f, 0, k); AddCircle(x + lx, z + lz, 0.3f, y, y + 5); }
        M.Box(x, y + 4.7f, z, 4.0f, 0.2f, 4.0f, 0, Shade(k, 0.9f));
        for (int i = 0; i < 4; i++) { float a = i*PI*0.5f; M.Box(x + cosf(a)*1.9f, y + 4.9f, z + sinf(a)*1.9f, (i & 1) ? 0.1f : 3.8f, 0.9f, (i & 1) ? 3.8f : 0.1f, 0, Shade(k, 1.1f)); }
    }

    // ---- construcao com paredes em segmentos (ruinas caminhaveis) ----
    // doorSide: 0 norte(-z local), 1 leste(+x), 2 sul(+z), 3 oeste(-x), -1 sem porta
    void Building(float bx, float bz, float w, float d, float h, float yaw, int doorSide, bool roof, float ruin, Col wc, int lootN, int tier) {
        float y0 = TerrainH(bx, bz); const float t = 0.35f;
        M.Box(bx, y0 - 0.05f, bz, w + 0.4f, 0.17f, d + 0.4f, yaw, Col(86, 84, 80));
        for (int s = 0; s < 4; s++) {
            bool alongX = (s % 2 == 0); float L = alongX ? w : d;
            float ox2 = (s == 1) ? w*0.5f : (s == 3 ? -w*0.5f : 0), oz2 = (s == 0) ? -d*0.5f : (s == 2 ? d*0.5f : 0);
            int segs = Maxi(3, (int)(L/2.6f)); if (segs % 2 == 0) segs++;
            float sl = L/(float)segs;
            for (int k = 0; k < segs; k++) {
                if (s == doorSide && k == segs/2) {                                      // vao da porta + verga
                    float off = 0; float lx = ox2 + (alongX ? off : 0), lz = oz2 + (alongX ? 0 : off);
                    Vec3 p = MeshB::L2W(bx, bz, yaw, lx, lz, y0 + 2.4f);
                    if (h > 2.8f && !rng.chance(ruin*0.6f)) M.Box(p.x, y0 + 2.4f, p.z, alongX ? sl : t, h - 2.4f, alongX ? t : sl, yaw, Shade(wc, 0.9f));
                    continue;
                }
                float off = -L*0.5f + sl*((float)k + 0.5f);
                float lx = ox2 + (alongX ? off : 0), lz = oz2 + (alongX ? 0 : off);
                float sh = h; if (rng.chance(ruin)) sh = h*rng.r(0.25f, 0.8f);
                Vec3 p = MeshB::L2W(bx, bz, yaw, lx, lz, y0);
                Col k2 = Var(wc, 0.12f);
                M.Box(p.x, y0, p.z, alongX ? sl + 0.02f : t, sh, alongX ? t : sl + 0.02f, yaw, k2);
                AddBox(p.x, p.z, (alongX ? sl : t)*0.5f, (alongX ? t : sl)*0.5f, yaw, y0, y0 + sh);
            }
        }
        if (roof) M.Box(bx, y0 + h, bz, w + 0.7f, 0.28f, d + 0.7f, yaw, Shade(wc, 0.7f));
        for (int i = 0; i < lootN; i++) {
            float lx = rng.r(-w*0.5f + 1.3f, w*0.5f - 1.3f), lz = rng.r(-d*0.5f + 1.3f, d*0.5f - 1.3f);
            Vec3 p = MeshB::L2W(bx, bz, yaw, lx, lz, y0);
            Cont(rng.chance(0.5f) ? CK_LOCKER : CK_CRATE, p.x, p.z, rng.r(0, 2*PI), tier, y0 + 0.1f);
        }
        if (w > 5 && rng.chance(0.7f)) {                                                 // mesa/estante
            Vec3 p = MeshB::L2W(bx, bz, yaw, rng.r(-w*0.25f, w*0.25f), rng.r(-d*0.25f, d*0.25f), y0);
            M.Box(p.x, y0, p.z, 1.6f, 0.8f, 0.9f, yaw + rng.r(-0.3f, 0.3f), Col(92, 74, 56)); AddBox(p.x, p.z, 0.8f, 0.45f, yaw, y0, y0 + 0.8f);
        }
    }

    // ---- pontos de interesse ----
    void Terrain() {
        float H[CELLS + 1][CELLS + 1]; Col K[CELLS + 1][CELLS + 1];
        C.minY = 1e9f; C.maxY = -1e9f;
        for (int j = 0; j <= CELLS; j++) for (int i = 0; i <= CELLS; i++) {
            H[j][i] = TerrainH(ox + i*CELL_M, oz + j*CELL_M); C.minY = Minf(C.minY, H[j][i]); C.maxY = Maxf(C.maxY, H[j][i]);
        }
        for (int j = 0; j <= CELLS; j++) for (int i = 0; i <= CELLS; i++) {
            float x = ox + i*CELL_M, z = oz + j*CELL_M;
            int i0 = Maxi(i - 1, 0), i1 = Mini(i + 1, CELLS), j0 = Maxi(j - 1, 0), j1 = Mini(j + 1, CELLS);
            float gx = (H[j][i1] - H[j][i0])/((float)(i1 - i0)*CELL_M), gz = (H[j1][i] - H[j0][i])/((float)(j1 - j0)*CELL_M);
            K[j][i] = GroundColor(x, z, H[j][i], sqrtf(gx*gx + gz*gz));
        }
        for (int j = 0; j < CELLS; j++) for (int i = 0; i < CELLS; i++) {
            Vec3 a(ox + i*CELL_M, H[j][i], oz + j*CELL_M), b(ox + (i + 1)*CELL_M, H[j][i + 1], oz + j*CELL_M);
            Vec3 c(ox + (i + 1)*CELL_M, H[j + 1][i + 1], oz + (j + 1)*CELL_M), d(ox + i*CELL_M, H[j + 1][i], oz + (j + 1)*CELL_M);
            if (((i + j) & 1) == 0) { M.TriC(a, d, b, K[j][i], K[j + 1][i], K[j][i + 1]); M.TriC(b, d, c, K[j][i + 1], K[j + 1][i], K[j + 1][i + 1]); }
            else { M.TriC(a, d, c, K[j][i], K[j + 1][i], K[j + 1][i + 1]); M.TriC(a, c, b, K[j][i], K[j + 1][i + 1], K[j][i + 1]); }
        }
    }
    Col GroundColor(float x, float z, float h, float slope) {
        u32 s = g_seed;
        float n1 = Fbm(x*0.02f, z*0.02f, 2, s + 5), n2 = VNoise(x*0.25f, z*0.25f, s + 9);
        Col c = Mix(Col(160, 136, 98), Col(130, 108, 80), n1);
        c = Mix(c, Col(104, 98, 88), Smoothstep(0.52f, 0.72f, Fbm(x*0.006f, z*0.006f, 2, s + 13)));
        c = Mix(c, Col(122, 114, 68), Smoothstep(0.55f, 0.68f, Fbm(x*0.012f, z*0.012f, 2, s + 17))*0.8f);
        c = Mix(c, Col(114, 106, 100), Smoothstep(0.45f, 0.95f, slope));
        if (h > 70.0f) c = Mix(c, Col(150, 146, 140), Smoothstep(70.0f, 140.0f, h)*0.8f);
        c = Shade(c, 0.93f + 0.14f*n2);
        float rf = RoadFactor(x, z); if (rf > 0.01f) c = Mix(c, Col(58, 58, 62), rf*0.95f);
        float ra = RadAt(x, z); if (ra > 0.01f) c = Mix(c, Col(104, 152, 58), ra*0.7f);
        if (C.poi && C.poi != POI_RADZONE && C.poi != POI_WRECKS && C.poi != POI_RUINS) {
            float d = sqrtf(Sqr(x - mx) + Sqr(z - mz)); c = Mix(c, Col(116, 94, 68), Smoothstep(38.0f, 24.0f, d)*0.85f);
        }
        return c;
    }

    void SettlerShack(float px, float pz, float cxTo, float czTo, float w, float d, Col wc) {
        float yaw = atan2f(cxTo - px, czTo - pz);
        Building(px, pz, w, d, 2.7f, yaw, 2, true, 0.0f, wc, 1, 1);
    }

    void GenHamlet(bool hope) {
        Col cols[4] = { Col(150, 92, 60), Col(96, 116, 130), Col(120, 120, 96), Col(132, 74, 58) };
        int n = hope ? 8 : 5 + rng.i(3);
        float a0 = rng.r(0, 2*PI);
        for (int i = 0; i < n; i++) {
            float a = a0 + 2*PI*(float)i/(float)n + rng.r(-0.12f, 0.12f), r = rng.r(17.0f, 21.0f);
            float px = mx + cosf(a)*r, pz = mz + sinf(a)*r;
            bool big = hope && i == 0;
            float w = big ? 10.0f : rng.r(4.2f, 6.0f), d = big ? 8.0f : rng.r(4.0f, 5.2f);
            SettlerShack(px, pz, mx, mz, w, d, cols[rng.i(4)]); Use(px, pz, Maxf(w, d)*0.7f);
        }
        Barrel(mx + 0.9f, mz, Col(150, 60, 40)); Fire(mx - 0.4f, mz + 0.5f); Use(mx, mz, 5.0f);
        float tx, tz; if (Place(3.0f, 22.0f, 28.0f, tx, tz)) Watchtower(tx, tz);
        for (int i = 0; i < 3; i++) { float x, z; if (Place(1.2f, 6.0f, 14.0f, x, z)) Cont(CK_CRATE, x, z, rng.r(0, 6), 1); }
        // barraca do mercador
        { float sx = mx + 6.0f, sz = mz + 5.0f; M.Box(sx, TerrainH(sx, sz), sz, 3.2f, 0.9f, 1.0f, 0, Col(110, 84, 60)); AddBox(sx, sz, 1.6f, 0.5f, 0, TerrainH(sx, sz), TerrainH(sx, sz) + 0.9f);
          M.Box(sx, TerrainH(sx, sz) + 2.4f, sz + 0.2f, 3.8f, 0.12f, 2.2f, 0, Col(130, 52, 44)); Spawn(NK_MERCHANT, sx, sz - 1.6f); }
        int ns = hope ? 6 : 3 + rng.i(3);
        for (int i = 0; i < ns; i++) { float x, z; if (Place(0.8f, 4.0f, 14.0f, x, z)) Spawn(NK_SETTLER, x, z); }
        for (int i = 0; i < 2; i++) { float x, z; if (Place(0.8f, 8.0f, 15.0f, x, z)) Spawn(NK_GUARD, x, z); }
        if (hope) { Spawn(NK_ELDER, mx + cosf(a0)*14.0f, mz + sinf(a0)*14.0f + 0.0f); }
    }

    void GenRaiders(bool base) {
        int nt = base ? 5 : 3 + rng.i(3);
        Col tc[3] = { Col(96, 52, 48), Col(78, 84, 56), Col(74, 70, 66) };
        for (int i = 0; i < nt; i++) { float x, z; if (Place(3.0f, 10.0f, 24.0f, x, z)) Tent(x, z, rng.r(0, PI), tc[rng.i(3)]); }
        if (base) {
            float x, z; if (Place(8.0f, 14.0f, 22.0f, x, z)) Building(x, z, 11.0f, 8.0f, 3.2f, rng.r(0, PI), 2, false, 0.3f, Col(104, 98, 92), 3, 3);
        }
        Fire(mx, mz); Use(mx, mz, 3.0f);
        int nb = 4 + rng.i(4); for (int i = 0; i < nb; i++) { float x, z; if (Place(0.6f, 6.0f, 26.0f, x, z)) Barrel(x, z, rng.chance(0.5f) ? Col(140, 56, 40) : Col(74, 84, 100)); }
        int nc = base ? 4 : 2; for (int i = 0; i < nc; i++) { float x, z; if (Place(1.0f, 4.0f, 22.0f, x, z)) Cont(rng.chance(0.3f) ? CK_SAFE : CK_CRATE, x, z, rng.r(0, 6), base ? 3 : 2); }
        for (int i = 0; i < 8; i++) {                                                  // estacas com caveiras
            float a = 2*PI*(float)i/8, x = mx + cosf(a)*28.5f, z = mz + sinf(a)*28.5f; if (!Free(x, z, 0.4f)) continue;
            float y = TerrainH(x, z); M.Box(x, y, z, 0.18f, 2.8f, 0.18f, 0, Col(70, 56, 44)); M.Box(x, y + 2.6f, z, 0.4f, 0.4f, 0.4f, a, Col(214, 208, 190)); AddCircle(x, z, 0.2f, y, y + 2.8f);
        }
        int nr = base ? 8 : 4 + rng.i(3); for (int i = 0; i < nr; i++) { float x, z; if (Place(0.8f, 3.0f, 22.0f, x, z)) Spawn(NK_RAIDER, x, z); }
        if (base || rng.chance(0.4f)) { float x, z; if (Place(3.0f, 12.0f, 26.0f, x, z)) Vehicle(VT_BUGGY, x, z, rng.r(0, 2*PI), rng.r(15, 60)); }
        if (base) Spawn(NK_BRUTE, mx + 6.0f, mz - 12.0f);
    }

    void GenGas(bool fixed) {
        float y = TerrainH(mx, mz); Col k(150, 146, 132);
        for (int i = 0; i < 4; i++) { float lx = (i & 1) ? 6.0f : -6.0f, lz = (i & 2) ? 3.2f : -3.2f; M.Box(mx + lx, y, mz + lz - 8.0f, 0.5f, 4.2f, 0.5f, 0, k); AddCircle(mx + lx, mz + lz - 8.0f, 0.45f, y, y + 4.2f); }
        M.Box(mx, y + 4.2f, mz - 8.0f, 14.5f, 0.4f, 8.5f, 0, Col(176, 66, 52));
        for (int i = 0; i < 3; i++) { float px = mx - 3.5f + 3.5f*(float)i; M.Box(px, y, mz - 8.0f, 0.8f, 1.5f, 0.7f, 0, Col(120, 40, 38)); AddBox(px, mz - 8.0f, 0.4f, 0.35f, 0, y, y + 1.5f); }
        Use(mx, mz - 8.0f, 9.0f);
        Building(mx, mz + 9.0f, 12.0f, 7.0f, 3.4f, 0, 0, true, 0.15f, Col(172, 168, 150), 3, 2); Use(mx, mz + 9.0f, 8.0f);
        for (int i = 0; i < 3; i++) { float x, z; if (Place(2.4f, 16.0f, 27.0f, x, z)) Car(x, z, rng.r(0, 2*PI), Col(120 + rng.i(60), 100 + rng.i(40), 70 + rng.i(30)), rng.chance(0.5f)); }
        if (fixed) { Spawn(NK_MERCHANT, mx + 2.0f, mz + 4.0f); Spawn(NK_MECHANIC, mx + 14.0f, mz + 2.0f); Vehicle(VT_BUGGY, mx + 19.0f, mz - 3.0f, 0.4f, 55.0f, true); Use(mx + 19.0f, mz - 3.0f, 3.0f);
                     Vehicle(VT_TRUCK, mx - 20.0f, mz + 12.0f, 1.2f, 0.0f); Use(mx - 20.0f, mz + 12.0f, 3.5f); }
        else if (rng.chance(0.6f)) { float x, z; if (Place(3.0f, 14.0f, 27.0f, x, z)) Vehicle(rng.chance(0.5f) ? VT_BUGGY : VT_TRUCK, x, z, rng.r(0, 2*PI), rng.r(0, 30)); }
        if (!fixed) for (int i = 0; i < 2; i++) { float x, z; if (Place(0.8f, 4.0f, 22.0f, x, z)) Spawn(rng.chance(0.5f) ? NK_RAIDER : NK_DOG, x, z); }
    }

    void GenRuins() {
        int nb = 2 + rng.i(3);
        for (int i = 0; i < nb; i++) {
            float w = rng.r(6.0f, 12.0f), d = rng.r(5.0f, 10.0f), x, z; float rad = sqrtf(w*w + d*d)*0.5f + 1.0f;
            if (!Place(rad, 0.0f, 26.0f, x, z)) continue;
            Building(x, z, w, d, rng.r(3.0f, 4.2f), rng.r(0, 2*PI), rng.i(4), rng.chance(0.4f), 0.45f, Col(118 + rng.i(30), 112 + rng.i(24), 104 + rng.i(18)), 1 + rng.i(3), 1 + rng.i(2));
        }
        for (int i = 0; i < 8; i++) { float x, z; if (Place(1.0f, 0.0f, 28.0f, x, z)) Rock(x, z, rng.r(0.5f, 1.3f)); }
        for (int i = 0; i < 3; i++) { float x, z; if (Place(2.4f, 0.0f, 28.0f, x, z)) Car(x, z, rng.r(0, 2*PI), Col(110 + rng.i(60), 90 + rng.i(40), 70 + rng.i(30)), rng.chance(0.5f)); }
        int nd = rng.i(3); for (int i = 0; i < nd; i++) { float x, z; if (Place(0.8f, 0, 26.0f, x, z)) { float q = rng.f(); Spawn(q < 0.35f ? NK_ASHEN : (q < 0.6f ? NK_RAIDER : NK_CRAWLER), x, z); } }
        if (rng.chance(0.12f)) { float x, z; if (Place(3.0f, 10.0f, 26.0f, x, z)) Vehicle(VT_TRUCK, x, z, rng.r(0, 2*PI), rng.r(0, 25)); }
    }

    void GenBunker(bool start) {
        float y = TerrainH(mx, mz);
        M.Box(mx, y - 0.05f, mz, 18.0f, 0.3f, 18.0f, 0, Col(96, 98, 100));
        M.Box(mx, y, mz, 5.0f, 1.3f, 5.0f, 0, Col(84, 86, 88)); AddBox(mx, mz, 2.5f, 2.5f, 0, y, y + 1.3f);
        M.Box(mx, y + 1.3f, mz, 3.2f, 0.12f, 3.2f, 0, start ? Col(70, 130, 150) : Col(40, 40, 44));
        for (int i = 0; i < 4; i++) { float a = i*PI*0.5f + PI*0.25f, x = mx + cosf(a)*8.6f, z = mz + sinf(a)*8.6f; M.Box(x, y, z, 1.4f, 1.0f, 1.4f, a, Col(104, 104, 100)); AddBox(x, z, 0.7f, 0.7f, a, y, y + 1.0f); }
        if (start) { Barrel(mx + 7.0f, mz - 3.0f, Col(60, 90, 140)); Cont(CK_LOCKER, mx - 6.5f, mz + 6.5f, 0.5f, 1); Cont(CK_CRATE, mx + 6.8f, mz + 5.0f, 1.0f, 5);
                     Vehicle(VT_TRUCK, mx + 2.0f, mz + 21.0f, 0.0f, 38.0f); Use(mx + 2.0f, mz + 21.0f, 3.5f); }
        else {
            for (int i = 0; i < 3; i++) { float x, z; if (Place(1.0f, 11.0f, 24.0f, x, z)) Cont(CK_LOCKER, x, z, rng.r(0, 6), 3); }
            for (int i = 0; i < 3; i++) { float x, z; if (Place(0.8f, 8.0f, 24.0f, x, z)) Spawn(NK_ASHEN, x, z); }
        }
    }

    void GenRadio() {
        float y = TerrainH(mx, mz); Col k(150, 70, 58);
        for (int seg = 0; seg < 6; seg++) {
            float r = 4.0f - 0.55f*(float)seg, sy = y + 8.0f*(float)seg;
            for (int i = 0; i < 4; i++) { float lx = (i & 1) ? r : -r, lz = (i & 2) ? r : -r; M.Box(mx + lx, sy, mz + lz, 0.4f, 8.0f, 0.4f, 0, (seg & 1) ? k : Col(190, 186, 176)); }
            for (int i = 0; i < 4; i++) { float a = i*PI*0.5f; M.Box(mx + cosf(a)*r, sy + 8.0f, mz + sinf(a)*r, (i & 1) ? 0.22f : 2*r, 0.22f, (i & 1) ? 2*r : 0.22f, 0, Col(170, 166, 156)); }
        }
        for (int i = 0; i < 4; i++) { float lx = (i & 1) ? 4.0f : -4.0f, lz = (i & 2) ? 4.0f : -4.0f; AddCircle(mx + lx, mz + lz, 0.5f, y, y + 8.0f); }
        C.hasBeacon = true; C.beacon = Vec3(mx, y + 49.0f, mz);
        Building(mx + 15.0f, mz + 6.0f, 7.0f, 6.0f, 3.0f, 0.4f, 2, true, 0.1f, Col(140, 138, 126), 1, 2);
        { Vec3 p = MeshB::L2W(mx + 15.0f, mz + 6.0f, 0.4f, -2.0f, -1.2f, y); Cont(CK_TERMINAL, p.x, p.z, 0.4f, 0, TerrainH(p.x, p.z) + 0.1f); }
        for (int i = 0; i < 3; i++) { float x, z; if (Place(1.0f, 10.0f, 26.0f, x, z) && (x < mx + 6 || x > mx + 24)) Barrel(x, z, Col(110, 110, 100)); }
        Use(mx, mz, 7.0f); Use(mx + 15.0f, mz + 6.0f, 8.0f);
        Spawn(NK_DOG, mx - 14.0f, mz + 8.0f); Spawn(NK_DOG, mx - 12.0f, mz - 12.0f); Spawn(NK_RAIDER, mx + 8.0f, mz - 10.0f);
    }

    void GenRad() {
        bool fixed = (C.cx == ANOM_CX && C.cz == ANOM_CZ);
        for (int i = 0; i < 10; i++) { float x, z; if (Place(0.6f, 6.0f, 26.0f, x, z)) Barrel(x, z, Col(110, 170, 60)); }
        for (int i = 0; i < 6; i++) { float x, z; if (Place(1.4f, 8.0f, 28.0f, x, z)) Rock(x, z, rng.r(1.0f, 2.2f)); }
        { float x, z; if (Place(1.0f, 0, 4.0f, x, z)) Cont(CK_CRATE, x, z, 0, 3); }
        int na = fixed ? 9 : 4 + rng.i(3);                                           // fissuras: campo minado invisivel; estilhaços no centro de algumas
        for (int i = 0; i < na; i++) { float x, z; if (Place(3.2f, 7.0f, 26.0f, x, z)) Anom(x, z, rng.i(2), i < (fixed ? 5 : 3) || rng.chance(0.3f)); }
        int ng = 3 + rng.i(3); for (int i = 0; i < ng; i++) { float x, z; if (Place(0.8f, 3.0f, 24.0f, x, z)) { float q = rng.f(); Spawn(q < 0.6f ? NK_ASHEN : NK_CRAWLER, x, z); } }
        if (rng.chance(fixed ? 0.0f : 0.3f)) { float x, z; if (Place(1.0f, 10.0f, 24.0f, x, z)) Spawn(NK_BRUTE, x, z); }
    }

    void GenWrecks() {
        int n = 8 + rng.i(7);
        for (int i = 0; i < n; i++) { float x, z; if (Place(2.5f, 0.0f, 28.0f, x, z)) Car(x, z, rng.r(0, 2*PI), Col(100 + rng.i(90), 80 + rng.i(60), 60 + rng.i(40)), rng.chance(0.4f)); }
        int nd = 1 + rng.i(3); for (int i = 0; i < nd; i++) { float x, z; if (Place(0.8f, 0, 26.0f, x, z)) Spawn(rng.chance(0.3f) ? NK_CRAWLER : NK_DOG, x, z); }
        if (rng.chance(0.4f)) { float x, z; if (Place(3.0f, 4.0f, 26.0f, x, z)) Vehicle(rng.chance(0.5f) ? VT_BUGGY : VT_TRUCK, x, z, rng.r(0, 2*PI), rng.r(0, 30)); }
    }

    void GenCamp() {
        float x, z;
        if (Place(3.0f, 4.0f, 10.0f, x, z)) Tent(x, z, rng.r(0, PI), Col(90, 100, 70));
        Fire(mx, mz); Use(mx, mz, 2.0f);
        if (Place(1.0f, 3.0f, 10.0f, x, z)) Cont(CK_CRATE, x, z, 0, 1);
        if (rng.chance(0.2f)) { float vx, vz; if (Place(3.0f, 10.0f, 22.0f, vx, vz)) Vehicle(VT_TRUCK, vx, vz, rng.r(0, 2*PI), rng.r(10, 45)); }
        if (rng.chance(0.6f)) Spawn(NK_SETTLER, mx + 2.0f, mz + 2.0f);
        else { Spawn(NK_RAIDER, mx + 3.0f, mz + 1.0f); Spawn(NK_RAIDER, mx - 3.0f, mz - 2.0f); }
    }

    void Scatter() {
        bool poi = C.poi != 0; int startDist = Maxi(abs(C.cx - BUNKER_CX), abs(C.cz - BUNKER_CZ));
        float treeP = Smoothstep(0.38f, 0.62f, Fbm(mx*0.004f, mz*0.004f, 2, g_seed + 91));
        for (int i = 0; i < 14; i++) {
            float x = ox + rng.r(2, CHUNK_SIZE - 2), z = oz + rng.r(2, CHUNK_SIZE - 2);
            if (poi && Sqr(x - mx) + Sqr(z - mz) < Sqr(31.0f)) continue;
            if (RoadFactor(x, z) > 0.02f || RadAt(x, z) > 0.4f) continue;
            if (!Free(x, z, 1.0f)) continue;
            if (rng.chance(0.15f + 0.7f*treeP)) { Tree(x, z); Use(x, z, 1.5f); }
            else if (rng.chance(0.25f)) { Rock(x, z, rng.r(0.5f, 1.6f)); Use(x, z, 1.5f); }
        }
        for (int i = 0; i < 18; i++) {
            float x = ox + rng.r(1, CHUNK_SIZE - 1), z = oz + rng.r(1, CHUNK_SIZE - 1);
            if (poi && Sqr(x - mx) + Sqr(z - mz) < Sqr(30.0f)) continue;
            if (RoadFactor(x, z) > 0.0f) continue;
            if (rng.chance(0.45f)) Shrub(x, z);
        }
        for (int i = 0; i < 4; i++) {                                                  // carros na estrada
            float x = ox + rng.r(3, CHUNK_SIZE - 3), z = oz + rng.r(3, CHUNK_SIZE - 3);
            if (RoadFactor(x, z) < 0.85f || !Free(x, z, 2.6f) || !rng.chance(0.3f)) continue;
            Use(x, z, 2.6f); Car(x, z, rng.chance(0.5f) ? 0.0f + rng.r(-0.2f, 0.2f) : PI*0.5f + rng.r(-0.2f, 0.2f), Col(110 + rng.i(80), 90 + rng.i(50), 70 + rng.i(40)), rng.chance(0.4f));
        }
        if (rng.chance(0.05f)) { float x = ox + rng.r(8, 56), z = oz + rng.r(8, 56); if (!poi && Free(x, z, 1.0f) && RoadFactor(x, z) < 0.01f) Pole(x, z); }
        if (!poi && startDist > 3) {                                                  // fauna e viajantes dos ermos
            float x = ox + rng.r(10, 54), z = oz + rng.r(10, 54);
            float r = rng.f();
            if (r < 0.040f) { int n = 2 + rng.i(2); for (int i = 0; i < n; i++) Spawn(NK_DOG, x + rng.r(-3, 3), z + rng.r(-3, 3)); }
            else if (r < 0.062f) { int n = 1 + rng.i(2); for (int i = 0; i < n; i++) Spawn(NK_ASHEN, x + rng.r(-3, 3), z + rng.r(-3, 3)); }
            else if (r < 0.072f) { int n = 3 + rng.i(3); for (int i = 0; i < n; i++) Spawn(NK_CRAWLER, x + rng.r(-4, 4), z + rng.r(-4, 4)); }
            else if (r < 0.076f && startDist > 12) Spawn(NK_BRUTE, x, z);
            else if (r < 0.088f) { Spawn(NK_RAIDER, x, z); Spawn(NK_RAIDER, x + 2, z + 1); }
            else if (r < 0.096f) { Spawn(NK_SETTLER, x, z); }
            else if (r < 0.116f && Free(x, z, 1.0f)) Cont(CK_BODY, x, z, rng.r(0, 6), 1);
            else if (r < 0.122f && Free(x, z, 1.5f)) Anom(x, z, rng.i(2), rng.chance(0.4f));
        }
    }
};

void GenerateChunk(Chunk& C) {
    C.poi = PoiAt(C.cx, C.cz); C.cenx = (float)(C.cx*CHUNK_SIZE + 32); C.cenz = (float)(C.cz*CHUNK_SIZE + 32);
    C.pos.reserve(9000*3); C.nrm.reserve(9000*3); C.col.reserve(9000*3);
    Gen G(C);
    G.Terrain();
    switch (C.poi) {
    case POI_RUINS: G.GenRuins(); break;
    case POI_CAMP: G.GenCamp(); break;
    case POI_RAIDERS: G.GenRaiders(false); break;
    case POI_RAIDER_BASE: G.GenRaiders(true); break;
    case POI_SETTLEMENT: G.GenHamlet(false); break;
    case POI_VILLAGE_HOPE: G.GenHamlet(true); break;
    case POI_GAS: G.GenGas(C.cx == GASFIX_CX && C.cz == GASFIX_CZ); break;
    case POI_RADZONE: G.GenRad(); break;
    case POI_WRECKS: G.GenWrecks(); break;
    case POI_BUNKER: G.GenBunker(C.cx == BUNKER_CX && C.cz == BUNKER_CZ); break;
    case POI_RADIO: G.GenRadio(); break;
    default: break;
    }
    G.Scatter();
    C.vcount = (int)(C.pos.size()/3);
}

// ---------------------------------------------------------------------
//  WORLD: streaming
// ---------------------------------------------------------------------
void World::Init(ThreadPool* p, u32 seed) { pool = p; WorldSetSeed(seed); }

void World::Reset() {
    for (;;) {                                               // espera os workers terminarem
        bool busy = false;
        for (std::unordered_map<u64, Chunk*>::iterator it = map.begin(); it != map.end(); ++it) if (it->second->state.load() == 0) busy = true;
        if (!busy) break;
        SleepMs(1);
    }
    for (std::unordered_map<u64, Chunk*>::iterator it = map.begin(); it != map.end(); ++it) delete it->second;
    map.clear(); statReady = statPending = 0;
}

Chunk* World::Get(int cx, int cz) const {
    std::unordered_map<u64, Chunk*>::const_iterator it = map.find(ChunkKey(cx, cz));
    if (it == map.end() || it->second->state.load() != 1) return 0;
    return it->second;
}

void World::Update(const Vec3& center, const std::function<void(Chunk*)>& onReady, const std::function<void(Chunk*)>& onUnload) {
    int pcx = ChunkOf(center.x), pcz = ChunkOf(center.z), R = viewRadius;
    int ready = 0, pending = 0;
    std::vector<u64> dead;
    for (std::unordered_map<u64, Chunk*>::iterator it = map.begin(); it != map.end(); ++it) {
        Chunk* c = it->second;
        if (c->state.load() == 1) {
            if (!c->spawned) { c->spawned = true; if (onReady) onReady(c); }
            if (Maxi(abs(c->cx - pcx), abs(c->cz - pcz)) > R + 1) dead.push_back(it->first);
            ready++;
        } else pending++;
    }
    for (size_t i = 0; i < dead.size(); i++) {
        Chunk* c = map[dead[i]]; if (onUnload) onUnload(c); delete c; map.erase(dead[i]); ready--;
    }
    if (pool && pending < 10) {
        struct Need { int d2, cx, cz; }; std::vector<Need> need;
        for (int dz = -R; dz <= R; dz++) for (int dx = -R; dx <= R; dx++) {
            int d2 = dx*dx + dz*dz; if (d2 > R*R + R) continue;
            int cx = pcx + dx, cz = pcz + dz; if (cx < 0 || cz < 0 || cx >= WORLD_CHUNKS || cz >= WORLD_CHUNKS) continue;
            if (map.find(ChunkKey(cx, cz)) != map.end()) continue;
            Need n; n.d2 = d2; n.cx = cx; n.cz = cz; need.push_back(n);
        }
        std::sort(need.begin(), need.end(), [](const Need& a, const Need& b) { return a.d2 < b.d2; });
        for (size_t i = 0; i < need.size() && pending < 14; i++) {
            Chunk* c = new Chunk(); c->cx = need[i].cx; c->cz = need[i].cz; map[ChunkKey(c->cx, c->cz)] = c; pending++;
            pool->Submit([c]() { GenerateChunk(*c); c->state.store(1); });
        }
    }
    statReady = ready; statPending = pending;
}

bool World::NearReady(const Vec3& c, int r) const {
    int pcx = ChunkOf(c.x), pcz = ChunkOf(c.z);
    for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
        int cx = pcx + dx, cz = pcz + dz; if (cx < 0 || cz < 0 || cx >= WORLD_CHUNKS || cz >= WORLD_CHUNKS) continue;
        if (!Get(cx, cz)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------
//  COLISAO
// ---------------------------------------------------------------------
bool World::Collide(float& x, float& z, float r, float feetY, float& groundY) const {
    bool hit = false; groundY = -1e9f;
    int pcx = ChunkOf(x), pcz = ChunkOf(z);
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
        const Chunk* ch = Get(pcx + dx, pcz + dz); if (!ch) continue;
        for (size_t i = 0; i < ch->cols.size(); i++) {
            const Collider& k = ch->cols[i];
            float rb = k.hx + k.hz + r; if (fabsf(x - k.x) > rb || fabsf(z - k.z) > rb) continue;
            if (feetY > k.y1 + 0.05f) continue;
            if (feetY < k.y0 - 1.8f) continue;
            bool canStand = feetY >= k.y1 - 0.7f;
            if (k.circle) {
                float ddx = x - k.x, ddz = z - k.z, d2 = ddx*ddx + ddz*ddz, rr = k.hx + r;
                if (canStand) { if (d2 < k.hx*k.hx) groundY = Maxf(groundY, k.y1); continue; }
                if (d2 < rr*rr) { float d = sqrtf(d2); if (d < 1e-5f) { ddx = 1; ddz = 0; d = 1; } x = k.x + ddx/d*rr; z = k.z + ddz/d*rr; hit = true; }
            } else {
                float dx2 = x - k.x, dz2 = z - k.z; float lx = dx2*k.c - dz2*k.s, lz = dx2*k.s + dz2*k.c;
                if (canStand) { if (fabsf(lx) < k.hx && fabsf(lz) < k.hz) groundY = Maxf(groundY, k.y1); continue; }
                float cl = Clampf(lx, -k.hx, k.hx), cz2 = Clampf(lz, -k.hz, k.hz), ex = lx - cl, ez = lz - cz2, d2 = ex*ex + ez*ez;
                float px = 0, pz = 0;
                if (d2 > 1e-10f) { if (d2 >= r*r) continue; float d = sqrtf(d2); px = ex/d*(r - d); pz = ez/d*(r - d); }
                else {                                                              // centro dentro da caixa: empurra pelo menor eixo
                    float ox2 = k.hx - fabsf(lx), oz2 = k.hz - fabsf(lz);
                    if (ox2 < oz2) px = (lx < 0 ? -1.0f : 1.0f)*(ox2 + r); else pz = (lz < 0 ? -1.0f : 1.0f)*(oz2 + r);
                }
                x += px*k.c + pz*k.s; z += -px*k.s + pz*k.c; hit = true;
            }
        }
    }
    return hit;
}

static float RayCollider(const Vec3& o, const Vec3& d, float maxT, const Collider& k) {
    float odx = o.x - k.x, odz = o.z - k.z;
    if (k.circle) {
        float a = d.x*d.x + d.z*d.z; if (a < 1e-9f) return -1.0f;
        float b = odx*d.x + odz*d.z, cc = odx*odx + odz*odz - k.hx*k.hx, disc = b*b - a*cc;
        if (disc < 0) return -1.0f;
        float t = (-b - sqrtf(disc))/a; if (t < 0) { if (cc < 0) t = 0; else return -1.0f; }
        if (t > maxT) return -1.0f;
        float y = o.y + d.y*t;
        return (y >= k.y0 && y <= k.y1) ? t : -1.0f;
    }
    float lox = odx*k.c - odz*k.s, loz = odx*k.s + odz*k.c, ldx = d.x*k.c - d.z*k.s, ldz = d.x*k.s + d.z*k.c;
    float t0 = 0, t1 = maxT;
    for (int ax = 0; ax < 2; ax++) {
        float oo = ax ? loz : lox, dd = ax ? ldz : ldx, h = ax ? k.hz : k.hx;
        if (fabsf(dd) < 1e-7f) { if (oo < -h || oo > h) return -1.0f; }
        else { float ta = (-h - oo)/dd, tb = (h - oo)/dd; if (ta > tb) { float tt = ta; ta = tb; tb = tt; } t0 = Maxf(t0, ta); t1 = Minf(t1, tb); if (t0 > t1) return -1.0f; }
    }
    float y = o.y + d.y*t0; return (y >= k.y0 && y <= k.y1) ? t0 : -1.0f;
}

float World::RayCast(const Vec3& o, const Vec3& d, float maxT) const {
    float best = maxT;
    int c0x = ChunkOf(Minf(o.x, o.x + d.x*maxT) - 2), c1x = ChunkOf(Maxf(o.x, o.x + d.x*maxT) + 2);
    int c0z = ChunkOf(Minf(o.z, o.z + d.z*maxT) - 2), c1z = ChunkOf(Maxf(o.z, o.z + d.z*maxT) + 2);
    for (int cz = c0z; cz <= c1z; cz++) for (int cx = c0x; cx <= c1x; cx++) {
        const Chunk* ch = Get(cx, cz); if (!ch) continue;
        for (size_t i = 0; i < ch->cols.size(); i++) { float t = RayCollider(o, d, best, ch->cols[i]); if (t >= 0 && t < best) best = t; }
    }
    return best;
}
