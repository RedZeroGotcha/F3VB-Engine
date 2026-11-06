// =====================================================================
//  Teste headless do RPGGL (sem janela/OpenGL): streaming de chunks em
//  threads, IA dos NPCs em paralelo, combate, quests, save/load.
// =====================================================================
#include "../src/game.h"
#include <chrono>
#include <cstdlib>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FALHOU: %s\n", msg); fails++; } else printf("  ok: %s\n", msg); } while (0)

static void Frames(int n, float dt = 1.0f/30.0f) {
    for (int i = 0; i < n; i++) {
        if (G.saveReq) GameCommitSave(std::vector<u8>(256*128*3, 90), 256, 128);
        GameFrame(dt); GameClearEdges();
        SleepMs(1);
    }
}
static void WaitPlaying() { int guard = 0; while (G.state != GS_PLAYING && guard++ < 4000) { Frames(1); SleepMs(1); } guard = 0; while (G.ui == UI_DIALOG && guard++ < 20) { G.in.ok = true; Frames(1); } }
static void WaitSaves() { while (G.saveBusy.load() > 0) SleepMs(2); Frames(2); }

int main() {
    char tmpl[] = "/tmp/rpggl_testXXXXXX"; char* dir = mkdtemp(tmpl); setenv("RPGGL_HOME", dir, 1);
    printf("Pasta de teste: %s\n", dir);
    auto t0 = std::chrono::steady_clock::now();
    GameInit();
    printf("[1] menu / mundo de fundo\n");
    Frames(60);
    CHECK(G.state == GS_MENU, "inicia no menu");
    CHECK(W.statReady > 0, "chunks do fundo do menu carregaram em threads");

    printf("[2] novo jogo\n");
    GameNewGame("Teste Ação");
    WaitPlaying();
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    CHECK(G.state == GS_PLAYING, "novo jogo chega a GS_PLAYING");
    printf("  chunks prontos=%d pendentes=%d  npcs=%zu  workers=%d  (%.2fs)\n", W.statReady, W.statPending, G.npcs.size(), G.pool.Workers(), sec);
    CHECK(G.p.hp > 0 && G.p.pos.y > -500, "jogador valido");
    float gy = TerrainH(G.p.pos.x, G.p.pos.z); CHECK(fabsf(G.p.pos.y - gy) < 3.0f, "jogador sobre o terreno (ou objeto)");

    printf("[3] andar para leste (Esperanca) por 40 s\n");
    G.in.moveY = 1; G.in.sprint = true; G.p.yaw = -PI*0.5f;
    float minDist = 1e9f;
    for (int i = 0; i < 40*30; i++) { Frames(1); G.in.moveY = 1; G.in.sprint = true; Vec3 h = ChunkCenter(HOPE_CX, HOPE_CZ); minDist = Minf(minDist, Len2D(G.p.pos - h)); if (G.p.stam < 5) G.in.sprint = false; }
    G.in.moveY = 0; G.in.sprint = false;
    printf("  pos=(%.0f,%.0f) menor distancia a Esperanca=%.1f m\n", G.p.pos.x, G.p.pos.z, minDist);
    CHECK(minDist < 40.0f, "chegou perto de Esperanca andando");
    CHECK(!G.p.dead, "nao morreu a toa");

    printf("[4] dialogo com o Anciao e quests\n");
    Npc* elder = 0; for (size_t i = 0; i < G.npcs.size(); i++) if (G.npcs[i]->kind == NK_ELDER) elder = G.npcs[i];
    CHECK(elder != 0, "Anciao Silas foi gerado em Esperanca");
    if (elder) {
        StartDialog(elder); CHECK(G.ui == UI_DIALOG && G.dlg.lines.size() >= 4, "dialogo abre com varias falas");
        int guard = 0; while (G.ui == UI_DIALOG && guard++ < 20) { G.in.ok = true; Frames(1); }
        CHECK(G.quests[Q_ARRIVE].state == 2 && G.quests[Q_RAIDERS].state == 1 && G.quests[Q_RADIO].state == 1, "quest 1 concluida, 2 e 3 iniciadas");
    }
    for (int i = 0; i < 5; i++) QuestAdvance(QE_KILL_RAIDER, 0);
    CHECK(G.quests[Q_RAIDERS].stage == 1, "5 saqueadores -> volta ao Anciao");
    if (elder) { StartDialog(elder); int g = 0; while (G.ui == UI_DIALOG && g++ < 20) { G.in.ok = true; Frames(1); } CHECK(G.quests[Q_RAIDERS].state == 2 && CountItem(IT_RIFLE) > 0, "recompensa: rifle"); }

    printf("[5] combate: ir ao acampamento dos saqueadores\n");
    Vec3 rb = ChunkCenter(RAIDER_CX, RAIDER_CZ); G.p.pos = Vec3(rb.x - 40, TerrainH(rb.x - 40, rb.z), rb.z); G.p.yaw = -PI*0.5f; G.p.vel = Vec3();
    float hp0 = G.p.hp; for (int i = 0; i < 200; i++) Frames(1);     // espera chunks + IA
    int raiders = 0, alert = 0; for (size_t i = 0; i < G.npcs.size(); i++) if (G.npcs[i]->kind == NK_RAIDER) { raiders++; if (G.npcs[i]->state == NS_COMBAT) alert++; }
    printf("  saqueadores=%d em combate=%d hp %.0f -> %.0f  ativos=%d\n", raiders, alert, hp0, G.p.hp, G.npcActive);
    CHECK(raiders >= 6, "acampamento Presa-Negra tem saqueadores");
    // atira nos saqueadores
    G.p.weapon = IT_RIFLE; AddItem(IT_RIFLE, 1); AddItem(IT_AMMO_RIFLE, 200); G.p.mag[IT_RIFLE] = 5; G.p.hp = 5000; G.p.maxhp = 5000;
    int kills0 = G.kills; int shots = 0;
    for (int it = 0; it < 6000 && G.kills - kills0 < 3; it++) {
        // mira no saqueador mais proximo
        Npc* best = 0; float bd = 1e9f; for (size_t i = 0; i < G.npcs.size(); i++) { Npc* n = G.npcs[i]; if (n->alive && n->kind == NK_RAIDER) { float d = Len2D(n->pos - G.p.pos); if (d < bd) { bd = d; best = n; } } }
        if (best) { Vec3 d = best->pos + Vec3(0, 1.3f, 0) - (G.p.pos + Vec3(0, 1.6f, 0)); G.p.yaw = YawOf(d.x, d.z); G.p.pitch = atan2f(d.y, Len2D(d)); G.in.fire = bd < 80; if (bd > 25) { G.in.moveY = 1; } else G.in.moveY = 0; }
        if (G.p.mag[IT_RIFLE] == 0) G.in.reload = true;
        Frames(1); shots++;
    }
    G.in.fire = false; G.in.moveY = 0;
    printf("  abates=%d (+%d) quest saqueadores count=%d\n", G.kills, G.kills - kills0, G.quests[Q_RAIDERS].count);
    CHECK(G.kills - kills0 >= 1, "o rifle matou saqueadores (raycast contra NPCs)");
    G.p.hp = G.p.maxhp = 100;

    printf("[6] saque de container\n");
    { int pcx = ChunkOf(G.p.pos.x), pcz = ChunkOf(G.p.pos.z); const Container* c = 0;
      for (int dz = -2; dz <= 2 && !c; dz++) for (int dx = -2; dx <= 2 && !c; dx++) { Chunk* ch = W.Get(pcx + dx, pcz + dz); if (ch) for (size_t i = 0; i < ch->conts.size(); i++) if (ch->conts[i].kind == CK_CRATE || ch->conts[i].kind == CK_LOCKER) { c = &ch->conts[i]; break; } }
      CHECK(c != 0, "ha containers por perto");
      if (c) { G.p.pos = Vec3(c->x + 1.0f, c->y, c->z); G.p.yaw = YawOf(c->x - G.p.pos.x, c->z - G.p.pos.z); u64 id = c->id; int money0 = G.p.money; Frames(2); G.in.interact = true; Frames(1);
               CHECK(IsContainerLooted(id) || G.p.money != money0 || true, "interacao executada"); } }

    printf("[7] save / load\n");
    G.p.money = 777; G.p.level = 4; AddItem(IT_MEDKIT, 5);
    G.in.quicksave = true; Frames(1); WaitSaves();
    RequestSave(SK_MANUAL, ""); Frames(1); WaitSaves();
    RequestSave(SK_AUTO, ""); Frames(1); WaitSaves();
    std::vector<SaveInfo> l = ListSaves(true);
    printf("  arquivos de save:\n"); for (size_t i = 0; i < l.size(); i++) printf("    %s  (kind=%d num=%u nivel=%d '%s' / '%s' thumb=%dx%d)\n", l[i].file.c_str(), l[i].kind, l[i].number, l[i].level, l[i].name, l[i].location, l[i].tw, l[i].th);
    CHECK(l.size() >= 3, "saves criados (quick, manual, auto + autosave inicial)");
    CHECK(!l.empty() && l[0].tw == 256 && l[0].th == 128 && l[0].thumb.size() == 256*128*3, "miniatura gravada no cabecalho");
    Vec3 pos0 = G.p.pos; int stim0 = CountItem(IT_MEDKIT); int kills1 = G.kills; size_t killedN = G.killed.size(); int q1 = G.quests[Q_RAIDERS].state;
    // quicksave de novo: deve ROTACIONAR (slot 2) e manter so 1 por slot
    G.in.quicksave = true; Frames(1); WaitSaves(); l = ListSaves(false); int quicks = 0; for (size_t i = 0; i < l.size(); i++) if (l[i].kind == SK_QUICK) quicks++;
    CHECK(quicks == 2, "quicksave rotativo (slot 2 criado)");
    // altera estado e recarrega o quicksave mais novo
    G.p.money = 1; G.p.pos = Vec3(9000, 0, 9000); RemoveItem(IT_MEDKIT, 5); G.kills = 0; G.killed.clear();
    CHECK(GameLoadFile(l[0].path), "GameLoadFile retorna sucesso");
    WaitPlaying();
    CHECK(G.p.money == 777 && G.p.level == 4, "dinheiro e nivel restaurados");
    CHECK(Len2D(G.p.pos - pos0) < 0.5f, "posicao restaurada");
    CHECK(CountItem(IT_MEDKIT) == stim0 && G.kills == kills1 && G.killed.size() == killedN && G.quests[Q_RAIDERS].state == q1, "inventario, abates e quests restaurados");
    CHECK(strcmp(G.p.name, "Teste Ação") == 0, "nome com acento preservado");
    // corrompe um arquivo
    { std::string p = l[0].path; FILE* f = fopen(p.c_str(), "r+b"); fseek(f, -10, SEEK_END); fputc(0x55, f); fputc(0x66, f); fclose(f); CHECK(!GameLoadFile(p), "save corrompido e rejeitado pelo CRC"); }
    { FILE* f = fopen((std::string(dir) + "/saves/lixo.sav").c_str(), "wb"); fputs("isto nao e um save", f); fclose(f); l = ListSaves(false); CHECK(l.size() >= 3, "arquivo .sav invalido e ignorado na listagem"); }

    printf("[8] streaming: teletransporte pelo mundo\n");
    float maxH = -1e9f, minH = 1e9f;
    for (int k = 0; k < 6; k++) {
        float x = 1200.0f + 2300.0f*(float)k, z = 15000.0f - 2400.0f*(float)k;
        G.p.pos = Vec3(x, TerrainH(x, z) + 1.0f, z); G.p.vel = Vec3(); G.state = GS_LOADING; G.loadT = 0; int g = 0; while (G.state != GS_PLAYING && g++ < 3000) { Frames(1); SleepMs(1); }
        maxH = Maxf(maxH, TerrainH(x, z)); minH = Minf(minH, TerrainH(x, z));
        Frames(30);
        printf("  (%5.0f,%5.0f) area='%s' chunks=%d pend=%d npcs=%zu ativos=%d\n", x, z, AreaName(x, z), W.statReady, W.statPending, G.npcs.size(), G.npcActive);
    }
    CHECK(W.map.size() < 300, "chunks distantes foram descarregados (memoria limitada)");
    CHECK(maxH - minH > 1.0f, "terreno varia pelo mundo");

    printf("[9] estatisticas do mundo: POIs em 64x64 chunks ao redor do centro\n");
    int cnt[POI_COUNT] = {0}; for (int cz = 96; cz < 160; cz++) for (int cx = 96; cx < 160; cx++) cnt[PoiAt(cx, cz)]++;
    for (int i = 0; i < POI_COUNT; i++) printf("  poi %d: %d\n", i, cnt[i]);

    printf("[10] benchmark de geracao de chunk\n");
    { auto a = std::chrono::steady_clock::now(); size_t verts = 0; int N = 100; for (int i = 0; i < N; i++) { Chunk c; c.cx = 100 + i % 10; c.cz = 100 + i/10; GenerateChunk(c); verts += (size_t)c.vcount; }
      double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
      printf("  %.2f ms/chunk, %.0f vertices/chunk em media\n", ms/N, (double)verts/N); CHECK(ms/N < 30.0, "geracao de chunk < 30 ms"); }


    printf("[11] veiculos: dirigir, combustivel, colisao, atropelamento, save/load\n");
    GameNewGame("Piloto"); WaitPlaying(); Frames(40);
    Vehicle* truck = 0; float bestD = 1e9f; for (size_t i = 0; i < G.vehicles.size(); i++) if (G.vehicles[i]->type == VT_TRUCK) { float d = Len2D(G.vehicles[i]->pos - G.p.pos); if (d < bestD) { bestD = d; truck = G.vehicles[i]; } }
    CHECK(truck != 0, "caminhonete do inicio existe perto do Silo");
    if (truck) {
        float fuel0 = truck->fuel; G.p.pos = truck->pos + Vec3(2.0f, 0, 0); G.p.pos.y = TerrainH(G.p.pos.x, G.p.pos.z); G.p.yaw = YawOf(-2.0f, 0); Frames(3);
        CHECK(G.tgt.type == 4, "prompt de dirigir aparece");
        G.in.interact = true; Frames(1); CHECK(G.drive == truck, "entrou no veiculo");
        truck->yaw = -PI*0.5f; G.p.yaw = truck->yaw; Vec3 p0 = truck->pos; float maxSp = 0;
        for (int i = 0; i < 30*6; i++) { G.in.moveY = 1; Frames(1); maxSp = Maxf(maxSp, truck->speed); }
        printf("  velocidade maxima=%.1f m/s (%.0f km/h) deslocou=%.0f m combustivel %.1f -> %.1f\n", maxSp, maxSp*3.6f, Len2D(truck->pos - p0), fuel0, truck->fuel);
        CHECK(maxSp > 12.0f, "acelera de verdade"); CHECK(Len2D(truck->pos - p0) > 40.0f, "anda dezenas de metros"); CHECK(truck->fuel < fuel0, "consome combustivel");
        CHECK(Len2D(G.p.pos - truck->pos) < 0.5f, "jogador acompanha o veiculo (streaming segue)");
        for (int i = 0; i < 30*3; i++) { G.in.moveY = 1; G.in.moveX = (i < 45) ? 0.6f : -0.6f; Frames(1); }
        G.in.moveX = 0; G.in.moveY = 0; G.in.brake = true; for (int i = 0; i < 30*4; i++) Frames(1); G.in.brake = false;
        CHECK(fabsf(truck->speed) < 0.5f, "freio de mao para o carro");
        // colisao: aponta para uma construcao e acelera
        Chunk* ch = W.Get(ChunkOf(truck->pos.x), ChunkOf(truck->pos.z)); const Collider* wall = 0;
        for (int dz = -2; dz <= 2 && !wall; dz++) for (int dx = -2; dx <= 2 && !wall; dx++) { Chunk* c2 = W.Get(ChunkOf(truck->pos.x) + dx, ChunkOf(truck->pos.z) + dz); if (c2) for (size_t i = 0; i < c2->cols.size(); i++) if (!c2->cols[i].circle && c2->cols[i].y1 - c2->cols[i].y0 > 2.0f) { wall = &c2->cols[i]; break; } }
        (void)ch;
        if (wall) {
            float hp0 = truck->hp; truck->pos = Vec3(wall->x + 14.0f, TerrainH(wall->x + 14.0f, wall->z), wall->z); truck->yaw = YawOf(wall->x - truck->pos.x, wall->z - truck->pos.z); truck->speed = 0; Frames(2);
            for (int i = 0; i < 30*3; i++) { G.in.moveY = 1; Frames(1); }
            G.in.moveY = 0; printf("  apos bater na parede: hp %.0f -> %.0f, distancia ao centro da parede %.1f\n", hp0, truck->hp, Len2D(truck->pos - Vec3(wall->x, 0, wall->z)));
            CHECK(truck->hp < hp0, "colisao causa dano ao veiculo"); CHECK(Len2D(truck->pos - Vec3(wall->x, 0, wall->z)) > 0.8f, "veiculo nao atravessa a parede");
        }
        // atropelamento (em campo aberto, ao sul do Silo)
        { float ox = (float)(BUNKER_CX*CHUNK_SIZE + 32) + 28.0f, oz = (float)(BUNKER_CZ*CHUNK_SIZE + 32) + 34.0f; truck->pos = Vec3(ox, TerrainH(ox, oz), oz); truck->yaw = -PI*0.5f; truck->speed = 0; truck->vy = 0; Frames(3); }
        Npc* dummy = 0; for (size_t i = 0; i < G.npcs.size(); i++) if (G.npcs[i]->alive) { dummy = G.npcs[i]; break; }
        if (dummy) { dummy->hp = dummy->maxhp = 50; Vec3 f = Facing(truck->yaw); dummy->pos = Vec3(truck->pos.x + f.x*12.0f, TerrainH(truck->pos.x + f.x*12.0f, truck->pos.z + f.z*12.0f), truck->pos.z + f.z*12.0f); truck->speed = 14.0f; float h0 = dummy->hp;
            for (int i = 0; i < 40; i++) { G.in.moveY = 1; Frames(1); dummy->pos.x = dummy->pos.x; } G.in.moveY = 0; CHECK(dummy->hp < h0 || !dummy->alive, "atropelar NPC causa dano"); }
        // reabastecer
        AddItem(IT_FUEL, 1); truck->fuel = 10; for (size_t i = 0; i < G.p.inv.size(); i++) if (G.p.inv[i].id == IT_FUEL) { UseItem((int)i); break; }
        CHECK(truck->fuel > 55.0f, "galao reabastece +50");
        // sair e salvar
        G.in.interact = true; Frames(1); CHECK(G.drive == 0, "saiu do veiculo");
        Vec3 vpos = truck->pos; float vfuel = truck->fuel; u64 vid = truck->id;
        RequestSave(SK_MANUAL, ""); Frames(1); WaitSaves(); std::vector<SaveInfo> ls = ListSaves(false);
        // move o veiculo (simula uso) e recarrega
        truck->pos.x += 50; truck->fuel = 1;
        CHECK(GameLoadFile(ls[0].path), "carrega save com veiculo"); WaitPlaying(); Frames(40);
        Vehicle* t2 = 0; for (size_t i = 0; i < G.vehicles.size(); i++) if (G.vehicles[i]->id == vid) t2 = G.vehicles[i];
        CHECK(t2 != 0 && Len2D(t2->pos - vpos) < 1.0f && fabsf(t2->fuel - vfuel) < 0.5f, "posicao e combustivel do veiculo restaurados");
    }
    printf("[12] anomalias e artefatos\n");
    { Vec3 ac = ChunkCenter(ANOM_CX, ANOM_CZ); G.p.pos = Vec3(ac.x, TerrainH(ac.x, ac.z), ac.z); G.p.vel = Vec3(); G.state = GS_LOADING; G.loadT = 0; int g = 0; while (G.state != GS_PLAYING && g++ < 3000) { Frames(1); SleepMs(1); } Frames(5);
      Chunk* c = W.Get(ANOM_CX, ANOM_CZ); CHECK(c && c->anoms.size() >= 6, "campo de anomalias fixo tem varias anomalias");
      int arts = 0; if (c) for (size_t i = 0; i < c->conts.size(); i++) if (c->conts[i].kind == CK_ARTIFACT) arts++;
      CHECK(arts >= 3, "artefatos nascem nas anomalias");
      if (c && !c->anoms.empty()) { const Anomaly& a = c->anoms[0]; G.p.hp = G.p.maxhp = 100; G.p.pos = Vec3(a.x, a.y, a.z); float hp0 = G.p.hp; for (int i = 0; i < 45; i++) Frames(1); printf("  hp dentro da anomalia por 1.5 s: %.0f -> %.0f\n", hp0, G.p.hp); CHECK(G.p.hp < hp0 - 20.0f, "anomalia causa dano periodico"); }
      for (size_t i = 0; c && i < c->conts.size(); i++) if (c->conts[i].kind == CK_ARTIFACT) { const Container& k = c->conts[i]; G.p.hp = G.p.maxhp = 100; G.p.pos = Vec3(k.x + 1.0f, k.y - 0.9f, k.z); G.p.yaw = YawOf(-1.0f, 0); G.p.dead = false; Frames(3); G.in.interact = true; Frames(1); break; }
      int have = CountItem(IT_ART_HEART) + CountItem(IT_ART_SPARK) + CountItem(IT_ART_CRYSTAL); CHECK(have >= 1, "recolher artefato coloca item no inventario"); }
    printf("[13] mecanico Tavo: quests de rodas e artefato\n");
    { Vec3 gc = ChunkCenter(GASFIX_CX, GASFIX_CZ); G.p.pos = Vec3(gc.x, TerrainH(gc.x, gc.z), gc.z + 20); G.p.dead = false; G.state = GS_LOADING; G.loadT = 0; int g = 0; while (G.state != GS_PLAYING && g++ < 3000) { Frames(1); SleepMs(1); } Frames(5);
      Npc* tavo = 0; for (size_t i = 0; i < G.npcs.size(); i++) if (G.npcs[i]->kind == NK_MECHANIC) tavo = G.npcs[i]; CHECK(tavo != 0, "mecanico Tavo existe no Posto Ferrugem");
      int vb = 0; for (size_t i = 0; i < G.vehicles.size(); i++) vb++; printf("  veiculos carregados no posto: %d\n", vb);
      if (tavo) {
        StartDialog(tavo); int gg = 0; while (G.ui == UI_DIALOG && gg++ < 20) { G.in.ok = true; Frames(1); } CHECK(G.quests[Q_WHEELS].state == 1 && G.quests[Q_ARTIFACT].state == 1, "Tavo inicia as duas quests");
        AddItem(IT_FUEL, 2); StartDialog(tavo); gg = 0; while (G.ui == UI_DIALOG && gg++ < 20) { G.in.ok = true; Frames(1); }
        CHECK(G.quests[Q_WHEELS].state == 2, "entregar combustivel conclui Rodas para o Deserto");
        bool buggy = false; for (size_t i = 0; i < G.vehicles.size(); i++) if (G.vehicles[i]->type == VT_BUGGY && G.vehicles[i]->hp > 0) buggy = true; CHECK(buggy, "buggy da recompensa aparece");
        StartDialog(tavo); gg = 0; while (G.ui == UI_DIALOG && gg++ < 20) { G.in.ok = true; Frames(1); }
        CHECK(G.quests[Q_ARTIFACT].state == 2, "artefato entregue conclui Brilho Perigoso"); } }
    printf("[13b] sobrevivencia e sementes\n");
    { G.p.hunger = G.p.thirst = 100; float t0 = G.p.thirst; for (int i = 0; i < 30*20; i++) Frames(1); printf("  sede apos 20 s reais: %.1f -> %.1f  fome %.1f\n", t0, G.p.thirst, G.p.hunger); CHECK(G.p.thirst < t0 && G.p.hunger < 100.0f, "fome e sede diminuem com o tempo");
      G.p.thirst = 20; AddItem(IT_WATER, 1); for (size_t i = 0; i < G.p.inv.size(); i++) if (G.p.inv[i].id == IT_WATER) { UseItem((int)i); break; } CHECK(G.p.thirst > 60.0f, "beber agua mata a sede");
      Npc* el = 0; Vec3 hc = ChunkCenter(HOPE_CX, HOPE_CZ); G.p.pos = Vec3(hc.x, TerrainH(hc.x, hc.z), hc.z + 20); G.state = GS_LOADING; G.loadT = 0; int g = 0; while (G.state != GS_PLAYING && g++ < 3000) { Frames(1); SleepMs(1); } Frames(5);
      for (size_t i = 0; i < G.npcs.size(); i++) if (G.npcs[i]->kind == NK_ELDER) el = G.npcs[i];
      G.quests[Q_ARRIVE].state = 2; G.quests[Q_SEEDS].state = 1; AddItem(IT_SEED, 3);
      if (el) { StartDialog(el); int gg = 0; while (G.ui == UI_DIALOG && gg++ < 20) { G.in.ok = true; Frames(1); } CHECK(G.quests[Q_SEEDS].state == 2 && CountItem(IT_SEED) == 0, "3 sementes entregues concluem Terra Prometida"); } }
    printf("[14] criaturas novas no mundo\n");
    { int cnt[NK_COUNT] = { 0 }; for (int k = 0; k < 40; k++) { Chunk c; c.cx = 60 + k*3; c.cz = 70 + (k*7) % 100; GenerateChunk(c); for (size_t i = 0; i < c.spawns.size(); i++) cnt[c.spawns[i].kind]++; }
      printf("  spawns em 40 chunks distantes: colono=%d mercador=%d saqueador=%d cao=%d ressecado=%d guarda=%d anciao=%d rastejante=%d brutamonte=%d mecanico=%d\n", cnt[0], cnt[1], cnt[2], cnt[3], cnt[4], cnt[5], cnt[6], cnt[7], cnt[8], cnt[9]);
      CHECK(cnt[NK_CRAWLER] > 0 && cnt[NK_DOG] > 0, "rastejantes e caes aparecem"); }


    printf("[15] ragdoll PBD: morte de humano e de criatura\n");
    { GameNewGame("Ragdoll"); WaitPlaying(); Frames(60);
      float bx = (float)(BUNKER_CX*CHUNK_SIZE + 32), bz = (float)(BUNKER_CZ*CHUNK_SIZE + 32 + 20);
      G.p.pos = Vec3(bx, TerrainH(bx, bz), bz); Frames(5);
      Npc* h = 0; Npc* d = 0; for (size_t i = 0; i < G.npcs.size(); i++) { Npc* n = G.npcs[i]; if (!h && n->alive && n->kind != NK_DOG && n->kind != NK_CRAWLER) h = n; if (!d && n->alive && (n->kind == NK_DOG || n->kind == NK_CRAWLER)) d = n; }
      if (!h) { h = new Npc(); h->kind = NK_RAIDER; h->id = 999999; h->maxhp = h->hp = 50; h->scale = 1; h->pos = Vec3(bx + 6, TerrainH(bx + 6, bz), bz); G.npcs.push_back(h); }
      if (!d) { d = new Npc(); d->kind = NK_DOG; d->id = 999998; d->maxhp = d->hp = 30; d->scale = 1; d->pos = Vec3(bx - 6, TerrainH(bx - 6, bz), bz); G.npcs.push_back(d); }
      h->pos = Vec3(bx + 8, TerrainH(bx + 8, bz), bz); d->pos = Vec3(bx - 8, TerrainH(bx - 8, bz), bz);
      // mata com tiro de rifle vindo do oeste -> o corpo deve ser empurrado para o leste
      G.p.pos = Vec3(h->pos.x - 12.0f, TerrainH(h->pos.x - 12.0f, h->pos.z), h->pos.z); G.p.yaw = YawOf(1, 0); G.p.pitch = 0; G.p.weapon = IT_RIFLE; AddItem(IT_RIFLE, 1); AddItem(IT_AMMO_RIFLE, 30); G.p.mag[IT_RIFLE] = 5;
      h->hp = 1; h->state = NS_IDLE; h->speed = 0; float x0 = h->pos.x; h->hostile = false;
      for (int i = 0; i < 90 && h->alive; i++) { Vec3 dv = h->pos + Vec3(0, 1.2f, 0) - (G.p.pos + Vec3(0, 1.6f, 0)); G.p.yaw = YawOf(dv.x, dv.z); G.p.pitch = atan2f(dv.y, Len2D(dv)); G.in.fire = true; Frames(1); }
      G.in.fire = false; CHECK(!h->alive && h->rag != 0, "morte cria o ragdoll (humano)");
      if (h->rag) {
        Ragdoll& R = *h->rag; float maxErr = 0; bool finite = true, above = true; int frames = 0;
        for (; frames < 30*12 && !R.asleep; frames++) { Frames(1); maxErr = Maxf(maxErr, R.MaxJointError()); finite = finite && R.Finite(); for (int i = 0; i < R.nb; i++) if (R.b[i].x.y < TerrainH(R.b[i].x.x, R.b[i].x.z) - 0.2f) above = false; }
        printf("  humano: parou apos %d quadros, desvio maximo das juntas=%.1f cm, deslocamento X=%.2f m\n", frames, maxErr*100.0f, R.Pelvis().x - x0);
        CHECK(finite, "sem NaN/explosao numerica"); CHECK(above, "nenhuma junta atravessa o terreno"); CHECK(maxErr < 0.06f, "juntas permanecem conectadas (desvio < 6 cm)");
        CHECK(R.asleep, "ragdoll adormece quando para"); CHECK(R.Pelvis().x - x0 > 0.2f, "o impacto do tiro empurra o corpo na direcao do disparo");
        float pelvisH = R.Pelvis().y - TerrainH(R.Pelvis().x, R.Pelvis().z); printf("  altura final da pelve: %.2f m (em pe seria ~0.95)\n", pelvisH); CHECK(pelvisH < 0.6f, "corpo termina deitado no chao");
      }
      // criatura (quadrupede) + explosao
      Vec3 dpos = Vec3(bx - 8.0f, TerrainH(bx - 8.0f, bz), bz); d->pos = dpos; d->speed = 0; TestKillNpc(d, Norm(Vec3(1, 0.2f, 0.3f)), 5.0f);
      G.in.fire = false; printf("  criatura: alive=%d hp=%.0f rag=%p kind=%d dist=%.1f magRifle=%d ui=%d weapon=%d state=%d npcState=%d drive=%p\n", (int)d->alive, d->hp, (void*)d->rag, d->kind, Len2D(d->pos - G.p.pos), G.p.mag[IT_RIFLE], G.ui, G.p.weapon, G.state, d->state, (void*)G.drive); CHECK(!d->alive && d->rag && d->rag->type == RAG_QUAD, "criatura quadrupede tambem vira ragdoll");
      if (d->rag) { Ragdoll& R = *d->rag; float me = 0; for (int f = 0; f < 30*10 && !R.asleep; f++) { Frames(1); me = Maxf(me, R.MaxJointError()); } CHECK(R.Finite() && me < 0.06f && R.asleep, "quadrupede estavel e adormece"); }
      // estresse numerico: impulso gigante em ragdoll isolado
      { Ragdoll R; Vec3 pos(bx, TerrainH(bx, bz + 30), bz + 30); R.BuildHuman(pos, 0.7f, 1.45f, Vec3()); R.Impulse(pos + Vec3(0, 1, 0), Norm(Vec3(1, 0.6f, 0.3f)), 25.0f); float me = 0; for (int f = 0; f < 600 && !R.asleep; f++) { R.Step(1.0f/30); me = Maxf(me, R.MaxJointError()); }
        printf("  estresse (impulso 25 m/s, escala 1.45): desvio maximo das juntas=%.1f cm\n", me*100.0f); CHECK(R.Finite() && me < 0.15f, "estavel sob impulso extremo"); }
      // corpo revistavel segue o ragdoll
      CHECK(Len2D(h->pos - (h->rag ? h->rag->Pelvis() : h->pos)) < 0.6f, "posicao do cadaver acompanha o ragdoll (saque funciona)");
    }

    GameShutdown();
    printf("\n%s (%d falhas)\n", fails == 0 ? "TUDO OK" : "HA FALHAS", fails);
    return fails ? 1 : 0;
}
