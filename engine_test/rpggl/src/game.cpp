// =====================================================================
//  game.cpp - regras do RPG: itens, jogador, NPCs (IA em paralelo),
//  combate, camera, interacao, quests e laco de simulacao.
//
//  Threads:
//    * principal ........ entrada, jogador, aplicacao dos resultados da IA
//    * pool (N workers) . IA dos NPCs em paralelo (ParallelFor) e geracao
//                         de chunks / gravacao de saves em segundo plano
//    * audio ............ mixer (audio.cpp)
//  A IA so le o mundo (colisores/terreno) e escreve no PROPRIO NPC;
//  tudo que afeta o resto do jogo e aplicado depois do join.
// =====================================================================
#include "game.h"

Game G;
static Rng g_rng(0xC0FFEEu);

const char* ATTR_NAMES[AT_COUNT] = { "Vigor", "Pontaria", "Fôlego", "Constituição", "Lábia", "Sobrevivência" };
const char* QUEST_NAMES[Q_COUNT] = { "Saindo do Silo", "Dentes de Presa", "Sinal Fantasma", "Rodas para o Deserto", "Brilho Perigoso", "Terra Prometida" };

// ---------------------------------------------------------------------
//  TABELAS
// ---------------------------------------------------------------------
static const ItemDef ITEMS[IT_COUNT] = {
    // nome, tipo, peso, valor, dano, carregador, municao, cadencia, recarga, alcance, dispersao, projeteis, cura, rads, dr, descricao
    { "Dinheiro", ITY_MISC, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "" },
    { "Cano de Chumbo", ITY_MELEE, 3.0f, 15, 18, 0, 0, 0.55f, 0, 2.4f, 0, 0, 0, 0, 0, "Um cano enferrujado. Rústico, mas honesto." },
    { "Pistola 10mm", ITY_GUN, 2.0f, 60, 20, 12, IT_AMMO_10, 0.26f, 1.4f, 90, 0.012f, 1, 0, 0, 0, "Semiautomática confiável. Aguenta o tranco do deserto." },
    { "Espingarda Cano Duplo", ITY_GUN, 5.5f, 110, 9, 2, IT_AMMO_SHELL, 0.8f, 2.2f, 38, 0.075f, 7, 0, 0, 0, "Devastadora de perto. Dois tiros e acabou." },
    { "Rifle de Caça", ITY_GUN, 4.5f, 150, 58, 5, IT_AMMO_RIFLE, 1.0f, 2.5f, 170, 0.004f, 1, 0, 0, 0, "Precisão de longe. Ideal para mirar com calma." },
    { "Munição 10mm", ITY_AMMO, 0.03f, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Munição para a pistola." },
    { "Cartucho Calibre 12", ITY_AMMO, 0.1f, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Munição para a espingarda." },
    { "Munição de Rifle", ITY_AMMO, 0.06f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Munição para o rifle." },
    { "Injetor Médico", ITY_AID, 0.2f, 40, 0, 0, 0, 0, 0, 0, 0, 0, 60, 0, 0, "Restaura 60 de vida na hora." },
    { "Garrafa d'Água", ITY_AID, 0.5f, 10, 0, 0, 0, 0, 0, 0, 0, 0, 12, 4, 0, "Restaura 12 de vida. Um pouco radioativa." },
    { "Comida Enlatada", ITY_AID, 0.6f, 12, 0, 0, 0, 0, 0, 0, 0, 0, 25, 0, 0, "Restaura 25 de vida. Ninguém pergunta a validade." },
    { "Soro Antirradiação", ITY_AID, 0.3f, 55, 0, 0, 0, 0, 0, 0, 0, 0, 0, -250, 0, "Remove radiação do organismo." },
    { "Sucata", ITY_JUNK, 1.0f, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Peças velhas. Alguém dá valor." },
    { "Jaqueta de Couro Reforçada", ITY_ARMOR, 6, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, "Resistência a dano +3." },
    { "Armadura de Sucata", ITY_ARMOR, 14, 160, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, "Placas soldadas. Resistência a dano +6." },
    { "Fita Gravada do Sinal", ITY_MISC, 0.1f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Gravação do sinal da Torre de Rádio." },
    { "Galão de Combustível", ITY_MISC, 5.0f, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Reabastece um veículo (+50). Use perto dele ou ao volante." },
    { "Estilhaço Rubro", ITY_ARTIFACT, 0.5f, 180, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Estilhaço quente e pulsante. Equipado: +30 de vida máxima." },
    { "Estilhaço Faísca", ITY_ARTIFACT, 0.5f, 220, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Estilhaço que estala. Equipado: estamina recupera bem mais rápido." },
    { "Estilhaço Âmbar", ITY_ARTIFACT, 0.5f, 260, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Estilhaço que zumbe. Equipado: -40% de absorção de radiação." },
    { "Ração do Silo", ITY_AID, 0.4f, 16, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, "Barra densa que sustenta um adulto por horas. Mata a fome." },
    { "Carne Crua", ITY_AID, 0.6f, 6, 0, 0, 0, 0, 0, 0, 0, 0, 6, 10, 0, "Carne de bicho do deserto. Mata a fome, mas é radioativa." },
    { "Semente Viva", ITY_MISC, 0.05f, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Uma semente intacta do mundo antigo. Esperança precisa de três." },
};
const ItemDef& ItemInfo(int id) { return ITEMS[Clampi(id, 0, IT_COUNT - 1)]; }

static const NpcStat NPCS[NK_COUNT] = {
    // nome, vida, andar, correr, deteccao, dano, alcance, altura, raio, hostil, xp
    { "Colono", 40, 1.3f, 4.2f, 0, 0, 0, 1.8f, 0.45f, false, 0 },
    { "Mercador", 60, 1.2f, 3.5f, 0, 0, 0, 1.8f, 0.45f, false, 0 },
    { "Saqueador", 62, 1.6f, 4.8f, 32, 9, 40, 1.8f, 0.45f, true, 25 },
    { "Cão Selvagem", 34, 2.0f, 7.5f, 24, 7, 1.5f, 0.85f, 0.45f, true, 15 },
    { "Ressecado", 85, 1.0f, 3.1f, 18, 12, 1.7f, 1.8f, 0.45f, true, 30 },
    { "Guarda", 95, 1.3f, 4.5f, 0, 0, 0, 1.8f, 0.45f, false, 0 },
    { "Ancião Silas", 50, 1.0f, 3.0f, 0, 0, 0, 1.8f, 0.45f, false, 0 },
    { "Rastejante", 28, 2.2f, 8.5f, 22, 6, 1.4f, 0.6f, 0.4f, true, 14 },
    { "Brutamonte", 240, 1.0f, 3.6f, 20, 28, 2.2f, 1.9f, 0.5f, true, 90 },
    { "Mecânico Tavo", 70, 1.1f, 3.5f, 0, 0, 0, 1.8f, 0.45f, false, 0 },
};
const NpcStat& NpcInfo(int kind) { return NPCS[Clampi(kind, 0, NK_COUNT - 1)]; }

Player::Player() : yaw(0), pitch(0), hp(100), maxhp(100), stam(100), rad(0), level(1), xp(0), points(0), money(0),
    weapon(IT_PIPE), armor(IT_NONE), artifact(IT_NONE), onGround(true), sneak(false), sprinting(false), dead(false),
    fireCd(0), reloadT(0), swingT(0), hurtT(0), recoil(0), aimAmt(0), animPhase(0), speed(0), bob(0), hitMarker(0), flash(0), lastHurtYaw(0), bodyYaw(0), stamDelay(0), reloadW(0) {
    const int sp[AT_COUNT] = { 5, 6, 5, 5, 4, 4 };
    for (int i = 0; i < AT_COUNT; i++) attr[i] = sp[i];
    memset(mag, 0, sizeof(mag)); strcpy(name, "Andarilho"); hunger = thirst = 100.0f;
}

Game::Game() : state(GS_MENU), ui(UI_MAIN), prevUi(UI_NONE), gameHours(8.5f), playSeconds(0), simTime(0), karma(0), kills(0),
    menuSel(0), tab(0), scroll(0), uiT(0), listSel(0), confirmDel(false), shopCol(0), shopNpc(0), locT(0),
    saveReq(0), saveReqKind(0), saveReqSlot(0), quickIdx(0), autoIdx(0), saveCounter(0), saveBusy(0), saveOk(0), saveFail(0), autoT(0), hasSaves(false), loadT(0),
    fps(60), shotT(-100), npcActive(0), menuCamT(0), quit(false) {
    drive = 0; camIdle = 0; vehHit = 0; introPending = false; ragActive = 0; memset(quests, 0, sizeof(quests)); shopSel[0] = shopSel[1] = 0; tgt.type = 0; tgt.id = 0; tgt.dist = 0; tgt.prompt[0] = 0;
}

// ---------------------------------------------------------------------
//  UTILIDADES
// ---------------------------------------------------------------------
void Notify(const std::string& s, int kind) {
    if (!G.msgs.empty() && G.msgs.back().text == s) { G.msgs.back().t = 6.0f; return; }      // evita repetir a mesma linha
    Msg m; m.text = s; m.t = 6.0f; m.kind = kind; G.msgs.push_back(m);
    if (G.msgs.size() > 7) G.msgs.erase(G.msgs.begin());
}
float TimeOfDay() { return fmodf(G.gameHours, 24.0f); }
float NightFactor() {
    float h = TimeOfDay();
    if (h >= 6.5f && h <= 19.0f) return 0.0f;
    if (h > 19.0f && h < 21.0f) return (h - 19.0f)/2.0f;
    if (h >= 21.0f || h <= 4.5f) return 1.0f;
    return 1.0f - (h - 4.5f)/2.0f;
}
float ArtifactBonus(int kind) {
    switch (G.p.artifact) {
    case IT_ART_HEART: return kind == 0 ? 30.0f : 0.0f;
    case IT_ART_SPARK: return kind == 1 ? 0.6f : 0.0f;
    case IT_ART_CRYSTAL: return kind == 2 ? 0.4f : 0.0f;
    }
    return 0.0f;
}
float EffMaxHp() { return (G.p.maxhp + ArtifactBonus(0))*(1.0f - Sat(G.p.rad/1000.0f)*0.5f); }
float MaxCarry() { return 120.0f + 10.0f*(float)G.p.attr[AT_VIG]; }
float CarryWeight() { float w = 0; for (size_t i = 0; i < G.p.inv.size(); i++) w += ItemInfo(G.p.inv[i].id).weight*(float)G.p.inv[i].count; return w; }

int AddItem(int id, int count) {
    if (id == IT_NONE) { G.p.money += count; return count; }
    for (size_t i = 0; i < G.p.inv.size(); i++) if (G.p.inv[i].id == id) { G.p.inv[i].count += count; return count; }
    Slot s; s.id = id; s.count = count; G.p.inv.push_back(s); return count;
}
int CountItem(int id) { for (size_t i = 0; i < G.p.inv.size(); i++) if (G.p.inv[i].id == id) return G.p.inv[i].count; return 0; }
bool RemoveItem(int id, int count) {
    for (size_t i = 0; i < G.p.inv.size(); i++) if (G.p.inv[i].id == id) {
        if (G.p.inv[i].count < count) return false;
        G.p.inv[i].count -= count; if (G.p.inv[i].count <= 0) G.p.inv.erase(G.p.inv.begin() + (long)i);
        return true;
    }
    return false;
}

void GainXp(int xp) {
    Player& p = G.p; p.xp += xp;
    for (;;) {
        int need = 80 + 45*p.level; if (p.xp < need || p.level >= 30) break;
        p.xp -= need; p.level++; p.points++; p.maxhp += 5.0f + (float)p.attr[AT_CON]; p.hp = EffMaxHp();
        Notify("SUBIU DE NÍVEL! Nível " + std::to_string(p.level) + " (1 ponto de atributo)", 2); PlaySfx(SND_LEVEL, 0.7f);
    }
}

// ---------------------------------------------------------------------
//  QUESTS
// ---------------------------------------------------------------------
const char* QuestObjective(int q) {
    const QuestState& s = G.quests[q];
    if (s.state == 0) return "";
    if (s.state == 2) return "Concluída.";
    static char buf[128];
    switch (q) {
    case Q_ARRIVE: return "Chegue à vila Esperança (a leste do Silo) e fale com o Ancião Silas.";
    case Q_RAIDERS:
        if (s.stage == 0) { snprintf(buf, sizeof(buf), "Elimine saqueadores (%d/5). O Acampamento Presa-Negra fica a nordeste.", s.count); return buf; }
        return "Volte para o Ancião Silas, em Esperança.";
    case Q_RADIO:
        if (s.stage == 0) return "Investigue a Torre de Rádio Alfa (sudoeste) e use o terminal.";
        return "Volte para o Ancião Silas, em Esperança.";
    case Q_SEEDS:
        if (CountItem(IT_SEED) >= 3) return "Você reuniu as sementes. Leve ao Ancião Silas, em Esperança.";
        snprintf(buf, sizeof(buf), "Reúna 3 Sementes Vivas (%d/3) e leve ao Ancião Silas. Silos, cofres e saqueadores guardam algumas.", CountItem(IT_SEED)); return buf;
    case Q_WHEELS:
        if (CountItem(IT_FUEL) >= 2) return "Você tem o combustível. Leve ao Mecânico Tavo, no Posto Ferrugem (noroeste).";
        snprintf(buf, sizeof(buf), "Traga 2 galões de combustível ao Mecânico Tavo, no Posto Ferrugem (%d/2). Saqueadores e postos costumam ter.", CountItem(IT_FUEL)); return buf;
    case Q_ARTIFACT:
        if (CountItem(IT_ART_HEART) + CountItem(IT_ART_SPARK) + CountItem(IT_ART_CRYSTAL) > 0) return "Leve o estilhaço ao Mecânico Tavo, no Posto Ferrugem.";
        return "Colete um estilhaço no Campo de Fissuras (sudoeste do Silo) e leve ao Tavo. Cuidado: estalos indicam fissuras.";
    }
    return "";
}
const char* QuestDesc(int q) {
    switch (q) {
    case Q_ARRIVE: return "Você deixou o Silo Ômega sem saber o que esperar. A fumaça de uma vila aparece ao leste.";
    case Q_RAIDERS: return "Saqueadores do Presa-Negra aterrorizam os viajantes e já levaram mantimentos da vila.";
    case Q_RADIO: return "A velha torre de rádio voltou a transmitir um sinal repetitivo. Ninguém ousa chegar perto.";
    case Q_SEEDS: return "O Silo Ômega guardava a vida do mundo antigo em sementes. Esperança precisa de três para plantar a primeira lavoura em oitenta anos.";
    case Q_WHEELS: return "Tavo, o mecânico do Posto Ferrugem, tem um buggy quase pronto. Falta combustível para a partida.";
    case Q_ARTIFACT: return "Tavo paga bem por estilhaços. Eles nascem dentro de fissuras: pontos invisíveis que estalam, queimam e eletrocutam.";
    }
    return "";
}
bool QuestMarker(int q, Vec3& out) {
    const QuestState& s = G.quests[q]; if (s.state != 1) return false;
    int cx = HOPE_CX, cz = HOPE_CZ;
    if (q == Q_RAIDERS && s.stage == 0) { cx = RAIDER_CX; cz = RAIDER_CZ; }
    if (q == Q_RADIO && s.stage == 0) { cx = RADIO_CX; cz = RADIO_CZ; }
    if (q == Q_SEEDS) { if (CountItem(IT_SEED) < 3) return false; }
    if (q == Q_WHEELS) { if (CountItem(IT_FUEL) < 2) return false; cx = GASFIX_CX; cz = GASFIX_CZ; }
    if (q == Q_ARTIFACT) { if (CountItem(IT_ART_HEART) + CountItem(IT_ART_SPARK) + CountItem(IT_ART_CRYSTAL) > 0) { cx = GASFIX_CX; cz = GASFIX_CZ; } else { cx = ANOM_CX; cz = ANOM_CZ; } }
    out = ChunkCenter(cx, cz); return true;
}
void QuestAdvance(int ev, int) {
    QuestState* q = G.quests;
    if (ev == QE_TALK_ELDER) {
        if (q[Q_ARRIVE].state == 1) {
            q[Q_ARRIVE].state = 2; GainXp(60); AddItem(IT_NONE, 40); AddItem(IT_AMMO_10, 12);
            q[Q_RAIDERS].state = 1; q[Q_RAIDERS].stage = 0; q[Q_RAIDERS].count = 0; q[Q_RADIO].state = 1; q[Q_RADIO].stage = 0; q[Q_SEEDS].state = 1;
            Notify("Missão concluída: Saindo do Silo", 2); Notify("Nova missão: Dentes de Presa", 2); Notify("Nova missão: Sinal Fantasma", 2); Notify("Nova missão: Terra Prometida", 2); PlaySfx(SND_QUEST, 0.7f);
        } else {
            if (q[Q_RAIDERS].state == 1 && q[Q_RAIDERS].stage == 1) {
                q[Q_RAIDERS].state = 2; GainXp(140); AddItem(IT_NONE, 90); if (CountItem(IT_RIFLE) == 0) AddItem(IT_RIFLE, 1); AddItem(IT_AMMO_RIFLE, 15);
                Notify("Missão concluída: Dentes de Presa", 2); PlaySfx(SND_QUEST, 0.7f);
            }
            if (q[Q_RADIO].state == 1 && q[Q_RADIO].stage == 1) {
                q[Q_RADIO].state = 2; GainXp(160); AddItem(IT_NONE, 120); if (CountItem(IT_ARMOR_SCRAP) == 0) AddItem(IT_ARMOR_SCRAP, 1);
                Notify("Missão concluída: Sinal Fantasma", 2); PlaySfx(SND_QUEST, 0.7f);
            }
        }
        if (q[Q_SEEDS].state == 1 && CountItem(IT_SEED) >= 3) {                                   // vale tambem na primeira conversa
            RemoveItem(IT_SEED, 3); q[Q_SEEDS].state = 2; GainXp(220); AddItem(IT_NONE, 100); AddItem(IT_RATION, 4);
            Notify("Missão concluída: Terra Prometida. Esperança plantou o primeiro verde em 80 anos.", 2); PlaySfx(SND_QUEST, 0.8f);
        }
    } else if (ev == QE_KILL_RAIDER) {
        if (q[Q_RAIDERS].state == 1 && q[Q_RAIDERS].stage == 0) {
            q[Q_RAIDERS].count++;
            if (q[Q_RAIDERS].count >= 5) { q[Q_RAIDERS].stage = 1; Notify("Saqueadores eliminados. Volte para o Ancião Silas.", 2); PlaySfx(SND_QUEST, 0.5f); }
            else Notify("Saqueadores: " + std::to_string(q[Q_RAIDERS].count) + "/5", 1);
        }
    } else if (ev == QE_TALK_MECHANIC) {
        if (q[Q_WHEELS].state == 0) {
            q[Q_WHEELS].state = 1; q[Q_ARTIFACT].state = 1; Notify("Nova missão: Rodas para o Deserto", 2); Notify("Nova missão: Brilho Perigoso", 2); PlaySfx(SND_QUEST, 0.6f);
        } else {
            if (q[Q_WHEELS].state == 1 && CountItem(IT_FUEL) >= 2) {
                RemoveItem(IT_FUEL, 2); q[Q_WHEELS].state = 2; GainXp(130); AddItem(IT_NONE, 60);
                Notify("Missão concluída: Rodas para o Deserto. O buggy é seu!", 2); PlaySfx(SND_QUEST, 0.7f); SpawnChunkVehicles(const_cast<Chunk*>(W.Get(GASFIX_CX, GASFIX_CZ)));
            }
            int art = CountItem(IT_ART_HEART) ? IT_ART_HEART : (CountItem(IT_ART_SPARK) ? IT_ART_SPARK : (CountItem(IT_ART_CRYSTAL) ? IT_ART_CRYSTAL : 0));
            if (q[Q_ARTIFACT].state == 1 && art) {
                RemoveItem(art, 1); if (G.p.artifact == art && CountItem(art) == 0) G.p.artifact = IT_NONE;
                q[Q_ARTIFACT].state = 2; GainXp(160); AddItem(IT_NONE, ItemInfo(art).value); AddItem(IT_MEDKIT, 2);
                Notify("Missão concluída: Brilho Perigoso", 2); PlaySfx(SND_QUEST, 0.7f);
            }
        }
    } else if (ev == QE_TERMINAL) {
        if (q[Q_RADIO].state == 1 && q[Q_RADIO].stage == 0) {
            q[Q_RADIO].stage = 1; AddItem(IT_TAPE, 1); Notify("Terminal: o sinal é um pedido de socorro automático, repetido há 80 anos.", 2); Notify("Você copiou a gravação. Volte para o Ancião Silas.", 2); PlaySfx(SND_QUEST, 0.6f);
        } else Notify("O terminal repete um sinal de socorro antigo.", 0);
    }
}

// ---------------------------------------------------------------------
//  LOOT
// ---------------------------------------------------------------------
static void GenLoot(u64 id, int tier, int kind, std::vector<Slot>& out) {
    Rng r((u32)(id ^ (id >> 32)) ^ 0xBEEFu);
    if (kind == CK_TERMINAL) return;
    #define ADD(it, n) do { Slot s_; s_.id = (it); s_.count = (n); out.push_back(s_); } while (0)
    if (kind == CK_SAFE) tier = Maxi(tier, 2);
    switch (tier) {
    case 1:
        if (r.chance(0.7f)) ADD(IT_JUNK, 1 + r.i(3)); if (r.chance(0.5f)) ADD(IT_CANNED, 1); if (r.chance(0.4f)) ADD(IT_WATER, 1);
        if (r.chance(0.5f)) ADD(IT_AMMO_10, 3 + r.i(8)); if (r.chance(0.15f)) ADD(IT_MEDKIT, 1); if (r.chance(0.12f)) ADD(IT_RATION, 1); if (r.chance(0.04f)) ADD(IT_SEED, 1); ADD(IT_NONE, 2 + r.i(18)); break;
    case 2:
        ADD(IT_AMMO_10, 6 + r.i(9)); if (r.chance(0.4f)) ADD(IT_AMMO_SHELL, 2 + r.i(5)); if (r.chance(0.3f)) ADD(IT_AMMO_RIFLE, 3 + r.i(6));
        if (r.chance(0.45f)) ADD(IT_MEDKIT, 1); if (r.chance(0.5f)) ADD(IT_CANNED, 1 + r.i(2)); if (r.chance(0.5f)) ADD(IT_JUNK, 1 + r.i(3));
        if (r.chance(0.1f)) ADD(IT_ANTIRAD, 1); if (r.chance(0.28f)) ADD(IT_FUEL, 1); if (r.chance(0.2f)) ADD(IT_RATION, 1); if (r.chance(0.12f)) ADD(IT_SEED, 1); ADD(IT_NONE, 8 + r.i(40)); break;
    case 3:
        ADD(IT_AMMO_10, 8 + r.i(12)); ADD(IT_AMMO_SHELL, 2 + r.i(6)); ADD(IT_AMMO_RIFLE, 4 + r.i(8)); ADD(IT_MEDKIT, 1 + r.i(2));
        if (r.chance(0.5f)) ADD(IT_ANTIRAD, 1); if (r.chance(0.65f)) ADD(IT_FUEL, 1 + r.i(2)); if (r.chance(0.5f)) ADD(IT_SEED, 1); if (r.chance(0.4f)) ADD(IT_RATION, 2); ADD(IT_NONE, 30 + r.i(80));
        if (r.chance(0.3f)) { int w = r.i(4); ADD(w == 0 ? IT_PISTOL : (w == 1 ? IT_SHOTGUN : (w == 2 ? IT_RIFLE : IT_ARMOR_LEATHER)), 1); }
        break;
    case 4: ADD(IT_ART_HEART + r.i(3), 1); break;                                     // estilhaço de fissura
    case 5: ADD(IT_AMMO_10, 12); ADD(IT_FUEL, 1); ADD(IT_MEDKIT, 1); ADD(IT_RATION, 2); ADD(IT_SEED, 1); ADD(IT_NONE, 20); break;   // kit inicial
    default: break;
    }
    if (kind == CK_BODY) ADD(IT_NONE, 3 + r.i(15));
    #undef ADD
}

static bool IsLooted(u64 id) {
    std::unordered_map<u64, float>::iterator it = G.looted.find(id); if (it == G.looted.end()) return false;
    if (G.gameHours - it->second >= 96.0f) { G.looted.erase(it); return false; }
    return true;
}
static void TakeLoot(u64 id, int tier, int kind) {
    std::vector<Slot> loot; GenLoot(id, tier, kind, loot);
    G.looted[id] = G.gameHours;
    if (loot.empty()) { Notify("Vazio.", 0); return; }
    for (size_t i = 0; i < loot.size(); i++) {
        Slot s = loot[i];
        if ((s.id == IT_PISTOL || s.id == IT_SHOTGUN || s.id == IT_RIFLE || s.id == IT_ARMOR_LEATHER) && CountItem(s.id) > 0) { s.id = IT_NONE; s.count = ItemInfo(loot[i].id).value/3; }
        AddItem(s.id, s.count);
        if (s.id == IT_NONE) Notify("+" + std::to_string(s.count) + " Fichas", 1); else Notify("+" + std::to_string(s.count) + " " + ItemInfo(s.id).name, 1);
    }
    PlaySfx(SND_PICKUP, 0.6f);
}

// ---------------------------------------------------------------------
//  NPCs: spawn / despawn
// ---------------------------------------------------------------------
void SpawnChunkNpcs(Chunk* c) {
    float cdist = sqrtf(Sqr((float)c->cx - 128.0f) + Sqr((float)c->cz - 128.0f));
    int lvl = Clampi(1 + (int)(cdist/9.0f), 1, 12);
    for (size_t i = 0; i < c->spawns.size(); i++) {
        const SpawnPt& s = c->spawns[i];
        std::unordered_map<u64, float>::iterator k = G.killed.find(s.id);
        if (k != G.killed.end()) { if (G.gameHours - k->second < 72.0f) continue; G.killed.erase(k); }
        const NpcStat& st = NpcInfo(s.kind);
        Npc* n = new Npc(); n->id = s.id; n->kind = s.kind; n->cx = c->cx; n->cz = c->cz;
        n->level = lvl; n->dmgMul = 1.0f + 0.06f*(float)(lvl - 1); n->maxhp = st.hp*(1.0f + 0.08f*(float)(lvl - 1)); n->hp = n->maxhp;
        n->pos = Vec3(s.x, TerrainH(s.x, s.z), s.z); n->home = n->pos; n->yaw = s.yaw; n->hostile = st.hostile; n->rng = Rng((u32)(s.id ^ (s.id >> 32)) | 1u);
        n->scale = (s.kind == NK_DOG || s.kind == NK_CRAWLER) ? 1.0f : (s.kind == NK_BRUTE ? 1.45f : (0.94f + 0.12f*n->rng.f())); n->state = NS_IDLE; n->t = n->rng.r(0, 4);
        std::unordered_map<u64, NpcOvr>::iterator o = G.npcOvr.find(s.id);
        if (o != G.npcOvr.end()) { n->pos.x = o->second.x; n->pos.z = o->second.z; n->pos.y = TerrainH(n->pos.x, n->pos.z); n->hp = o->second.hp; n->state = o->second.state; G.npcOvr.erase(o); }
        G.npcs.push_back(n);
    }
}
void DespawnChunkNpcs(Chunk* c) {
    for (size_t i = 0; i < G.npcs.size();) {
        Npc* n = G.npcs[i];
        if (n->cx == c->cx && n->cz == c->cz) {
            if (n->alive && (n->hp < n->maxhp - 0.5f || n->state == NS_COMBAT)) {
                NpcOvr o; o.x = n->pos.x; o.z = n->pos.z; o.hp = n->hp; o.state = (u8)(n->state == NS_COMBAT ? NS_COMBAT : NS_IDLE); G.npcOvr[n->id] = o;
            }
            delete n; G.npcs.erase(G.npcs.begin() + (long)i);
        } else i++;
    }
}
static void ClearNpcs() { for (size_t i = 0; i < G.npcs.size(); i++) delete G.npcs[i]; G.npcs.clear(); }

// ---------------------------------------------------------------------
//  IA DOS NPCs  (roda em WORKERS: so escreve no proprio NPC)
// ---------------------------------------------------------------------
struct NpcCtx { Vec3 pp; bool pAlive, pSneak, pSprint; float now, shotT, night; Vec3 shotPos; };

static void NpcMove(Npc& n, Vec3 dir, float speed, float dt, bool faceMove) {
    float l = Len2D(dir);
    if (l < 1e-4f || speed <= 0.01f) { n.speed = Approach(n.speed, 0, 9, dt); return; }
    dir = Vec3(dir.x/l, 0, dir.z/l);
    n.speed = Approach(n.speed, speed, 6, dt);
    float step = n.speed*dt, nx = n.pos.x + dir.x*step, nz = n.pos.z + dir.z*step, gy;
    W.Collide(nx, nz, NpcInfo(n.kind).radius*0.9f, n.pos.y, gy);
    nx = Clampf(nx, WORLD_EDGE, WORLD_SIZE - WORLD_EDGE); nz = Clampf(nz, WORLD_EDGE, WORLD_SIZE - WORLD_EDGE);
    float ty = TerrainH(nx, nz), dist = Maxf(0.001f, sqrtf(Sqr(nx - n.pos.x) + Sqr(nz - n.pos.z)));
    if ((ty - n.pos.y)/dist > 1.3f && dist > 0.001f) { n.speed *= 0.3f; return; }            // ladeira ingreme demais
    n.pos.x = nx; n.pos.z = nz; n.pos.y = ty;
    n.animPhase += n.speed*dt*4.2f;
    if (faceMove) n.yaw = TurnToward(n.yaw, YawOf(dir.x, dir.z), 7.0f*dt);
}

static void NpcThink(Npc& n, float dt, const NpcCtx& c) {
    n.out = 0; n.hurtT = Maxf(0, n.hurtT - dt);
    if (!n.alive) { n.deadT += dt; return; }
    const NpcStat& st = NpcInfo(n.kind);
    Vec3 to = c.pp - n.pos; float d = Len2D(to); n.cd -= dt; n.t += dt;
    bool stationary = (n.kind == NK_MERCHANT || n.kind == NK_ELDER || n.kind == NK_MECHANIC);
    bool sees = false, heard = false;
    if (st.hostile && c.pAlive) {
        float range = st.detect*(c.pSneak ? 0.55f : 1.0f)*(1.0f - 0.25f*c.night)*(c.pSprint ? 1.3f : 1.0f);
        if (d < range) {
            Vec3 f = Facing(n.yaw); float ca = d > 0.01f ? (f.x*to.x + f.z*to.z)/d : 1.0f;
            if (ca > -0.35f || d < 7.0f || n.state == NS_COMBAT) {
                Vec3 eye(n.pos.x, n.pos.y + 1.4f*n.scale, n.pos.z), tg(c.pp.x, c.pp.y + 1.2f, c.pp.z); Vec3 dv = tg - eye; float dist = Len(dv);
                if (dist > 0.1f) { dv = dv*(1.0f/dist); if (W.RayCast(eye, dv, dist) >= dist - 0.6f) sees = true; }
            }
        }
        if (c.now - c.shotT < 1.0f && Len2D(c.shotPos - n.pos) < 42.0f) heard = true;
    }
    switch (n.state) {
    case NS_IDLE: case NS_WANDER: case NS_ALERT: {
        if (st.hostile) {
            if (sees || heard) n.alert += dt*(d < 12.0f || heard ? 3.5f : 1.7f); else n.alert = Maxf(0, n.alert - dt*0.5f);
            if (n.alert >= 1.0f) { n.state = NS_COMBAT; n.lostT = 0; n.t = 0; n.alert = 1; n.out = 3; break; }       // out=3: gritou (som)
        } else if (n.kind == NK_SETTLER && c.now - c.shotT < 2.5f && Len2D(c.shotPos - n.pos) < 28.0f) { n.state = NS_FLEE; n.t = 0; break; }
        if (stationary || n.kind == NK_GUARD) {
            if (d < 6.0f) n.yaw = TurnToward(n.yaw, YawOf(to.x, to.z), 3.0f*dt);
            if (n.kind == NK_GUARD && n.state == NS_WANDER) { /* patrulha curta */ }
        }
        if (n.state == NS_IDLE) {
            n.speed = Approach(n.speed, 0, 9, dt);
            if (n.t > n.rng.r(3.0f, 8.0f) && !stationary) {
                float a = n.rng.r(0, 2*PI), r = n.rng.r(2.0f, 9.0f);
                n.strafe = n.home.x + cosf(a)*r; n.lostT = n.home.z + sinf(a)*r;           // reaproveita campos como alvo (x,z)
                n.state = NS_WANDER; n.t = 0;
            }
        } else {
            Vec3 tg(n.strafe, 0, n.lostT), dv = tg - n.pos; float dd = Len2D(dv);
            if (dd < 0.8f || n.t > 9.0f) { n.state = NS_IDLE; n.t = 0; n.lostT = 0; n.strafe = 1; }
            else NpcMove(n, dv, st.walk, dt, true);
        }
        break;
    }
    case NS_COMBAT: {
        if (!c.pAlive) { n.state = NS_IDLE; n.t = 0; n.alert = 0; break; }
        if (sees) n.lostT = 0; else n.lostT += dt;
        if (n.lostT > 8.0f || d > st.detect*3.0f) { n.state = NS_IDLE; n.t = 0; n.alert = 0; n.lostT = 0; n.strafe = 1; break; }
        n.yaw = TurnToward(n.yaw, YawOf(to.x, to.z), 7.0f*dt);
        if (n.kind == NK_RAIDER) {
            if (n.hp < n.maxhp*0.2f && n.rng.chance(dt*0.6f)) { n.state = NS_FLEE; n.t = 0; break; }
            Vec3 dir(to.x/Maxf(d, 0.01f), 0, to.z/Maxf(d, 0.01f));
            if (d > 17.0f || !sees) NpcMove(n, dir, st.run, dt, false);
            else if (d < 7.0f) NpcMove(n, dir*-1.0f, st.walk*1.3f, dt, false);
            else { if (n.t > n.rng.r(1.2f, 3.0f)) { n.t = 0; n.strafe = (n.strafe > 0) ? -1.0f : 1.0f; } NpcMove(n, Vec3(-dir.z*n.strafe, 0, dir.x*n.strafe), st.walk, dt, false); }
            if (sees && d < 45.0f && n.cd <= 0.0f) {
                float p = 0.78f - d*0.012f - (c.pSprint ? 0.12f : 0) - (c.pSneak ? 0.06f : 0); p = Clampf(p, 0.1f, 0.82f);
                Vec3 f = Facing(n.yaw);
                n.fireFrom = Vec3(n.pos.x + f.x*0.6f, n.pos.y + 1.4f, n.pos.z + f.z*0.6f);
                n.fireHit = n.rng.f() < p; n.fireTo = Vec3(c.pp.x, c.pp.y + 1.1f, c.pp.z);
                if (!n.fireHit) { n.fireTo.x += n.rng.r(-1.4f, 1.4f); n.fireTo.y += n.rng.r(-0.8f, 0.6f); n.fireTo.z += n.rng.r(-1.4f, 1.4f); }
                n.fireDmg = st.dmg*n.dmgMul*n.rng.r(0.8f, 1.25f); n.out = 1; n.cd = n.rng.r(0.9f, 1.9f);
            }
        } else {
            Vec3 dir(to.x/Maxf(d, 0.01f), 0, to.z/Maxf(d, 0.01f));
            if (d > st.reach*0.8f) NpcMove(n, dir, st.run, dt, false); else n.speed = Approach(n.speed, 0, 12, dt);
            if (d < st.reach + 0.5f && fabsf(c.pp.y - n.pos.y) < 1.6f && n.cd <= 0.0f) { n.out = 2; n.fireDmg = st.dmg*n.dmgMul*n.rng.r(0.85f, 1.2f); n.cd = (n.kind == NK_DOG || n.kind == NK_CRAWLER) ? 0.95f : (n.kind == NK_BRUTE ? 1.9f : 1.4f); n.t = 0; }
        }
        break;
    }
    case NS_FLEE: {
        Vec3 away(n.pos.x - c.pp.x, 0, n.pos.z - c.pp.z); NpcMove(n, away, st.run, dt, true);
        if (n.t > 5.0f) { n.state = NS_IDLE; n.t = 0; }
        break;
    }
    default: break;
    }
}

// ---------------------------------------------------------------------
//  JOGADOR: dano, mortes, combate
// ---------------------------------------------------------------------
static void HurtPlayerEx(float dmg, const Vec3& from, bool armor) {
    Player& p = G.p; if (p.dead) return;
    if (armor) dmg *= 1.0f - Minf(0.7f, (float)ItemInfo(p.armor).dr*0.06f);
    p.hp -= dmg; p.hurtT = 0.45f; p.lastHurtYaw = YawOf(from.x - p.pos.x, from.z - p.pos.z);
    PlaySfx(SND_HURT, 0.5f);
    if (p.hp <= 0) { p.hp = 0; p.dead = true; G.drive = 0; SetEngine(false, 0); PlaySfx(SND_DEATH, 0.8f); UiOpen(UI_DEAD); }
}
static void HurtPlayer(float dmg, const Vec3& from) { HurtPlayerEx(dmg, from, true); }

struct HitInfo { Vec3 pt, dir; float force, t; bool valid; };
static HitInfo g_hit = { Vec3(), Vec3(), 0, -10.0f, false };
static void SetHit(const Vec3& pt, const Vec3& dir, float force) { g_hit.pt = pt; g_hit.dir = dir; g_hit.force = force; g_hit.t = G.simTime; g_hit.valid = true; }

static void StartRagdoll(Npc& n) {
    int live = 0; for (size_t i = 0; i < G.npcs.size(); i++) if (G.npcs[i]->rag) live++;
    if (live >= 48 || n.rag) return;                                              // limite de memoria/CPU; alem disso cai no tombo antigo
    bool quad = (n.kind == NK_DOG || n.kind == NK_CRAWLER); float sc = n.scale*(n.kind == NK_CRAWLER ? 0.8f : 1.0f);
    n.rag = new Ragdoll(); Vec3 vel = Facing(n.yaw)*n.speed;
    if (quad) n.rag->BuildQuad(n.pos, n.yaw, sc, vel); else n.rag->BuildHuman(n.pos, n.yaw, sc, vel);
    Vec3 dir, at; float force;
    if (g_hit.valid && G.simTime - g_hit.t < 0.25f) { dir = g_hit.dir; force = g_hit.force; at = g_hit.pt; }
    else { dir = Facing(n.yaw)*-1.0f; force = 2.0f; at = n.pos + Vec3(0, 1.0f, 0); }
    dir.y += 0.25f; n.rag->Impulse(at, Norm(dir), force); g_hit.valid = false;
}

static void KillNpc(Npc& n) {
    n.alive = false; n.state = NS_DEAD; n.deadT = 0; n.hp = 0; G.killed[n.id] = G.gameHours; StartRagdoll(n);
    const NpcStat& st = NpcInfo(n.kind);
    if (st.xp > 0) GainXp(st.xp + n.level*2);
    G.kills++;
    if (n.kind == NK_RAIDER) QuestAdvance(QE_KILL_RAIDER, 0);
    if (!st.hostile) { G.karma -= 10; Notify("Você matou um inocente. (Karma -10)", 3); }
    PlaySfx((n.kind == NK_DOG || n.kind == NK_CRAWLER) ? SND_BARK : SND_DEATH, 0.5f);
}

static void DamageNpc(Npc& n, float dmg, bool head) {
    if (!n.alive) return;
    if (head) dmg *= 2.0f;
    n.hp -= dmg; n.hurtT = 0.25f; G.p.hitMarker = head ? 0.35f : 0.2f;
    if (n.hp <= 0) { KillNpc(n); return; }
    if (n.hostile) { if (n.state != NS_COMBAT) { n.state = NS_COMBAT; n.lostT = 0; n.alert = 1; } }
    else if (n.state != NS_FLEE) { n.state = NS_FLEE; n.t = 0; }
}

struct Hit { float t; int type; Npc* npc; bool head; };
static Hit TraceShot(const Vec3& o, const Vec3& d, float range, bool terrain) {
    Hit h; h.t = range; h.type = 0; h.npc = 0; h.head = false;
    float tw = W.RayCast(o, d, range); if (tw < h.t) { h.t = tw; h.type = 2; }
    if (terrain) for (float t = 1.0f; t < h.t; t += 1.5f) { Vec3 q = o + d*t; if (q.y < TerrainH(q.x, q.z)) { h.t = t; h.type = 2; break; } }
    float a = d.x*d.x + d.z*d.z;
    if (a > 1e-8f) for (size_t i = 0; i < G.npcs.size(); i++) {
        Npc* n = G.npcs[i]; if (!n->alive) continue;
        if (fabsf(n->pos.x - o.x) > range + 2 || fabsf(n->pos.z - o.z) > range + 2) continue;
        const NpcStat& st = NpcInfo(n->kind); float r = st.radius*n->scale*(n->kind == NK_DOG ? 0.9f : 1.0f), ht = st.height*n->scale;
        float ox = o.x - n->pos.x, oz = o.z - n->pos.z, b = ox*d.x + oz*d.z, cc = ox*ox + oz*oz - r*r, disc = b*b - a*cc;
        if (disc < 0) continue;
        float t = (-b - sqrtf(disc))/a; if (t < 0 || t >= h.t) continue;
        float y = o.y + d.y*t - n->pos.y; if (y < 0 || y > ht) continue;
        h.t = t; h.type = 1; h.npc = n; h.head = y > ht*0.82f && n->kind != NK_DOG;
    }
    return h;
}
static void AddPuff(const Vec3& p, int kind) { Puff f; f.p = p; f.t = 0.5f; f.kind = kind; G.puffs.push_back(f); if (G.puffs.size() > 60) G.puffs.erase(G.puffs.begin()); }

static Vec3 MuzzlePos() {
    const Player& p = G.p; Vec3 f = Facing(p.bodyYaw), r(cosf(p.bodyYaw), 0, -sinf(p.bodyYaw));
    if (G.cam.first) return Vec3(p.pos.x, p.pos.y + 1.55f, p.pos.z) + Facing(p.yaw)*0.7f + Vec3(cosf(p.yaw), 0, -sinf(p.yaw))*0.18f - Vec3(0, 0.14f, 0);
    return Vec3(p.pos.x, p.pos.y + 1.4f, p.pos.z) + f*0.8f + r*0.3f;
}

static void PlayerShoot(const ItemDef& w) {
    Player& p = G.p; p.mag[p.weapon]--; p.fireCd = w.rate; p.recoil = Minf(1.0f, p.recoil + (w.pellets > 1 ? 0.9f : 0.5f));
    G.shotT = G.simTime; G.shotPos = p.pos;
    PlaySfx(p.weapon == IT_SHOTGUN ? SND_SHOTGUN : (p.weapon == IT_RIFLE ? SND_RIFLE : SND_PISTOL), 0.9f);
    Vec3 fwd = G.cam.fwd, right = G.cam.right; Vec3 up = Norm(Cross(right, fwd));
    Vec3 head(p.pos.x, p.pos.y + 1.6f, p.pos.z);
    Vec3 org = G.cam.eye + fwd*Maxf(0.0f, Dot(head - G.cam.eye, fwd));
    float moving = Sat(p.speed/4.0f);
    float spread = w.spread*(1.0f - 0.7f*p.aimAmt)*(p.sneak ? 0.8f : 1.0f)*(1.0f + 0.9f*moving) + p.recoil*0.012f;
    Vec3 mz = MuzzlePos();
    for (int i = 0; i < Maxi(1, w.pellets); i++) {
        Vec3 d = Norm(fwd + right*(g_rng.r(-1, 1)*spread) + up*(g_rng.r(-1, 1)*spread));
        Hit h = TraceShot(org, d, w.range, true);
        Vec3 pt = org + d*h.t;
        if (h.type == 1) {
            float falloff = w.pellets > 1 ? Sat(1.2f - h.t/w.range) : 1.0f;
            SetHit(pt, d, (p.weapon == IT_SHOTGUN ? 5.0f : (p.weapon == IT_RIFLE ? 5.5f : 2.5f))*(h.head ? 1.3f : 1.0f));
            DamageNpc(*h.npc, w.dmg*(0.9f + 0.05f*(float)p.attr[AT_PON]*0.2f)*falloff, h.head); AddPuff(pt, 1); PlaySfx(SND_HIT, 0.35f);
        } else if (h.type == 2) AddPuff(pt, 0);
        Tracer t; t.a = mz; t.b = pt; t.t = 0.08f; t.hit = h.type == 1; G.tracers.push_back(t);
        if (G.tracers.size() > 40) G.tracers.erase(G.tracers.begin());
    }
}

static void PlayerSwing(const ItemDef& w, bool bash) {
    Player& p = G.p; if (p.stam < 6.0f) return;
    p.stam -= bash ? 5.0f : 8.0f; p.stamDelay = 0.8f; p.fireCd = bash ? 0.7f : w.rate; p.swingT = 0.32f;
    PlaySfx(SND_SWING, 0.5f);
    G.shotT = G.simTime - 0.6f; G.shotPos = p.pos;
    Vec3 fwd = Facing(p.yaw); float dmg = bash ? 9.0f : w.dmg*(0.8f + 0.06f*(float)p.attr[AT_VIG]); float reach = bash ? 2.0f : w.range;
    Npc* best = 0; float bd = 1e9f;
    for (size_t i = 0; i < G.npcs.size(); i++) {
        Npc* n = G.npcs[i]; if (!n->alive) continue;
        float dx = n->pos.x - p.pos.x, dz = n->pos.z - p.pos.z, d = sqrtf(dx*dx + dz*dz);
        if (d > reach + 0.5f || fabsf(n->pos.y - p.pos.y) > 1.8f) continue;
        if (d > 0.4f && (fwd.x*dx + fwd.z*dz)/d < 0.45f) continue;
        if (d < bd) { bd = d; best = n; }
    }
    if (best) { SetHit(best->pos + Vec3(0, 1.1f, 0), fwd, bash ? 2.0f : 3.5f); DamageNpc(*best, dmg, false); AddPuff(best->pos + Vec3(0, 1.1f, 0), 1); PlaySfx(SND_HIT, 0.7f); }
}

void EquipWeapon(int id) {
    Player& p = G.p; if (id == p.weapon || CountItem(id) <= 0) return;
    p.weapon = id; p.reloadT = 0; p.fireCd = 0.35f; Notify(std::string("Equipado: ") + ItemInfo(id).name, 0); PlaySfx(SND_CLICK, 0.4f);
}
static void CycleWeapon(int dir) {
    static const int order[4] = { IT_PIPE, IT_PISTOL, IT_SHOTGUN, IT_RIFLE };
    int cur = 0; for (int i = 0; i < 4; i++) if (order[i] == G.p.weapon) cur = i;
    for (int k = 1; k <= 4; k++) { int j = ((cur + dir*k) % 4 + 4) % 4; if (CountItem(order[j]) > 0) { EquipWeapon(order[j]); return; } }
}
static void StartReload() {
    Player& p = G.p; const ItemDef& w = ItemInfo(p.weapon);
    if (w.type != ITY_GUN || p.reloadT > 0 || p.mag[p.weapon] >= w.mag) return;
    if (CountItem(w.ammo) <= 0) { Notify("Sem munição.", 3); return; }
    p.reloadT = w.reload; p.reloadW = p.weapon; PlaySfx(SND_RELOAD, 0.6f);
}
void UseItem(int idx) {
    Player& p = G.p; if (idx < 0 || idx >= (int)p.inv.size()) return;
    Slot s = p.inv[(size_t)idx]; const ItemDef& d = ItemInfo(s.id);
    switch (d.type) {
    case ITY_AID: {
        float feed = (s.id == IT_CANNED) ? 40.0f : (s.id == IT_RATION ? 60.0f : (s.id == IT_MEAT ? 28.0f : 0.0f)), drink = (s.id == IT_WATER) ? 45.0f : 0.0f;
        bool needFood = feed > 0 && p.hunger < 92.0f, needDrink = drink > 0 && p.thirst < 92.0f;
        if (feed > 0 || drink > 0) {
            if (!needFood && !needDrink && p.hp >= EffMaxHp() - 0.5f) { Notify(feed > 0 ? "Você não está com fome." : "Você não está com sede.", 0); return; }
            p.hunger = Minf(100.0f, p.hunger + feed); p.thirst = Minf(100.0f, p.thirst + drink);
            p.hp = Minf(EffMaxHp(), p.hp + d.heal); p.rad = Clampf(p.rad + d.rads, 0, 1000.0f); Notify(std::string("Consumiu: ") + d.name, 0); RemoveItem(s.id, 1); PlaySfx(SND_HEAL, 0.5f); break;
        }
        if (d.heal > 0 && p.hp >= EffMaxHp() - 0.5f && d.rads >= 0) { Notify("Sua vida já está no máximo.", 0); return; }
        if (d.rads < 0 && p.rad <= 0.5f) { Notify("Você não tem radiação.", 0); return; }
        p.hp = Minf(EffMaxHp(), p.hp + d.heal); p.rad = Clampf(p.rad + d.rads, 0, 1000.0f);
        Notify(std::string("Usou: ") + d.name, 0); RemoveItem(s.id, 1); PlaySfx(SND_HEAL, 0.6f); break; }
    case ITY_GUN: case ITY_MELEE: EquipWeapon(s.id); break;
    case ITY_ARTIFACT: p.artifact = (p.artifact == s.id) ? IT_NONE : s.id; Notify(p.artifact ? std::string("Estilhaço equipado: ") + d.name : std::string("Estilhaço removido: ") + d.name, 2); PlaySfx(SND_ZAP, 0.3f); p.hp = Minf(p.hp, EffMaxHp()); break;
    case ITY_MISC:
        if (s.id == IT_FUEL) {
            Vehicle* v = G.drive ? G.drive : NearestVehicle(p.pos, 7.0f);
            if (!v || v->state != 0) { Notify("Nenhum veículo por perto para reabastecer.", 3); return; }
            if (v->fuel >= 99.0f) { Notify("O tanque já está cheio.", 0); return; }
            v->fuel = Minf(100.0f, v->fuel + 50.0f); RemoveItem(IT_FUEL, 1); Notify(std::string("Reabasteceu: ") + VehInfo(v->type).name, 1); PlaySfx(SND_PICKUP, 0.6f);
        }
        break;
    case ITY_ARMOR: p.armor = (p.armor == s.id) ? IT_NONE : s.id; Notify(p.armor ? std::string("Vestiu: ") + d.name : std::string("Removeu: ") + d.name, 0); PlaySfx(SND_CLICK, 0.4f); break;
    default: break;
    }
}

// ---------------------------------------------------------------------
//  INTERACAO / DIALOGOS / COMERCIO
// ---------------------------------------------------------------------
static int CorpseTier(int kind) { return (kind == NK_RAIDER || kind == NK_BRUTE) ? 2 : ((kind == NK_DOG || kind == NK_CRAWLER) ? 0 : 1); }

static void UpdateTarget() {
    Target& t = G.tgt; t.type = 0; t.prompt[0] = 0; Player& p = G.p; Vec3 f = Facing(p.yaw); float best = 1e9f;
    for (size_t i = 0; i < G.npcs.size(); i++) {
        Npc* n = G.npcs[i]; float dx = n->pos.x - p.pos.x, dz = n->pos.z - p.pos.z, d = sqrtf(dx*dx + dz*dz);
        if (d > 3.4f || fabsf(n->pos.y - p.pos.y) > 2.2f) continue;
        if (d > 1.2f && (f.x*dx + f.z*dz)/d < 0.35f) continue;
        if (n->alive) {
            if (n->hostile || n->state == NS_FLEE) continue;
            if (d < best) { best = d; t.type = 1; t.id = n->id; t.dist = d; t.pos = n->pos; snprintf(t.prompt, sizeof(t.prompt), "Falar com %s", NpcInfo(n->kind).name); }
        } else if (!IsLooted(n->id) && !n->looted) {
            if (d < best) { best = d; t.type = 3; t.id = n->id; t.dist = d; t.pos = n->pos; snprintf(t.prompt, sizeof(t.prompt), "Revistar %s", NpcInfo(n->kind).name); }
        }
    }
    if (!G.drive) for (size_t i = 0; i < G.vehicles.size(); i++) {
        Vehicle* v = G.vehicles[i]; if (v->state != 0) continue; float d = Len2D(v->pos - p.pos); if (d > 3.8f || d >= best) continue;
        best = d; t.type = 4; t.id = v->id; t.dist = d; t.pos = v->pos; snprintf(t.prompt, sizeof(t.prompt), "Dirigir %s%s", VehInfo(v->type).name, v->fuel <= 0.5f ? " (sem combustível)" : "");
    }
    int pcx = ChunkOf(p.pos.x), pcz = ChunkOf(p.pos.z);
    static const char* verbs[CK_COUNT] = { "Abrir caixote", "Abrir armário", "Vasculhar porta-malas", "Revistar corpo", "Abrir cofre", "Usar terminal", "Recolher estilhaço" };
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
        const Chunk* ch = W.Get(pcx + dx, pcz + dz); if (!ch) continue;
        for (size_t i = 0; i < ch->conts.size(); i++) {
            const Container& k = ch->conts[i];
            float ex = k.x - p.pos.x, ez = k.z - p.pos.z, d = sqrtf(ex*ex + ez*ez), lim = (k.kind == CK_TRUNK) ? 3.6f : (k.kind == CK_ARTIFACT ? 1.9f : 2.7f);
            if (d > lim || fabsf(k.y - p.pos.y) > 2.2f) continue;
            if (d > 1.4f && (f.x*ex + f.z*ez)/d < 0.3f) continue;
            if (k.kind != CK_TERMINAL && IsLooted(k.id)) continue;
            if (d < best) { best = d; t.type = 2; t.id = k.id; t.dist = d; t.pos = Vec3(k.x, k.y, k.z); snprintf(t.prompt, sizeof(t.prompt), "%s", verbs[Clampi(k.kind, 0, CK_COUNT - 1)]); }
        }
    }
}
static const Container* FindContainer(u64 id) {
    int pcx = ChunkOf(G.p.pos.x), pcz = ChunkOf(G.p.pos.z);
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
        const Chunk* ch = W.Get(pcx + dx, pcz + dz); if (!ch) continue;
        for (size_t i = 0; i < ch->conts.size(); i++) if (ch->conts[i].id == id) return &ch->conts[i];
    }
    return 0;
}
static Npc* FindNpc(u64 id) { for (size_t i = 0; i < G.npcs.size(); i++) if (G.npcs[i]->id == id) return G.npcs[i]; return 0; }

static void EnterVehicle(Vehicle* v) {
    if (!v || v->state != 0) return; G.drive = v; G.p.sneak = false; G.p.pos = v->pos; G.p.vel = Vec3(); G.camIdle = 0; G.p.reloadT = 0;
    Notify(std::string("Ao volante: ") + VehInfo(v->type).name + (v->fuel <= 0.5f ? "  (sem combustível!)" : ""), 2); PlaySfx(SND_CLICK, 0.5f);
}
static void DoInteract() {
    Target t = G.tgt; if (t.type == 0) return;
    if (t.type == 4) { for (size_t i = 0; i < G.vehicles.size(); i++) if (G.vehicles[i]->id == t.id) EnterVehicle(G.vehicles[i]); return; }
    if (t.type == 1) { Npc* n = FindNpc(t.id); if (n) StartDialog(n); }
    else if (t.type == 3) {
        Npc* n = FindNpc(t.id); if (!n) return; n->looted = true; int tier = CorpseTier(n->kind);
        if (tier == 0) {
            G.looted[n->id] = G.gameHours;
            if (n->kind == NK_DOG || n->kind == NK_CRAWLER) { int m = 1 + (int)(n->id % 2); AddItem(IT_MEAT, m); Notify("+" + std::to_string(m) + " Carne Crua", 1); PlaySfx(SND_PICKUP, 0.5f); } else Notify("Nada de útil.", 0);
        } else TakeLoot(n->id, tier, CK_BODY);
    } else if (t.type == 2) {
        const Container* k = FindContainer(t.id); if (!k) return;
        if (k->kind == CK_TERMINAL) { QuestAdvance(QE_TERMINAL, 0); PlaySfx(SND_CLICK, 0.6f); }
        else TakeLoot(k->id, k->tier, k->kind);
    }
}

void StartDialog(Npc* n) {
    Dialog& d = G.dlg; d.npc = n->id; d.kind = n->kind; d.speaker = NpcInfo(n->kind).name; d.lines.clear(); d.page = 0; d.onEnd = 0;
    Rng r((u32)(n->id ^ (n->id >> 32)) + (u32)G.kills*7u + (u32)(G.simTime*0.1f));
    std::vector<std::string>& L = d.lines;
    QuestState* q = G.quests;
    if (n->kind == NK_ELDER) {
        d.onEnd = 1;
        if (q[Q_ARRIVE].state == 1) {
            L.push_back("Você veio do Silo Ômega? Faz oitenta anos que ninguém sai por aquela porta. Um Semeador... achei que fosse lenda. Seja bem-vindo a Esperança.");
            L.push_back("A lenda diz que o silo guardava a vida inteira do mundo antigo em sementes. Aqui, nada cresce além de poeira e teimosia. Se ainda existirem sementes vivas, três delas mudariam tudo.");
            L.push_back("Não temos muito, mas temos água limpa e uma fogueira. Em troca, preciso de alguém que saiba lidar com uma arma.");
            L.push_back("Primeiro: os saqueadores do Presa-Negra, a nordeste, estão nos sufocando. Levaram nossos mantimentos e ainda atacam quem viaja.");
            L.push_back("Segundo: a velha Torre de Rádio, a sudoeste, voltou a transmitir. Ninguém tem coragem de chegar perto.");
            L.push_back("E se precisar de rodas, procure o Tavo, o mecânico do Posto Ferrugem, a noroeste. Ele sempre tem trabalho para quem sabe dirigir.");
            L.push_back("Leve isto para a estrada. E volte inteiro.");
        } else {
            bool any = false;
            if (q[Q_SEEDS].state == 1 && CountItem(IT_SEED) >= 3) { L.push_back("Três sementes vivas... Dá para sentir o peso delas. Vamos plantar hoje mesmo. Esperança vai ter verde outra vez, Semeador. Obrigado."); any = true; }
            if (q[Q_RAIDERS].state == 1 && q[Q_RAIDERS].stage == 1) { L.push_back("Então o Presa-Negra ficou sem dentes? Você fez mais por Esperança em um dia do que nós em um ano. Tome, é o mínimo."); any = true; }
            if (q[Q_RADIO].state == 1 && q[Q_RADIO].stage == 1) { L.push_back("Um pedido de socorro de oitenta anos atrás... O mundo acabou e a máquina continua pedindo ajuda. Obrigado por descobrir, andarilho."); any = true; }
            if (!any) {
                if (q[Q_RAIDERS].state == 1) L.push_back("Os saqueadores ainda estão lá, a nordeste. Mantenha a cabeça baixa e a mira firme.");
                if (q[Q_RADIO].state == 1) L.push_back("A Torre de Rádio fica a sudoeste. Use o terminal da cabana ao pé da torre.");
                if (q[Q_SEEDS].state == 1) { char b[128]; snprintf(b, sizeof(b), "Sementes vivas: você tem %d de 3. Procure em silos, cofres e nas tralhas dos saqueadores.", CountItem(IT_SEED)); L.push_back(b); }
                if (q[Q_RAIDERS].state == 2 && q[Q_RADIO].state == 2 && q[Q_SEEDS].state == 2) { L.push_back("Esperança tem uma dívida com você. Por ora, o deserto está mais quieto. Mas ele nunca fica quieto por muito tempo."); L.push_back("(Fim do conteúdo de história desta versão. O mundo continua aberto: explore!)"); }
                if (L.empty()) L.push_back("Boa sorte lá fora.");
            }
        }
    } else if (n->kind == NK_MECHANIC) {
        d.onEnd = 3;
        if (q[Q_WHEELS].state == 0) {
            L.push_back("Opa, um rosto novo. Eu sou o Tavo. Conserto qualquer coisa que tenha roda, e este posto é minha oficina.");
            L.push_back("Tenho um buggy quase pronto ali nos fundos. Falta combustível para a partida: preciso de dois galões. Saqueadores e postos abandonados costumam ter.");
            L.push_back("E outra coisa: pago bem por estilhaços. Eles nascem dentro das fissuras, no campo a sudoeste do Silo. Dá para ouvir os estalos antes de pisar. Quem não ouve, não volta.");
            L.push_back("Se trouxer o combustível, o buggy é seu. Pode ficar com a caminhonete encostada ali também, se conseguir fazê-la andar.");
        } else {
            bool any = false; int art = CountItem(IT_ART_HEART) + CountItem(IT_ART_SPARK) + CountItem(IT_ART_CRYSTAL);
            if (q[Q_WHEELS].state == 1 && CountItem(IT_FUEL) >= 2) { L.push_back("Dois galões! Isso aí liga o bichinho. Pode levar o buggy, ele é seu. Tanque cheio e boa estrada."); any = true; }
            if (q[Q_ARTIFACT].state == 1 && art > 0) { L.push_back("Isso brilha como promessa de dinheiro. Um estilhaço de verdade! Aqui está o pagamento, mais uns remédios de brinde."); any = true; }
            if (!any) {
                if (q[Q_WHEELS].state == 1) L.push_back("Combustível, dois galões. Procure nos acampamentos dos saqueadores. Eles sempre têm algum.");
                if (q[Q_ARTIFACT].state == 1) L.push_back("Estilhaço? Fica no Campo de Fissuras. Ouça os estalos e dê a volta. Se estalar muito, corra.");
                if (L.empty()) L.push_back("Dica de mecânico: dirija rápido, mas olhe para onde vai. O deserto não perdoa capotagem. E reabasteça antes do fim do tanque!");
            }
        }
    } else if (n->kind == NK_MERCHANT) {
        d.onEnd = 2; L.push_back(r.chance(0.5f) ? "Ora, um cliente! Tenho o que o deserto deixou de sobra." : "Fichas falam mais alto que promessas. Dê uma olhada."); 
    } else if (n->kind == NK_GUARD) {
        static const char* g[4] = { "Mantenha as mãos longe do gatilho dentro da vila, e a gente se entende.", "Vi uma fumaça a nordeste ontem. Saqueadores, provavelmente.", "À noite os Ressecados chegam mais perto. Não ande sozinho no escuro.", "Se estiver ferido, o mercador vende injetores médicos. Caro, mas vende." };
        L.push_back(g[r.i(4)]);
    } else {
        static const char* s[6] = { "A água aqui ainda é boa. Isso vale mais que qualquer arma.", "Meu irmão foi até a rodovia e não voltou. Cuidado com os carros abandonados.", "Dizem que existe um abrigo selado a oeste. Eu não acredito.", "Quando a poeira abaixa, dá para ver a torre de rádio piscando.", "Você parece ter saído de um buraco. Sem ofensa.", "Zonas verdes brilham à noite. Não entre. Nunca." };
        L.push_back(s[r.i(6)]);
    }
    UiOpen(UI_DIALOG);
}
void FinishDialog() {
    int e = G.dlg.onEnd; u64 id = G.dlg.npc; G.dlg.onEnd = 0;
    if (e == 1) QuestAdvance(QE_TALK_ELDER, 0);
    if (e == 3) QuestAdvance(QE_TALK_MECHANIC, 0);
    if (e == 2) { OpenBarter(id); return; }
    UiOpen(UI_NONE);
}
int BuyPrice(int id) { return Maxi(1, (int)(ItemInfo(id).value*(1.5f - 0.04f*(float)G.p.attr[AT_LAB]))); }
int SellPrice(int id) { return Maxi(1, (int)(ItemInfo(id).value*(0.4f + 0.02f*(float)G.p.attr[AT_LAB]))); }
void OpenBarter(u64 npcId) {
    G.shop.clear(); G.shopNpc = npcId; G.shopCol = 0; G.shopSel[0] = G.shopSel[1] = 0;
    static const int stock[] = { IT_AMMO_10, IT_AMMO_SHELL, IT_AMMO_RIFLE, IT_MEDKIT, IT_ANTIRAD, IT_CANNED, IT_WATER, IT_RATION, IT_FUEL, IT_PISTOL, IT_SHOTGUN, IT_RIFLE, IT_ARMOR_LEATHER, IT_ARMOR_SCRAP };
    Rng r((u32)(npcId ^ (npcId >> 32)));
    for (size_t i = 0; i < sizeof(stock)/sizeof(stock[0]); i++) {
        bool core = i < 9; if (!core && !r.chance(0.55f)) continue;
        BarterItem b; b.id = stock[i]; b.price = BuyPrice(stock[i]); G.shop.push_back(b);
    }
    UiOpen(UI_BARTER);
}
void BarterTrade() {
    Player& p = G.p;
    if (G.shopCol == 0) {
        if (G.shop.empty()) return; int i = Clampi(G.shopSel[0], 0, (int)G.shop.size() - 1); const BarterItem& b = G.shop[(size_t)i];
        if (p.money < b.price) { Notify("Fichas insuficientes.", 3); PlaySfx(SND_EMPTY, 0.5f); return; }
        p.money -= b.price; AddItem(b.id, ItemInfo(b.id).type == ITY_AMMO ? 5 : 1);
        Notify(std::string("Comprou: ") + ItemInfo(b.id).name, 1); PlaySfx(SND_PICKUP, 0.5f);
    } else {
        if (p.inv.empty()) return; int i = Clampi(G.shopSel[1], 0, (int)p.inv.size() - 1); Slot s = p.inv[(size_t)i];
        if (s.id == p.weapon || s.id == p.armor) { Notify("Desequipe antes de vender.", 3); return; }
        if (s.id == IT_TAPE) { Notify("Item de missão: não pode ser vendido.", 3); return; }
        int n = ItemInfo(s.id).type == ITY_AMMO ? Mini(5, s.count) : 1;
        p.money += SellPrice(s.id)*n; RemoveItem(s.id, n); Notify(std::string("Vendeu: ") + ItemInfo(s.id).name, 1); PlaySfx(SND_PICKUP, 0.5f);
        G.shopSel[1] = Clampi(G.shopSel[1], 0, Maxi(0, (int)p.inv.size() - 1));
    }
}


// ---------------------------------------------------------------------
//  VEICULOS: fisica arcade, combustivel, dano, atropelamento, explosao
// ---------------------------------------------------------------------
const VehStat& VehInfo(int type) {
    static const VehStat V[VT_COUNT] = {
        // nome, vel. max (m/s), aceleracao, freio, vida, comprimento, meia-largura, entre-eixos
        { "Buggy do Deserto", 34.0f, 14.0f, 24.0f, 240.0f, 4.0f, 1.0f, 2.6f },
        { "Caminhonete Enferrujada", 25.0f, 9.5f, 20.0f, 420.0f, 5.0f, 1.1f, 3.1f },
    };
    return V[Clampi(type, 0, VT_COUNT - 1)];
}
static void VehGround(const Vehicle& v, float& avg, float& pitch, float& roll) {
    const VehStat& vs = VehInfo(v.type); Vec3 f = Facing(v.yaw), r(cosf(v.yaw), 0, -sinf(v.yaw)); float hl = vs.wheelBase*0.5f, hw = vs.halfW*0.9f;
    float hf = TerrainH(v.pos.x + f.x*hl, v.pos.z + f.z*hl), hb = TerrainH(v.pos.x - f.x*hl, v.pos.z - f.z*hl);
    float hr = TerrainH(v.pos.x + r.x*hw, v.pos.z + r.z*hw), hlf = TerrainH(v.pos.x - r.x*hw, v.pos.z - r.z*hw);
    avg = (hf + hb + hr + hlf)*0.25f; pitch = atan2f(hf - hb, vs.wheelBase); roll = atan2f(hr - hlf, 2.0f*hw);
}
static Vehicle* MakeVehicle(u64 id, int type, float x, float z, float yaw, float fuel, float hp, int state) {
    Vehicle* v = new Vehicle(); v->id = id; v->type = type; v->pos = Vec3(x, TerrainH(x, z), z); v->yaw = yaw; v->fuel = fuel; v->hp = hp > 0 ? hp : VehInfo(type).hp; v->state = state;
    float g; VehGround(*v, g, v->pitch, v->roll); v->pos.y = g; v->prevGround = g; G.vehicles.push_back(v); return v;
}
static Vehicle* FindVehicle(u64 id) { for (size_t i = 0; i < G.vehicles.size(); i++) if (G.vehicles[i]->id == id) return G.vehicles[i]; return 0; }
Vehicle* NearestVehicle(const Vec3& p, float maxDist) {
    Vehicle* b = 0; float bd = maxDist; for (size_t i = 0; i < G.vehicles.size(); i++) { float d = Len2D(G.vehicles[i]->pos - p); if (d < bd) { bd = d; b = G.vehicles[i]; } }
    return b;
}
void SpawnChunkVehicles(Chunk* c) {
    if (!c) return;
    for (size_t i = 0; i < c->vehicles.size(); i++) {
        const VehiclePt& vp = c->vehicles[i];
        if (vp.locked && G.quests[Q_WHEELS].state != 2) continue;
        if (G.vehOvr.count(vp.id) || FindVehicle(vp.id)) continue;
        MakeVehicle(vp.id, vp.type, vp.x, vp.z, vp.yaw, vp.fuel, 0, 0);
    }
    float x0 = (float)(c->cx*CHUNK_SIZE), z0 = (float)(c->cz*CHUNK_SIZE);
    for (std::unordered_map<u64, VehOvr>::iterator it = G.vehOvr.begin(); it != G.vehOvr.end();) {                 // veiculos movidos que estao neste chunk
        const VehOvr& o = it->second;
        if (o.x >= x0 && o.x < x0 + CHUNK_SIZE && o.z >= z0 && o.z < z0 + CHUNK_SIZE && !FindVehicle(it->first)) { MakeVehicle(it->first, o.type, o.x, o.z, o.yaw, o.fuel, o.hp, o.state); it = G.vehOvr.erase(it); }
        else ++it;
    }
}
static void HurtPlayerRaw(float dmg, const Vec3& from) { HurtPlayerEx(dmg, from, false); }

static void ExplodeVehicle(Vehicle& v) {
    v.state = 2; v.hp = 0; v.speed = 0; PlaySfx(SND_EXPLODE, 1.0f);
    for (int i = 0; i < 8; i++) AddPuff(v.pos + Vec3(g_rng.r(-1.5f, 1.5f), g_rng.r(0.3f, 1.8f), g_rng.r(-1.5f, 1.5f)), g_rng.chance(0.5f) ? 0 : 3);
    float dp = Len2D(G.p.pos - v.pos);
    if (G.drive == &v) { ExitVehicle(); HurtPlayerRaw(55.0f, v.pos); }
    else if (dp < 7.0f) HurtPlayerRaw(70.0f*(1.0f - dp/7.0f), v.pos);
    for (size_t i = 0; i < G.npcs.size(); i++) { Npc* n = G.npcs[i]; if (!n->alive) continue; float d = Len2D(n->pos - v.pos); if (d < 7.0f) { Vec3 away = Norm(Vec3(n->pos.x - v.pos.x, 0.4f, n->pos.z - v.pos.z)); SetHit(n->pos + Vec3(0, 0.8f, 0), away, 2.0f + 8.0f*(1.0f - d/7.0f)); DamageNpc(*n, 160.0f*(1.0f - d/7.0f), false); } }
}

static void DriveStep(Vehicle& v, float dt, float thr, float steerIn, bool brake, bool driven) {
    const VehStat& vs = VehInfo(v.type); bool fuelOk = v.fuel > 0 && v.state == 0;
    float ground, pitch, roll; VehGround(v, ground, pitch, roll);
    v.throttle = thr;
    if (fuelOk && thr > 0.05f) v.speed += vs.accel*thr*Sat(1.0f - v.speed/vs.maxSpeed)*dt;
    else if (thr < -0.05f) { if (v.speed > 0.8f) v.speed -= vs.brake*(-thr)*dt; else if (fuelOk) v.speed -= vs.accel*0.55f*(-thr)*dt; }
    else { float sgn = v.speed > 0 ? 1.0f : -1.0f; v.speed -= (v.speed*0.10f + sgn*0.8f)*dt; if (fabsf(v.speed) < 0.2f) v.speed = 0; }
    if (brake) { float sgn = v.speed > 0 ? 1.0f : -1.0f; float dv = 26.0f*dt; if (dv > fabsf(v.speed)) v.speed = 0; else v.speed -= sgn*dv; }
    v.speed -= 9.8f*sinf(pitch)*0.7f*dt;
    v.speed = Clampf(v.speed, -9.0f, vs.maxSpeed*1.15f);
    float st = steerIn*0.55f/(1.0f + fabsf(v.speed)*0.09f); if (brake) st *= 1.8f; v.steer = Approach(v.steer, st, 8.0f, dt);
    v.yaw = WrapAngle(v.yaw - (v.speed/vs.wheelBase)*tanf(v.steer)*dt);
    Vec3 f = Facing(v.yaw); float nx = v.pos.x + f.x*v.speed*dt, nz = v.pos.z + f.z*v.speed*dt;
    float half = vs.length*0.5f - vs.halfW, rr = vs.halfW + 0.1f, gy;
    float fx = nx + f.x*half, fz = nz + f.z*half, bx = nx - f.x*half, bz = nz - f.z*half;
    bool h1 = W.Collide(fx, fz, rr, v.pos.y, gy), h2 = W.Collide(bx, bz, rr, v.pos.y, gy);
    if (h1 || h2) {
        nx = ((fx - f.x*half) + (bx + f.x*half))*0.5f; nz = ((fz - f.z*half) + (bz + f.z*half))*0.5f;
        float imp = fabsf(v.speed);
        if (imp > 4.5f) { float dmg = (imp - 4.0f)*5.0f; v.hp -= dmg; PlaySfx(SND_CRASH, Sat(imp/18.0f)); if (driven) HurtPlayerRaw(dmg*0.12f, v.pos); AddPuff(v.pos + f*(vs.length*0.5f*(v.speed > 0 ? 1.0f : -1.0f)) + Vec3(0, 0.8f, 0), 0); }
        v.speed *= -0.22f;
    }
    nx = Clampf(nx, WORLD_EDGE, WORLD_SIZE - WORLD_EDGE); nz = Clampf(nz, WORLD_EDGE, WORLD_SIZE - WORLD_EDGE); v.pos.x = nx; v.pos.z = nz;
    // atropelamento
    if (fabsf(v.speed) > 4.0f) {
        Vec3 r(cosf(v.yaw), 0, -sinf(v.yaw));
        for (size_t i = 0; i < G.npcs.size(); i++) {
            Npc* n = G.npcs[i]; if (!n->alive) continue; float dx = n->pos.x - v.pos.x, dz = n->pos.z - v.pos.z; if (dx*dx + dz*dz > 25.0f) continue;
            float along = dx*f.x + dz*f.z, lat = dx*r.x + dz*r.z;
            if (fabsf(along) < vs.length*0.5f + 0.4f && fabsf(lat) < vs.halfW + 0.55f) { SetHit(n->pos + Vec3(0, 0.8f, 0), f*(v.speed > 0 ? 1.0f : -1.0f), fabsf(v.speed)*0.7f + 1.5f); DamageNpc(*n, fabsf(v.speed)*7.0f, false); AddPuff(n->pos + Vec3(0, 1.0f, 0), 1); PlaySfx(SND_HIT, 0.8f); v.hp -= 4.0f; v.speed *= 0.93f; }
        }
    }
    // relevo: segue o chao; saltos quando o terreno some embaixo
    VehGround(v, ground, pitch, roll);
    float dG = (ground - v.prevGround)/Maxf(dt, 0.001f); v.prevGround = ground;
    float ny = v.pos.y + v.vy*dt; v.vy -= 22.0f*dt;
    if (ny <= ground + 0.03f) { if (v.vy < -9.0f) { float dmg = (-v.vy - 8.0f)*6.0f; v.hp -= dmg; PlaySfx(SND_CRASH, 0.7f); if (driven) HurtPlayerRaw(dmg*0.1f, v.pos); } ny = ground; v.vy = Clampf(dG, -30.0f, 30.0f); }
    v.pos.y = ny; v.pitch = Approach(v.pitch, pitch, 9.0f, dt); v.roll = Approach(v.roll, roll, 9.0f, dt);
    v.wheelRot += v.speed*dt/0.45f;
    if (fuelOk) v.fuel = Maxf(0, v.fuel - dt*(0.035f + 0.018f*fabsf(thr)*Maxf(fabsf(v.speed), 3.0f)*0.35f));
    if (v.hp <= 0 && v.state == 0) ExplodeVehicle(v);
}

void ExitVehicle() {
    Vehicle* v = G.drive; if (!v) return; Player& p = G.p;
    Vec3 left(-cosf(v->yaw), 0, sinf(v->yaw)); float x = v->pos.x + left.x*2.4f, z = v->pos.z + left.z*2.4f, gy; W.Collide(x, z, 0.4f, v->pos.y, gy);
    p.pos = Vec3(x, TerrainH(x, z), z); p.vel = Vec3(); p.yaw = v->yaw; p.bodyYaw = v->yaw; p.onGround = true; G.drive = 0; SetEngine(false, 0);
    Notify("Você saiu do veículo.", 0);
}

static void UpdateVehicles(float dt) {
    float farDist = (float)(G.set.viewR + 2)*CHUNK_SIZE;
    for (size_t i = 0; i < G.vehicles.size();) {
        Vehicle* v = G.vehicles[i];
        if (v != G.drive) {
            if (fabsf(v->speed) > 0.05f || fabsf(v->vy) > 0.5f) DriveStep(*v, dt, 0, 0, true, false);
            if (v->state == 2) v->smoke += dt;
            if (Len2D(v->pos - G.p.pos) > farDist) { VehOvr o; o.type = v->type; o.x = v->pos.x; o.z = v->pos.z; o.yaw = v->yaw; o.fuel = v->fuel; o.hp = v->hp; o.state = (u8)v->state; G.vehOvr[v->id] = o; delete v; G.vehicles.erase(G.vehicles.begin() + (long)i); continue; }
        }
        i++;
    }
}

static void DrivingUpdate(float dt) {
    Player& p = G.p; Input& in = G.in; Vehicle& v = *G.drive;
    float sens = G.set.sens; float lx = in.lookDX*sens + in.padLookX*dt*2.8f*sens, ly = in.lookDY*sens + in.padLookY*dt*2.0f*sens; if (G.set.invertY) ly = -ly;
    p.yaw = WrapAngle(p.yaw - lx); p.pitch = Clampf(p.pitch + ly, -0.9f, 0.9f);
    if (fabsf(lx) + fabsf(ly) > 0.0005f) G.camIdle = 0; else G.camIdle += dt;
    if (G.camIdle > 1.2f && fabsf(v.speed) > 2.0f) { p.yaw = TurnToward(p.yaw, v.yaw + (v.speed < -1.0f ? PI : 0.0f), 2.4f*dt); p.pitch = Approach(p.pitch, -0.14f, 3.0f, dt); }
    if (in.camToggle) G.cam.first = !G.cam.first;
    if (in.interact) { ExitVehicle(); return; }
    int ns = Maxi(1, (int)ceilf(dt/0.02f)); for (int i = 0; i < ns && G.drive; i++) DriveStep(v, dt/(float)ns, in.moveY, in.moveX, in.brake, true);
    if (!G.drive) return;
    p.pos = v.pos; p.vel = Vec3(); p.bodyYaw = v.yaw; p.onGround = true; p.speed = fabsf(v.speed); p.sprinting = false; p.aimAmt = 0; p.animPhase += v.speed*dt*0.0f;
    float rf = RadAt(p.pos.x, p.pos.z); if (rf > 0.02f) p.rad = Minf(1000.0f, p.rad + rf*3.5f*dt);
    float stMax = 80.0f + 4.0f*(float)p.attr[AT_FOL]; p.stam = Minf(stMax, p.stam + 18.0f*dt);
    SetEngine(v.fuel > 0 && v.state == 0, Sat(fabsf(v.speed)/VehInfo(v.type).maxSpeed*0.85f + fabsf(in.moveY)*0.2f));
}

// ---------------------------------------------------------------------
//  ANOMALIAS: pontos invisiveis que estalam e descarregam (campo minado)
// ---------------------------------------------------------------------
static void UpdateAnomalies(float dt) {
    static std::unordered_map<u64, float> charge;
    Player& p = G.p; Vec3 ref = G.drive ? G.drive->pos : p.pos; int pcx = ChunkOf(ref.x), pcz = ChunkOf(ref.z);
    if (charge.size() > 200) charge.clear();
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
        const Chunk* ch = W.Get(pcx + dx, pcz + dz); if (!ch) continue;
        for (size_t i = 0; i < ch->anoms.size(); i++) {
            const Anomaly& a = ch->anoms[i]; float ex = ref.x - a.x, ez = ref.z - a.z, d = sqrtf(ex*ex + ez*ez);
            if (d < 10.0f && g_rng.f() < (1.0f - d/10.0f)*dt*6.0f) PlaySfx(SND_ZAP, 0.10f + 0.3f*(1.0f - d/10.0f));
            u64 key = ((u64)(u32)(int)(a.x*4.0f) << 32) | (u32)(int)(a.z*4.0f); bool inside = d < a.r && fabsf(ref.y - a.y) < 3.2f;
            float& c = charge[key];
            if (inside) {
                c += dt;
                if (c >= 0.6f && !p.dead) {
                    c = 0; float dmg = a.type == 0 ? 32.0f : 26.0f; PlaySfx(SND_ZAP, 0.9f);
                    for (int k = 0; k < 5; k++) { Tracer t; t.a = Vec3(a.x, a.y + 0.8f, a.z); t.b = Vec3(a.x + g_rng.r(-3, 3), a.y + g_rng.r(0.2f, 3.0f), a.z + g_rng.r(-3, 3)); t.t = 0.18f; t.hit = false; t.col = a.type == 0 ? 1 : 2; G.tracers.push_back(t); }
                    AddPuff(Vec3(a.x, a.y + 0.8f, a.z), a.type == 0 ? 2 : 3);
                    if (G.drive) { G.drive->hp -= dmg*0.8f; HurtPlayerRaw(dmg*0.3f, Vec3(a.x, a.y, a.z)); } else HurtPlayerRaw(dmg, Vec3(a.x, a.y, a.z));
                }
            } else if (c > 0) c = Maxf(0, c - dt*2.0f);
        }
    }
}

static void UpdateSurvival(float dt) {
    Player& p = G.p; float hrs = dt/120.0f;                               // 1 s real = 30 s de jogo
    float hungerBefore = p.hunger, thirstBefore = p.thirst;
    p.thirst = Maxf(0, p.thirst - 6.0f*hrs); p.hunger = Maxf(0, p.hunger - 4.0f*hrs);
    if (hungerBefore > 25.0f && p.hunger <= 25.0f) Notify("Você está com fome. Coma algo.", 3);
    if (thirstBefore > 25.0f && p.thirst <= 25.0f) Notify("Você está com sede. Beba água.", 3);
    if ((p.hunger <= 0 || p.thirst <= 0) && p.hp > 5.0f) p.hp = Maxf(5.0f, p.hp - 0.6f*dt);       // a fome machuca, mas nao mata sozinha
    if (p.thirst <= 0) p.stam = Minf(p.stam, 40.0f);
}

// ---------------------------------------------------------------------
//  ATUALIZACAO DO JOGADOR
// ---------------------------------------------------------------------
static void UpdatePlayer(float dt) {
    Player& p = G.p; Input& in = G.in; const ItemDef& wd = ItemInfo(p.weapon); bool gun = wd.type == ITY_GUN;
    if (p.dead) return;
    UpdateSurvival(dt);
    if (G.drive) { DrivingUpdate(dt); return; }
    // ---- olhar ----
    float sens = G.set.sens*(p.aimAmt > 0.5f ? 0.55f : 1.0f);
    p.yaw = WrapAngle(p.yaw - in.lookDX*sens - in.padLookX*dt*2.8f*sens);
    float dy = in.lookDY*sens + in.padLookY*dt*2.0f*sens; if (G.set.invertY) dy = -dy;
    p.pitch = Clampf(p.pitch + dy, -1.3f, 1.4f);
    if (in.camToggle) G.cam.first = !G.cam.first;
    if (in.sneakToggle) { p.sneak = !p.sneak; }
    if (in.selectWeapon) { static const int order[4] = { IT_PIPE, IT_PISTOL, IT_SHOTGUN, IT_RIFLE }; EquipWeapon(order[Clampi(in.selectWeapon, 1, 4) - 1]); }
    if (in.nextWeapon || in.wheel < -0.5f) CycleWeapon(1);
    if (in.prevWeapon || in.wheel > 0.5f) CycleWeapon(-1);
    if (in.heal) { int idx = -1; for (size_t i = 0; i < p.inv.size(); i++) if (p.inv[i].id == IT_MEDKIT) idx = (int)i; if (idx >= 0) UseItem(idx); else Notify("Sem injetores médicos.", 3); }
    // ---- movimento ----
    float mx = in.moveX, my = in.moveY, ml = sqrtf(mx*mx + my*my); if (ml > 1.0f) { mx /= ml; my /= ml; ml = 1.0f; }
    Vec3 fw = Facing(p.yaw), rt(cosf(p.yaw), 0, -sinf(p.yaw)), wish = fw*my + rt*mx;
    bool overweight = CarryWeight() > MaxCarry();
    bool wantAim = in.aim && gun && p.reloadT <= 0;
    p.sprinting = in.sprint && ml > 0.3f && my > -0.2f && p.stam > 4.0f && !p.sneak && !overweight && !wantAim && p.onGround;
    float spd = p.sneak ? 1.9f : (p.sprinting ? 6.6f : 3.7f); if (wantAim) spd *= 0.65f; if (overweight) spd *= 0.55f;
    float rate = p.onGround ? 11.0f : 2.0f;
    p.vel.x = Approach(p.vel.x, wish.x*spd, rate, dt); p.vel.z = Approach(p.vel.z, wish.z*spd, rate, dt);
    if (p.sprinting) { p.stam -= 16.0f*dt; p.stamDelay = 0.9f; }
    if (in.jump && p.onGround && p.stam >= 7.0f) { p.vel.y = 7.2f; p.onGround = false; p.stam -= 7.0f; p.stamDelay = 0.8f; }
    int ns = Maxi(1, (int)ceilf(dt/0.016f)); float h = dt/(float)ns;
    for (int s = 0; s < ns; s++) {
        p.vel.y -= 22.0f*h;
        float nx = p.pos.x + p.vel.x*h, nz = p.pos.z + p.vel.z*h, ny = p.pos.y + p.vel.y*h, gy;
        W.Collide(nx, nz, 0.4f, p.pos.y, gy);
        float th = TerrainH(nx, nz), thOld = TerrainH(p.pos.x, p.pos.z), moved = sqrtf(Sqr(nx - p.pos.x) + Sqr(nz - p.pos.z));
        if (p.onGround && moved > 1e-4f && (th - thOld)/moved > 1.15f && th > p.pos.y + 0.03f) { nx = p.pos.x; nz = p.pos.z; th = thOld; p.vel.x *= 0.6f; p.vel.z *= 0.6f; }
        p.pos.x = nx; p.pos.z = nz;
        float ground = Maxf(th, gy);
        if (ny <= ground) { ny = ground; if (p.vel.y < 0) p.vel.y = 0; p.onGround = true; }
        else if (p.onGround && p.vel.y <= 0 && ny - ground < 0.45f) { ny = ground; p.vel.y = 0; }
        else p.onGround = false;
        p.pos.y = ny;
    }
    static float edgeMsg = 0; edgeMsg -= dt;
    if (p.pos.x < WORLD_EDGE || p.pos.z < WORLD_EDGE || p.pos.x > WORLD_SIZE - WORLD_EDGE || p.pos.z > WORLD_SIZE - WORLD_EDGE) {
        p.pos.x = Clampf(p.pos.x, WORLD_EDGE, WORLD_SIZE - WORLD_EDGE); p.pos.z = Clampf(p.pos.z, WORLD_EDGE, WORLD_SIZE - WORLD_EDGE);
        if (edgeMsg <= 0) { Notify("Tempestade de radiação adiante: o fim do mundo conhecido. Volte.", 3); edgeMsg = 5; }
    }
    p.speed = sqrtf(p.vel.x*p.vel.x + p.vel.z*p.vel.z);
    float oldPh = p.animPhase; p.animPhase += p.speed*dt*4.2f*(p.onGround ? 1.0f : 0.3f);
    if (p.onGround && p.speed > 1.0f && floorf(oldPh/PI) != floorf(p.animPhase/PI)) PlaySfx(SND_STEP, p.sneak ? 0.08f : (p.sprinting ? 0.32f : 0.2f));
    // ---- corpo gira ----
    bool shooting = (in.fire && gun) || wantAim || p.swingT > 0;
    if (G.cam.first || shooting) p.bodyYaw = TurnToward(p.bodyYaw, p.yaw, 14.0f*dt);
    else if (p.speed > 0.6f) p.bodyYaw = TurnToward(p.bodyYaw, YawOf(p.vel.x, p.vel.z), 10.0f*dt);
    else if (fabsf(AngleDiff(p.bodyYaw, p.yaw)) > 1.6f) p.bodyYaw = TurnToward(p.bodyYaw, p.yaw, 4.0f*dt);
    // ---- estamina, timers ----
    float maxSt = 80.0f + 4.0f*(float)p.attr[AT_FOL];
    if (p.stamDelay > 0) p.stamDelay -= dt; else p.stam = Minf(maxSt, p.stam + 18.0f*(1.0f + ArtifactBonus(1))*dt);
    p.stam = Clampf(p.stam, 0, maxSt);
    p.fireCd -= dt; p.swingT = Maxf(0, p.swingT - dt); p.hurtT = Maxf(0, p.hurtT - dt); p.hitMarker = Maxf(0, p.hitMarker - dt); p.recoil = Maxf(0, p.recoil - dt*2.2f);
    p.aimAmt = Approach(p.aimAmt, wantAim ? 1.0f : 0.0f, 12.0f, dt);
    // ---- recarregar / atirar ----
    if (p.reloadT > 0) {
        p.reloadT -= dt;
        if (p.reloadT <= 0 && p.reloadW == p.weapon) { int take = Mini(wd.mag - p.mag[p.weapon], CountItem(wd.ammo)); if (take > 0) { p.mag[p.weapon] += take; RemoveItem(wd.ammo, take); } }
    }
    if (in.reload) StartReload();
    if (in.bash && p.fireCd <= 0 && p.reloadT <= 0) PlayerSwing(wd, true);
    else if (in.fire && p.fireCd <= 0 && p.reloadT <= 0) {
        if (wd.type == ITY_MELEE) PlayerSwing(wd, false);
        else if (gun) {
            if (p.mag[p.weapon] > 0) PlayerShoot(wd);
            else { p.fireCd = 0.3f; if (CountItem(wd.ammo) > 0) StartReload(); else { PlaySfx(SND_EMPTY, 0.5f); Notify("Sem munição.", 3); } }
        }
    }
    // ---- radiacao ----
    float rf = RadAt(p.pos.x, p.pos.z);
    if (rf > 0.02f) {
        p.rad = Minf(1000.0f, p.rad + rf*9.0f*Maxf(0.1f, 1.0f - 0.05f*(float)p.attr[AT_SOB] - ArtifactBonus(2))*dt);
        if (g_rng.f() < rf*dt*14.0f) PlaySfx(SND_GEIGER, 0.35f);
    }
    if (p.rad >= 1000.0f) { p.hp -= 2.0f*dt; if (p.hp <= 0) { p.hp = 0; p.dead = true; PlaySfx(SND_DEATH, 0.8f); UiOpen(UI_DEAD); } }
    p.hp = Minf(p.hp, EffMaxHp());
    if (in.interact) DoInteract();
}

// ---------------------------------------------------------------------
//  CAMERA (primeira / terceira pessoa)
// ---------------------------------------------------------------------
static void UpdateCamera(float dt) {
    Player& p = G.p; Camera& c = G.cam;
    float eyeH = p.sneak ? 1.28f : 1.68f; static float eh = 1.68f; eh = Approach(eh, eyeH, 10.0f, dt);
    float cp = cosf(p.pitch);
    c.yaw = p.yaw; c.pitch = p.pitch; c.fwd = Vec3(-sinf(p.yaw)*cp, sinf(p.pitch), -cosf(p.yaw)*cp); c.right = Vec3(cosf(p.yaw), 0, -sinf(p.yaw));
    float baseFov = G.set.fov, zoom = (p.weapon == IT_RIFLE) ? 0.5f : 0.75f;
    c.fov = baseFov*Lerpf(1.0f, zoom, p.aimAmt) + (p.sprinting ? 4.0f : 0.0f);
    if (G.drive) {
        const Vehicle& v = *G.drive; const VehStat& vs = VehInfo(v.type); Vec3 piv(v.pos.x, v.pos.y + 1.7f, v.pos.z);
        if (c.first) {
            Vec3 lf = Facing(v.yaw), lr(cosf(v.yaw), 0, -sinf(v.yaw)); c.eye = Vec3(v.pos.x, v.pos.y + 1.45f, v.pos.z) - lr*0.4f + lf*(vs.length*0.05f - 0.2f); c.dist = 0;
            c.fov = G.set.fov + Sat(fabsf(v.speed)/vs.maxSpeed)*10.0f; return;
        }
        float want = vs.length*1.75f + 1.5f; Vec3 desired = piv - c.fwd*want + Vec3(0, 0.6f, 0);
        Vec3 dv = desired - piv; float len = Len(dv); Vec3 dir = dv*(1.0f/Maxf(len, 0.001f)); float t = W.RayCast(piv, dir, len); if (t < len) len = Maxf(0.8f, t - 0.3f);
        Vec3 e = piv + dir*len; float th = TerrainH(e.x, e.z) + 0.5f; if (e.y < th) e.y = th; c.eye = e; c.dist = len;
        c.fov = G.set.fov + Sat(fabsf(v.speed)/vs.maxSpeed)*12.0f; return;
    }
    Vec3 head(p.pos.x, p.pos.y + eh, p.pos.z);
    if (c.first) {
        float bob = sinf(p.animPhase)*0.03f*Sat(p.speed/4.0f);
        c.eye = head + Vec3(0, bob, 0) + c.fwd*0.12f; c.dist = 0;
    } else {
        float want = (p.sneak ? 2.8f : 3.5f)*Lerpf(1.0f, 0.55f, p.aimAmt), shoulder = Lerpf(0.55f, 0.75f, p.aimAmt);
        Vec3 desired = head - c.fwd*want + c.right*shoulder + Vec3(0, 0.25f, 0);
        Vec3 dv = desired - head; float len = Len(dv); Vec3 dir = dv*(1.0f/Maxf(len, 0.001f));
        float t = W.RayCast(head, dir, len); if (t < len) len = Maxf(0.4f, t - 0.3f);
        Vec3 e = head + dir*len; float th = TerrainH(e.x, e.z) + 0.4f; if (e.y < th) e.y = th;
        c.eye = e; c.dist = len;
    }
}

// ---------------------------------------------------------------------
//  LACO DE SIMULACAO
// ---------------------------------------------------------------------
static void UpdateNpcs(float dt, bool playerPresent) {
    NpcCtx c; c.pp = G.p.pos; c.pAlive = playerPresent && !G.p.dead; c.pSneak = G.p.sneak; c.pSprint = G.p.sprinting; c.now = G.simTime; c.shotT = G.shotT; c.shotPos = G.shotPos; c.night = NightFactor();
    Vec3 ref = playerPresent ? G.p.pos : ChunkCenter(HOPE_CX, HOPE_CZ);
    std::vector<Npc*> act; act.reserve(G.npcs.size());
    for (size_t i = 0; i < G.npcs.size(); i++) { Npc* n = G.npcs[i]; if (Sqr(n->pos.x - ref.x) + Sqr(n->pos.z - ref.z) < Sqr(150.0f)) act.push_back(n); }
    G.npcActive = (int)act.size();
    if (!playerPresent) { c.pp = Vec3(-9999, 0, -9999); }
    G.pool.ParallelFor((int)act.size(), 12, [&](int a, int b) { for (int i = a; i < b; i++) NpcThink(*act[(size_t)i], dt, c); });
    if (!playerPresent) return;
    for (size_t i = 0; i < act.size(); i++) {
        Npc* n = act[i]; if (!n->out) continue;
        float d = Len2D(n->pos - G.p.pos);
        if (n->out == 1) {
            Tracer t; t.a = n->fireFrom; t.b = n->fireTo; t.t = 0.1f; t.hit = false; G.tracers.push_back(t);
            PlaySfx(SND_PISTOL, Sat(1.0f - d/70.0f)*0.7f);
            if (n->fireHit) { AddPuff(n->fireTo, 1); HurtPlayer(n->fireDmg, n->pos); } else AddPuff(n->fireTo, 0);
        } else if (n->out == 2) {
            PlaySfx((n->kind == NK_DOG || n->kind == NK_CRAWLER) ? SND_BARK : SND_GROWL, Sat(1.0f - d/30.0f)*0.7f);
            if (d < NpcInfo(n->kind).reach + 0.9f) { HurtPlayer(n->fireDmg, n->pos); if (n->kind == NK_ASHEN) G.p.rad = Minf(1000.0f, G.p.rad + 25.0f); }
        } else if (n->out == 3) PlaySfx((n->kind == NK_DOG || n->kind == NK_CRAWLER) ? SND_BARK : ((n->kind == NK_ASHEN || n->kind == NK_BRUTE) ? SND_GROWL : SND_CLICK), Sat(1.0f - d/50.0f)*0.6f);
        n->out = 0;
    }
}

static void UpdateRagdolls(float dt) {
    std::vector<Ragdoll*> act; std::vector<Npc*> own;
    for (size_t i = 0; i < G.npcs.size(); i++) { Npc* n = G.npcs[i]; if (!n->alive && n->rag && !n->rag->asleep && Sqr(n->pos.x - G.p.pos.x) + Sqr(n->pos.z - G.p.pos.z) < Sqr(130.0f)) { act.push_back(n->rag); own.push_back(n); } }
    G.ragActive = (int)act.size(); if (act.empty()) return;
    G.pool.ParallelFor((int)act.size(), 2, [&](int a, int b) { for (int i = a; i < b; i++) act[(size_t)i]->Step(dt); });   // cada ragdoll em uma thread
    for (size_t i = 0; i < act.size(); i++) { Vec3 pv = act[i]->Pelvis(); own[i]->pos = Vec3(pv.x, TerrainH(pv.x, pv.z), pv.z); }        // saque/interacao seguem o corpo
}

void TestKillNpc(Npc* n, const Vec3& dir, float force) { SetHit(n->pos + Vec3(0, 0.8f, 0), dir, force); DamageNpc(*n, 1e6f, false); }   // gancho para testes

void GameClearWorld() {
    for (size_t i = 0; i < G.npcs.size(); i++) delete G.npcs[i];
    for (size_t i = 0; i < G.vehicles.size(); i++) delete G.vehicles[i];
    G.vehicles.clear(); G.vehOvr.clear(); G.drive = 0; SetEngine(false, 0);
    G.npcs.clear(); W.Reset(); G.tracers.clear(); G.puffs.clear(); G.msgs.clear();
    G.killed.clear(); G.looted.clear(); G.npcOvr.clear(); G.discovered.clear(); G.tgt.type = 0;
}

void GameNewGame(const char* name) {
    GameClearWorld();
    G.p = Player(); Player& p = G.p;
    strncpy(p.name, (name && name[0]) ? name : "Andarilho", 31); p.name[31] = 0;
    p.maxhp = 60.0f + 8.0f*(float)p.attr[AT_CON]; p.hp = p.maxhp; p.stam = 100; p.money = 35;
    AddItem(IT_PIPE, 1); AddItem(IT_PISTOL, 1); AddItem(IT_AMMO_10, 24); AddItem(IT_MEDKIT, 3); AddItem(IT_WATER, 2); AddItem(IT_CANNED, 2); AddItem(IT_JUNK, 3); AddItem(IT_ARMOR_LEATHER, 1);
    p.armor = IT_ARMOR_LEATHER; p.weapon = IT_PISTOL; p.mag[IT_PISTOL] = 12;
    float bx = (float)(BUNKER_CX*CHUNK_SIZE + 32), bz = (float)(BUNKER_CZ*CHUNK_SIZE + 32 + 7);
    p.pos = Vec3(bx, TerrainH(bx, bz), bz); p.yaw = -PI*0.5f; p.bodyYaw = p.yaw; p.pitch = -0.1f;
    memset(G.quests, 0, sizeof(G.quests)); G.quests[Q_ARRIVE].state = 1;
    G.gameHours = 8.5f; G.playSeconds = 0; G.karma = 0; G.kills = 0; G.simTime = 0; G.shotT = -100; G.autoT = 280.0f; G.lastLocation = ""; G.locT = 0;
    G.cam.first = false; G.state = GS_LOADING; G.ui = UI_NONE; G.loadT = 0; G.introPending = true;
    Notify("Você deixa o Silo Ômega. O mundo lá fora é vasto e silencioso.", 2);
    Notify("Siga para leste: há fumaça no horizonte (Esperança).", 0);
}

static void StartIntro() {
    Dialog& d = G.dlg; d.npc = 0; d.kind = -1; d.speaker = "Voz do Silo"; d.page = 0; d.onEnd = 0; d.lines.clear();
    d.lines.push_back("PROTOCOLO SEMEADURA: ciclo de hibernação concluído. Tempo decorrido desde o selamento: 80 anos e 3 meses.");
    d.lines.push_back("Bem-vindo de volta, Semeador. O Céu caiu enquanto você dormia. Os destroços abriram Fissuras no chão e soltaram Estilhaços que ainda queimam.");
    d.lines.push_back("A terra agora é cinza e a poeira é ferrosa. Mas o silo guardou a vida: sementes vivas, espalhadas por silos, cofres e pelas mãos dos sobreviventes.");
    d.lines.push_back("Ao leste, fumaça de uma vila: Esperança. Procure o Ancião Silas. Coma, beba e reabasteça: o deserto cobra de quem esquece de si mesmo.");
    d.lines.push_back("Há uma caminhonete velha ao sul do silo (E para dirigir). F1 mostra todos os controles. Boa semeadura.");
    UiOpen(UI_DIALOG);
}
void GameFinishLoadSetup() { G.state = GS_LOADING; G.ui = UI_NONE; G.loadT = 0; G.cam.first = G.cam.first; G.tgt.type = 0; }

void RespawnAfterDeath() {
    RefreshSaveList();
    if (!g_saveList.empty() && GameLoadFile(g_saveList[0].path)) return;
    char nm[32]; strncpy(nm, G.p.name, 31); nm[31] = 0; GameNewGame(nm);
}

void ApplyOptions() { W.viewRadius = Clampi(G.set.viewR, 3, 8); }

static void UpdateEffects(float dt) {
    for (size_t i = 0; i < G.msgs.size();) { G.msgs[i].t -= dt; if (G.msgs[i].t <= 0) G.msgs.erase(G.msgs.begin() + (long)i); else i++; }
    for (size_t i = 0; i < G.tracers.size();) { G.tracers[i].t -= dt; if (G.tracers[i].t <= 0) G.tracers.erase(G.tracers.begin() + (long)i); else i++; }
    for (size_t i = 0; i < G.puffs.size();) { G.puffs[i].t -= dt; if (G.puffs[i].t <= 0) G.puffs.erase(G.puffs.begin() + (long)i); else i++; }
    G.locT = Maxf(0, G.locT - dt);
}

void GameInit() {
    int hw = 4;
#ifdef _WIN32
    SYSTEM_INFO si; GetSystemInfo(&si); hw = (int)si.dwNumberOfProcessors;
#else
    hw = (int)std::thread::hardware_concurrency(); if (hw <= 0) hw = 4;
#endif
    G.pool.Start(Clampi(hw - 1, 2, 8));
    W.Init(&G.pool, 20772207u);
    LoadSettings(); ApplyOptions();
    RefreshSaveList(); G.hasSaves = !g_saveList.empty();
    G.state = GS_MENU; G.ui = UI_MAIN; G.menuSel = G.hasSaves ? 0 : 1;
}
void GameShutdown() {
    SaveSettings();
    G.pool.Stop();                                        // espera gravacoes de save pendentes
    for (size_t i = 0; i < G.npcs.size(); i++) delete G.npcs[i];
    for (size_t i = 0; i < G.vehicles.size(); i++) delete G.vehicles[i];
    G.npcs.clear(); G.vehicles.clear(); W.Reset();
}

void GameFrame(float dt) {
    dt = Clampf(dt, 0.0001f, 0.05f); G.uiT += dt;
    int ok = G.saveOk.exchange(0), fail = G.saveFail.exchange(0);
    if (ok) { Notify("Jogo salvo.", 2); PlaySfx(SND_SAVE, 0.6f); RefreshSaveList(); }
    if (fail) Notify("ERRO ao gravar o save!", 3);

    int uiStart = G.ui;
    UiUpdate(dt);

    bool menu = G.state == GS_MENU;
    Vec3 center = menu ? ChunkCenter(HOPE_CX, HOPE_CZ) : G.p.pos;
    W.viewRadius = menu ? 3 : Clampi(G.set.viewR, 3, 8);
    W.Update(center, [](Chunk* c) { SpawnChunkNpcs(c); SpawnChunkVehicles(c); }, [](Chunk* c) { DespawnChunkNpcs(c); });

    if (G.state == GS_LOADING) {
        G.loadT += dt;
        if ((W.NearReady(G.p.pos, 2) && G.loadT > 0.4f) || G.loadT > 30.0f) { G.state = GS_PLAYING; UiOpen(UI_NONE); if (G.introPending) { G.introPending = false; StartIntro(); } }
        UpdateCamera(dt); UpdateEffects(dt); return;
    }
    if (menu) {
        G.menuCamT += dt; float a = G.menuCamT*0.07f; Vec3 c = center;
        float h = TerrainH(c.x, c.z);
        Camera& cam = G.cam; cam.first = false; cam.fov = 62.0f;
        cam.eye = Vec3(c.x + cosf(a)*46.0f, h + 17.0f, c.z + sinf(a)*46.0f);
        Vec3 d = Norm(Vec3(c.x, h + 3.0f, c.z) - cam.eye); cam.fwd = d; cam.yaw = YawOf(d.x, d.z); cam.pitch = asinf(d.y);
        cam.right = Vec3(cosf(cam.yaw), 0, -sinf(cam.yaw));
        G.gameHours += dt/120.0f; UpdateNpcs(dt, false); UpdateEffects(dt); return;
    }

    bool run = (uiStart == UI_NONE && G.ui == UI_NONE);          // fechar um menu neste quadro nao dispara a acao do mesmo botao
    if (run) {
        G.simTime += dt; G.playSeconds += dt; G.gameHours += dt/120.0f;
        UpdatePlayer(dt); UpdateVehicles(dt); UpdateAnomalies(dt); UpdateNpcs(dt, true); UpdateRagdolls(dt); UpdateTarget();
        // locais
        int cx = ChunkOf(G.p.pos.x), cz = ChunkOf(G.p.pos.z); const char* area = AreaName(G.p.pos.x, G.p.pos.z);
        if (G.lastLocation != area) { G.lastLocation = area; G.location = area; if (strcmp(area, "Ermos") != 0) G.locT = 4.0f; }
        int poi = PoiAt(cx, cz);
        if (poi && !G.discovered.count(ChunkKey(cx, cz)) && Sqr(G.p.pos.x - (float)(cx*CHUNK_SIZE + 32)) + Sqr(G.p.pos.z - (float)(cz*CHUNK_SIZE + 32)) < Sqr(48.0f)) {
            G.discovered.insert(ChunkKey(cx, cz)); Notify(std::string("Local descoberto: ") + PoiName(poi, cx, cz), 2); GainXp(8);
        }
        G.autoT += dt; if (G.autoT >= 300.0f && !G.p.dead) { G.autoT = 0; RequestSave(SK_AUTO, ""); }
        float night = NightFactor(); SetAmbience(0.25f + 0.2f*night + 0.2f*Sat(G.p.speed/6.0f), RadAt(G.p.pos.x, G.p.pos.z));
        if (G.in.quicksave && !G.p.dead) RequestSave(SK_QUICK, "");
        if (G.in.quickload) { RefreshSaveList(); if (!g_saveList.empty()) GameLoadFile(g_saveList[0].path); else Notify("Nenhum save encontrado.", 3); }
    } else {
        SetEngine(false, 0);   // menu aberto: mundo congelado
    }
    UpdateCamera(dt); UpdateEffects(dt);
}

void GameClearEdges() {
    Input& i = G.in;
    i.lookDX = i.lookDY = 0; i.jump = i.interact = i.reload = i.diary = i.quicksave = i.quickload = i.camToggle = false;
    i.nextWeapon = i.prevWeapon = i.bash = i.heal = i.sneakToggle = i.pause = i.debug = false; i.selectWeapon = 0; i.wheel = 0;
    i.up = i.down = i.left = i.right = i.ok = i.back = i.del = i.tabL = i.tabR = false; i.chars.clear(); i.backspace = false;
}

bool IsContainerLooted(u64 id) { return IsLooted(id); }
