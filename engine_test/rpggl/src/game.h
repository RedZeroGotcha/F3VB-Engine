// =====================================================================
//  game.h - estado do jogo, entidades, itens, quests, entrada.
//  (independente de Windows/OpenGL: pode ser testado headless)
// =====================================================================
#pragma once
#include "world.h"
#include "ragdoll.h"
#include <unordered_set>

// ---------------------------------------------------------------------
//  ITENS
// ---------------------------------------------------------------------
enum ItemId {
    IT_NONE = 0, IT_PIPE, IT_PISTOL, IT_SHOTGUN, IT_RIFLE, IT_AMMO_10, IT_AMMO_SHELL, IT_AMMO_RIFLE,
    IT_MEDKIT, IT_WATER, IT_CANNED, IT_ANTIRAD, IT_JUNK, IT_ARMOR_LEATHER, IT_ARMOR_SCRAP, IT_TAPE, IT_FUEL, IT_ART_HEART, IT_ART_SPARK, IT_ART_CRYSTAL, IT_RATION, IT_MEAT, IT_SEED, IT_COUNT
};
enum ItemType { ITY_MELEE, ITY_GUN, ITY_AMMO, ITY_AID, ITY_JUNK, ITY_ARMOR, ITY_MISC, ITY_ARTIFACT };
struct ItemDef {
    const char* name; int type; float weight; int value;
    float dmg; int mag; int ammo; float rate; float reload; float range; float spread; int pellets;
    float heal; float rads; int dr; const char* desc;
};
const ItemDef& ItemInfo(int id);

struct Slot { int id, count; };

// ---------------------------------------------------------------------
//  JOGADOR / NPCs
// ---------------------------------------------------------------------
enum { AT_VIG, AT_PON, AT_FOL, AT_CON, AT_LAB, AT_SOB, AT_COUNT };      // Vigor, Pontaria, Folego, Constituicao, Labia, Sobrevivencia
extern const char* ATTR_NAMES[AT_COUNT];

struct Player {
    Vec3 pos, vel; float yaw, pitch;
    float hp, maxhp, stam, rad, hunger, thirst;
    int level, xp, points, money; int attr[AT_COUNT];
    std::vector<Slot> inv; int weapon, armor, artifact; int mag[IT_COUNT];
    bool onGround, sneak, sprinting, dead;
    float fireCd, reloadT, swingT, hurtT, recoil, aimAmt, animPhase, speed, bob, hitMarker, flash, lastHurtYaw, bodyYaw, stamDelay;
    int reloadW;
    char name[32];
    Player();
};

enum { NS_IDLE, NS_WANDER, NS_ALERT, NS_COMBAT, NS_FLEE, NS_DEAD };

struct Npc {
    u64 id; int kind; Vec3 pos, vel; float yaw, hp, maxhp, scale;
    int state; float t, cd, alert, animPhase, speed, deadT, hurtT, strafe, lostT;
    Vec3 home; bool alive, hostile, looted; Rng rng; int cx, cz; float dmgMul;
    // saida da IA para a thread principal (o worker nunca mexe no resto do jogo)
    int out; Vec3 fireFrom, fireTo; bool fireHit; float fireDmg; int level;
    Ragdoll* rag;                                     // criado na morte (PBD); a IA nunca o toca
    Npc(const Npc&) = delete; Npc& operator=(const Npc&) = delete; ~Npc() { delete rag; }
    Npc() : id(0), kind(0), yaw(0), hp(1), maxhp(1), scale(1), state(0), t(0), cd(0), alert(0), animPhase(0), speed(0), deadT(0), hurtT(0), strafe(1), lostT(0),
        alive(true), hostile(false), looted(false), cx(0), cz(0), dmgMul(1), out(0), fireHit(false), fireDmg(0), level(1), rag(0) {}
};
struct NpcStat { const char* name; float hp, walk, run, detect, dmg, reach, height, radius; bool hostile; int xp; };
const NpcStat& NpcInfo(int kind);

struct NpcOvr { float x, z, hp; u8 state; };

// ---------------------------------------------------------------------
//  QUESTS
// ---------------------------------------------------------------------
enum { Q_ARRIVE = 0, Q_RAIDERS, Q_RADIO, Q_WHEELS, Q_ARTIFACT, Q_SEEDS, Q_COUNT };
struct QuestState { int state, stage, count; };     // state: 0 nao iniciada, 1 ativa, 2 concluida
extern const char* QUEST_NAMES[Q_COUNT];

// ---------------------------------------------------------------------
//  ENTRADA (preenchida pela plataforma a cada quadro)
// ---------------------------------------------------------------------
struct Input {
    float moveX = 0, moveY = 0;          // -1..1 (Y>0 = frente)
    float lookDX = 0, lookDY = 0;        // radianos (mouse, ja escalados)
    float padLookX = 0, padLookY = 0;    // -1..1 (analogico direito)
    bool fire = false, aim = false, sprint = false, brake = false;   // seguradas (brake = freio de mao)
    bool jump = false, interact = false, reload = false, diary = false, quicksave = false, quickload = false, camToggle = false;
    bool nextWeapon = false, prevWeapon = false, bash = false, heal = false, sneakToggle = false, pause = false, debug = false;
    int  selectWeapon = 0;               // 1..4 ou 0
    float wheel = 0;
    // menus
    bool up = false, down = false, left = false, right = false, ok = false, back = false, del = false, tabL = false, tabR = false;
    std::string chars; bool backspace = false;
    bool usingPad = false;
};

// ---------------------------------------------------------------------
//  ESTADO GLOBAL
// ---------------------------------------------------------------------
enum GameState { GS_MENU, GS_LOADING, GS_PLAYING };
enum UiMode { UI_NONE, UI_MAIN, UI_NEWGAME, UI_DIARY, UI_DIALOG, UI_BARTER, UI_PAUSE, UI_SAVE, UI_LOAD, UI_OPTIONS, UI_DEAD, UI_LEVELUP };
enum SaveKind { SK_MANUAL = 0, SK_QUICK = 1, SK_AUTO = 2 };

struct Settings { float sens, fov, volume; bool invertY, showFps; int viewR; Settings() : sens(1.0f), fov(75.0f), volume(0.8f), invertY(false), showFps(false), viewR(5) {} };
struct Camera { Vec3 eye, fwd, right; float yaw, pitch, fov, dist; bool first; Camera() : yaw(0), pitch(0), fov(75), dist(3), first(false) {} };
struct Msg { std::string text; float t; int kind; };
struct Tracer { Vec3 a, b; float t; bool hit; int col = 0; };   // col: 0 tiro, 1 arco eletrico, 2 chama
struct Puff { Vec3 p; float t; int kind; };
struct Dialog { u64 npc; int kind; std::string speaker; std::vector<std::string> lines; int page; int onEnd; };
struct Target { int type; u64 id; float dist; Vec3 pos; char prompt[96]; };   // 0 nada, 1 NPC vivo, 2 container, 3 cadaver
struct BarterItem { int id; int price; };

// ---------------------------------------------------------------------
//  VEICULOS
// ---------------------------------------------------------------------
struct VehStat { const char* name; float maxSpeed, accel, brake, hp, length, halfW, wheelBase; };
const VehStat& VehInfo(int type);
struct Vehicle {
    u64 id; int type; Vec3 pos; float yaw, pitch, roll, speed, steer, vy, fuel, hp, wheelRot, throttle, smoke, prevGround;
    int state;                                  // 0 = funcional, 2 = destruido (carcaca)
    Vehicle() : id(0), type(0), yaw(0), pitch(0), roll(0), speed(0), steer(0), vy(0), fuel(50), hp(100), wheelRot(0), throttle(0), smoke(0), prevGround(0), state(0) {}
};
struct VehOvr { int type; float x, z, yaw, fuel, hp; u8 state; };

struct SaveInfo;

struct Game {
    int state, ui, prevUi;
    Settings set; Input in; Camera cam; Player p;
    std::vector<Npc*> npcs;
    float gameHours, playSeconds, simTime; int karma, kills;
    std::unordered_map<u64, float> killed, looted;
    std::unordered_map<u64, NpcOvr> npcOvr;
    std::vector<Vehicle*> vehicles; std::unordered_map<u64, VehOvr> vehOvr; Vehicle* drive; float camIdle; float vehHit; bool introPending; int ragActive;
    std::unordered_set<u64> discovered;
    QuestState quests[Q_COUNT];
    // interface
    int menuSel, tab, scroll; std::string nameBuf; float uiT;
    int listSel; bool confirmDel;
    Dialog dlg; std::vector<BarterItem> shop; int shopCol, shopSel[2]; u64 shopNpc;
    std::vector<Msg> msgs; std::vector<Tracer> tracers; std::vector<Puff> puffs;
    Target tgt; std::string location; float locT; std::string lastLocation;
    // saves (a gravacao em si roda numa thread de fundo)
    int saveReq, saveReqKind, saveReqSlot; std::string saveReqName; int quickIdx, autoIdx, saveCounter;
    std::atomic<int> saveBusy, saveOk, saveFail; float autoT; bool hasSaves;
    std::string pendingLoad; float loadT;
    // mundo
    ThreadPool pool; float fps, shotT; Vec3 shotPos; int npcActive; float menuCamT; bool quit;
    Game();
};
extern Game G;

// ---------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------
enum Snd {
    SND_PISTOL, SND_SHOTGUN, SND_RIFLE, SND_SWING, SND_HIT, SND_STEP, SND_PICKUP, SND_CLICK, SND_EMPTY, SND_RELOAD, SND_HURT, SND_DEATH,
    SND_LEVEL, SND_SAVE, SND_QUEST, SND_GEIGER, SND_GROWL, SND_BARK, SND_HEAL, SND_BULLETHIT, SND_ZAP, SND_EXPLODE, SND_CRASH, SND_COUNT
};
void PlaySfx(int id, float vol);                   // audio.cpp (ou stub nos testes)
void SetAmbience(float wind, float geiger);
void SetEngine(bool on, float rpm01);             // som continuo do motor do veiculo

// game.cpp
void GameInit();
void GameShutdown();
void GameNewGame(const char* name);
void GameFrame(float dt);                          // 1x por quadro
void GameClearEdges();                             // apos o quadro: zera os eventos de "um disparo"
void Notify(const std::string& s, int kind = 0);
float TimeOfDay();                                 // 0..24
int  AddItem(int id, int count);
int  CountItem(int id);
bool RemoveItem(int id, int count);
float CarryWeight();
float MaxCarry();
float EffMaxHp();
enum QuestEvent { QE_TALK_ELDER = 1, QE_KILL_RAIDER, QE_TERMINAL, QE_TALK_MECHANIC };
void QuestAdvance(int ev, int arg);
const char* QuestObjective(int q);
const char* QuestDesc(int q);
void StartDialog(Npc* n);
void FinishDialog();
void OpenBarter(u64 npcId);
void BarterTrade();
int  BuyPrice(int id);
int  SellPrice(int id);
void GameClearWorld();
void GameFinishLoadSetup();
void UseItem(int invIndex);
void EquipWeapon(int id);
void GainXp(int xp);
void RespawnAfterDeath();
void TestKillNpc(Npc* n, const Vec3& dir, float force);      // so para testes
float NightFactor();
bool QuestMarker(int q, Vec3& out);
void ApplyOptions();
void SpawnChunkNpcs(Chunk* c);
void SpawnChunkVehicles(Chunk* c);
Vehicle* NearestVehicle(const Vec3& p, float maxDist);
void ExitVehicle();
float ArtifactBonus(int kind);
void DespawnChunkNpcs(Chunk* c);

// save.cpp
struct SaveInfo {
    std::string path, file; u32 version; u64 stamp; u32 number; float playSeconds; int level; char name[32]; char location[48]; float gameHours; char kind;
    int tw, th; std::vector<u8> thumb; bool valid;
};
std::string SaveDir();
std::vector<SaveInfo> ListSaves(bool withThumbs);
void GameCommitSave(const std::vector<u8>& thumbRgb, int tw, int th);     // chamado pela plataforma (apos desenhar o quadro)
bool GameLoadFile(const std::string& path);
bool DeleteSave(const std::string& path);
void RequestSave(int kind, const std::string& overwritePath);
void LoadSettings();
void SaveSettings();
void RefreshSaveList();
extern std::vector<SaveInfo> g_saveList;

// ui.cpp
void UiUpdate(float dt);
void UiOpen(int ui);

bool IsContainerLooted(u64 id);
