// Teste de fumaca da camada grafica: roda RenderFrame/DrawHUD contra stubs de OpenGL
// (nao desenha nada, mas executa TODO o codigo de montagem de cena/HUD sob ASan/UBSan).
#include "../src/gfx.h"
#include <cstdlib>
#include <unistd.h>
static void Pump(int n) { for (int i = 0; i < n; i++) { if (G.saveReq) { std::vector<u8> t; int a, b; CaptureThumb(1280, 720, t, a, b); GameCommitSave(t, a, b); } GameFrame(1.0f/30); RenderFrame(1280, 720); DrawHUD(1280, 720); GameClearEdges(); SleepMs(1); } }
static void Settle() { int g = 0; while (G.state != GS_PLAYING && g++ < 3000) Pump(1); g = 0; while (G.ui == UI_DIALOG && g++ < 20) { G.in.ok = true; Pump(1); } }
int main() {
    char tmpl[] = "/tmp/rpggl_smokeXXXXXX"; setenv("RPGGL_HOME", mkdtemp(tmpl), 1);
    InitRender(); InitHUD((HDC)1, 900); GameInit();
    Pump(40); printf("menu ok (chunks=%d)\n", W.statReady);
    for (int ui : { UI_OPTIONS, UI_LOAD, UI_MAIN, UI_NEWGAME }) { UiOpen(ui); Pump(3); }
    G.nameBuf = "Teste"; GameNewGame("Teste"); Settle(); Pump(60); printf("jogo ok  (intro dialog fechado)\n");
    G.in.camToggle = true; Pump(5); G.in.camToggle = true; Pump(5);                 // 1a e 3a pessoa
    G.p.aimAmt = 1; G.in.fire = true; Pump(30); G.in.fire = false;
    UiOpen(UI_DIARY); for (int tab = 0; tab < 4; tab++) { G.tab = tab; for (int z = 0; z < 3; z++) { G.listSel = z; Pump(2); } }
    UiOpen(UI_NONE); UiOpen(UI_PAUSE); Pump(3); UiOpen(UI_SAVE); Pump(3); RequestSave(SK_MANUAL, ""); UiOpen(UI_NONE); Pump(3); while (G.saveBusy.load()) SleepMs(2); Pump(3); UiOpen(UI_LOAD); Pump(3); UiOpen(UI_NONE);
    Vehicle* v = 0; for (size_t i = 0; i < G.vehicles.size(); i++) if (!v || Len2D(G.vehicles[i]->pos - G.p.pos) < Len2D(v->pos - G.p.pos)) v = G.vehicles[i];
    if (v) { G.p.pos = v->pos + Vec3(2.5f, 0, 0); Pump(3); G.in.interact = true; Pump(2); G.in.moveY = 1; Pump(90); G.cam.first = true; Pump(10); G.cam.first = false; G.gameHours = 23.0f; Pump(20); G.in.moveY = 0; G.in.interact = true; Pump(2); printf("veiculo ok\n"); }
    G.gameHours = 22.0f; Pump(10); G.gameHours = 6.0f; Pump(10); G.gameHours = 12.0f; Pump(10);
    Vec3 ac = ChunkCenter(ANOM_CX, ANOM_CZ); G.p.pos = Vec3(ac.x, ac.y + 1, ac.z); G.state = GS_LOADING; G.loadT = 0; Settle(); Pump(40); printf("fissuras ok\n");
    G.quests[Q_SEEDS].state = 2; Vec3 hc = ChunkCenter(HOPE_CX, HOPE_CZ); G.p.pos = Vec3(hc.x, hc.y + 1, hc.z + 15); G.state = GS_LOADING; G.loadT = 0; Settle(); Pump(40); printf("esperanca (lavoura) ok\n");
    G.p.dead = true; UiOpen(UI_DEAD); Pump(5); g_showHelp = true; G.in.debug = true; Pump(3); G.in.debug = true; Pump(1);
    GameShutdown(); ShutdownHUD(); printf("RENDER SMOKE OK\n"); return 0;
}
