// =====================================================================
//  world.h - mundo aberto em chunks (celulas) + streaming multi-thread
//
//  * Mundo: 256 x 256 chunks de 64 m  =  16,4 km x 16,4 km.
//  * Terreno e pontos de interesse (POI) sao funcoes PURAS da semente:
//    o mundo inteiro "existe" sem ocupar memoria; so os chunks perto do
//    jogador sao gerados (em threads de fundo) e mantidos na RAM.
//  * Cada chunk guarda: malha estatica (terreno + construcoes + arvores),
//    colisores, pontos de spawn de NPCs e containers de saque.
//  * Nada aqui toca em OpenGL: a thread principal desenha os vetores
//    prontos com vertex arrays (OpenGL 1.1).
// =====================================================================
#pragma once
#include "common.h"
#include "sys.h"
#include <unordered_map>

static const int   CHUNK_SIZE   = 64;
static const int   WORLD_CHUNKS = 256;
static const int   CELLS        = 16;                                  // celulas por lado (4 m cada)
static const float CELL_M       = (float)CHUNK_SIZE/(float)CELLS;
static const float WORLD_SIZE   = (float)(CHUNK_SIZE*WORLD_CHUNKS);
static const float WORLD_EDGE   = 160.0f;                              // margem intransponivel

enum PoiType {
    POI_NONE = 0, POI_RUINS, POI_CAMP, POI_RAIDERS, POI_SETTLEMENT, POI_GAS, POI_RADZONE, POI_WRECKS, POI_BUNKER,
    POI_RADIO, POI_VILLAGE_HOPE, POI_RAIDER_BASE, POI_COUNT
};

enum NpcKind { NK_SETTLER, NK_MERCHANT, NK_RAIDER, NK_DOG, NK_ASHEN, NK_GUARD, NK_ELDER, NK_CRAWLER, NK_BRUTE, NK_MECHANIC, NK_COUNT };
enum ContKind { CK_CRATE, CK_LOCKER, CK_TRUNK, CK_BODY, CK_SAFE, CK_TERMINAL, CK_ARTIFACT, CK_COUNT };
enum VehType { VT_BUGGY, VT_TRUCK, VT_COUNT };

struct Collider {                      // caixa orientada (OBB) ou circulo
    float x, z, hx, hz, c, s, y0, y1; bool circle;
};
struct SpawnPt  { u64 id; int kind; float x, z, yaw; };
struct Container{ u64 id; int kind; float x, y, z, yaw; int tier; };
struct VehiclePt{ u64 id; int type; float x, z, yaw, fuel; bool locked; };   // carros dirigiveis (viram entidades dinamicas)
struct Anomaly  { float x, y, z, r; int type; };                           // 0 = eletrica, 1 = termica

struct Chunk {
    int cx, cz;
    std::atomic<int> state;            // 0 = gerando (worker), 1 = pronto
    std::vector<float> pos, nrm;       // 3 floats por vertice
    std::vector<u8>    col;            // 3 bytes por vertice
    int vcount;
    std::vector<Collider>  cols;
    std::vector<SpawnPt>   spawns;
    std::vector<Container> conts;
    std::vector<VehiclePt> vehicles;
    std::vector<Anomaly>   anoms;
    std::vector<Vec3>      fires;      // fogueiras / chamas (desenhadas dinamicamente)
    int poi; Vec3 beacon; bool hasBeacon;
    float cenx, cenz, minY, maxY;
    bool spawned;
    Chunk() : cx(0), cz(0), state(0), vcount(0), poi(0), hasBeacon(false), cenx(0), cenz(0), minY(0), maxY(0), spawned(false) {}
};

// ---- funcoes puras (thread-safe) ----
void  WorldSetSeed(u32 seed);
u32   WorldSeed();
float TerrainH(float x, float z);
Vec3  TerrainNormal(float x, float z);
int   PoiAt(int cx, int cz);
float RoadFactor(float x, float z);    // 1 = no meio da estrada, 0 = fora
float RadAt(float x, float z);         // 0..1 (zona radioativa)
const char* PoiName(int poi, int cx, int cz);
const char* AreaName(float x, float z);  // nome da area onde o ponto esta
inline u64  ChunkKey(int cx, int cz) { return ((u64)(u32)cx << 32) | (u32)cz; }
inline u64  MakeId(int cx, int cz, int kind, int idx) { return ((u64)cx << 44) | ((u64)cz << 28) | ((u64)(kind & 0xFF) << 20) | (u64)(idx & 0xFFFFF); }
inline int  ChunkOf(float v) { return (int)floorf(v/(float)CHUNK_SIZE); }

// locais fixos da historia (em chunks)
static const int BUNKER_CX = 126, BUNKER_CZ = 126;
static const int HOPE_CX   = 128, HOPE_CZ   = 126;
static const int RAIDER_CX = 131, RAIDER_CZ = 122;
static const int RADIO_CX  = 122, RADIO_CZ  = 131;
static const int GASFIX_CX = 124, GASFIX_CZ = 122;
static const int ANOM_CX   = 121, ANOM_CZ   = 123;       // campo de fissuras (fixo, perto do inicio)
inline Vec3 ChunkCenter(int cx, int cz) { return Vec3((float)(cx*CHUNK_SIZE + CHUNK_SIZE/2), TerrainH((float)(cx*CHUNK_SIZE + 32), (float)(cz*CHUNK_SIZE + 32)), (float)(cz*CHUNK_SIZE + CHUNK_SIZE/2)); }

void GenerateChunk(Chunk& c);          // roda numa thread de trabalho

struct World {
    ThreadPool* pool;
    std::unordered_map<u64, Chunk*> map;
    int viewRadius;
    int statReady, statPending;

    World() : pool(0), viewRadius(5), statReady(0), statPending(0) {}
    void Init(ThreadPool* p, u32 seed);
    void Reset();                      // descarta tudo (espera os workers)
    void Update(const Vec3& center, const std::function<void(Chunk*)>& onReady, const std::function<void(Chunk*)>& onUnload);
    Chunk* Get(int cx, int cz) const;  // so devolve chunks prontos
    bool   NearReady(const Vec3& c, int r) const;

    // colisao 2D com construcoes/arvores. Ajusta (x,z); groundY = altura do chao/degrau sob o ponto.
    bool   Collide(float& x, float& z, float r, float feetY, float& groundY) const;
    // raio contra colisores + terreno: devolve distancia do impacto (ou maxT)
    float  RayCast(const Vec3& o, const Vec3& d, float maxT) const;
};
extern World W;
