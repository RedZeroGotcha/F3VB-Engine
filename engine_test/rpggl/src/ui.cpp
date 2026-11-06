// =====================================================================
//  ui.cpp - logica dos menus (principal, pausa, save/load, opcoes,
//  Diário, dialogo, comercio).  Sem desenho: isso fica em hud.cpp.
// =====================================================================
#include "game.h"

static int Wrap(int v, int n) { return n <= 0 ? 0 : ((v % n) + n) % n; }

void UiOpen(int ui) {
    if (ui != G.ui) G.prevUi = G.ui;
    G.ui = ui; G.menuSel = 0; G.confirmDel = false; G.scroll = 0; G.listSel = 0; G.uiT = 0;
    if (ui == UI_SAVE || ui == UI_LOAD) RefreshSaveList();
    if (ui == UI_DIARY) {
        std::sort(G.p.inv.begin(), G.p.inv.end(), [](const Slot& a, const Slot& b) { int ta = ItemInfo(a.id).type, tb = ItemInfo(b.id).type; return ta != tb ? ta < tb : a.id < b.id; });
        G.tab = 0;
    }
    if (ui == UI_MAIN) { RefreshSaveList(); G.menuSel = G.hasSaves ? 0 : 1; }
    if (ui == UI_NEWGAME) G.nameBuf = "";
    if (ui == UI_DEAD) G.menuSel = 0;
}

static void ListNav(int count) {
    Input& in = G.in; if (count <= 0) return;
    if (in.up) { G.listSel = Wrap(G.listSel - 1, count); G.confirmDel = false; }
    if (in.down) { G.listSel = Wrap(G.listSel + 1, count); G.confirmDel = false; }
}
static void MenuNav(int count) {
    Input& in = G.in;
    if (in.up) G.menuSel = Wrap(G.menuSel - 1, count);
    if (in.down) G.menuSel = Wrap(G.menuSel + 1, count);
    if (in.up || in.down) PlaySfx(SND_CLICK, 0.25f);
}
static void BackToMenu() {
    GameClearWorld(); G.state = GS_MENU; G.p.dead = false; G.saveReq = 0; UiOpen(UI_MAIN);
}

void UiUpdate(float) {
    Input& in = G.in;
    if (G.state == GS_LOADING) return;

    // ---------------- em jogo, sem menu ----------------
    if (G.state == GS_PLAYING && G.ui == UI_NONE) {
        if (in.diary) UiOpen(UI_DIARY); else if (in.pause || in.back) UiOpen(UI_PAUSE);
        return;
    }

    switch (G.ui) {
    case UI_MAIN: {
        MenuNav(5);
        if (in.ok) {
            switch (G.menuSel) {
            case 0: if (G.hasSaves) { RefreshSaveList(); if (!g_saveList.empty()) GameLoadFile(g_saveList[0].path); } break;
            case 1: UiOpen(UI_NEWGAME); break;
            case 2: UiOpen(UI_LOAD); break;
            case 3: UiOpen(UI_OPTIONS); break;
            case 4: G.quit = true; break;
            }
        }
        break;
    }
    case UI_NEWGAME: {
        for (size_t i = 0; i < in.chars.size(); i++) { unsigned char c = (unsigned char)in.chars[i]; if ((c >= 32 && c != 127) && G.nameBuf.size() < 20) G.nameBuf += (char)c; }
        if (in.backspace && !G.nameBuf.empty()) { do { G.nameBuf.erase(G.nameBuf.size() - 1); } while (!G.nameBuf.empty() && ((unsigned char)G.nameBuf[G.nameBuf.size() - 1] & 0xC0) == 0x80); }
        if (in.ok) GameNewGame(G.nameBuf.empty() ? "Andarilho" : G.nameBuf.c_str());
        else if (in.back) UiOpen(UI_MAIN);
        break;
    }
    case UI_PAUSE: {
        MenuNav(6);
        if (in.back || in.pause) { UiOpen(UI_NONE); break; }
        if (in.ok) {
            switch (G.menuSel) {
            case 0: UiOpen(UI_NONE); break;
            case 1: UiOpen(UI_SAVE); break;
            case 2: UiOpen(UI_LOAD); break;
            case 3: UiOpen(UI_OPTIONS); break;
            case 4: BackToMenu(); break;
            case 5: G.quit = true; break;
            }
        }
        break;
    }
    case UI_SAVE: case UI_LOAD: {
        bool isSave = G.ui == UI_SAVE; int count = (int)g_saveList.size() + (isSave ? 1 : 0);
        ListNav(count);
        if (in.back) { int back = G.state == GS_PLAYING ? UI_PAUSE : UI_MAIN; UiOpen(back); if (back == UI_PAUSE) G.menuSel = isSave ? 1 : 2; else G.menuSel = 2; break; }
        int idx = G.listSel - (isSave ? 1 : 0);
        if (in.del && idx >= 0 && idx < (int)g_saveList.size()) {
            if (!G.confirmDel) { G.confirmDel = true; Notify("Pressione DEL/X de novo para apagar este save.", 3); }
            else { DeleteSave(g_saveList[(size_t)idx].path); RefreshSaveList(); G.confirmDel = false; G.listSel = Clampi(G.listSel, 0, Maxi(0, (int)g_saveList.size() + (isSave ? 1 : 0) - 1)); Notify("Save apagado.", 0); }
        }
        if (in.ok) {
            if (isSave) { RequestSave(SK_MANUAL, idx < 0 ? std::string() : g_saveList[(size_t)idx].path); UiOpen(UI_NONE); }
            else if (idx >= 0 && idx < (int)g_saveList.size()) GameLoadFile(g_saveList[(size_t)idx].path);
        }
        break;
    }
    case UI_OPTIONS: {
        MenuNav(7); Settings& s = G.set; float dir = in.right ? 1.0f : (in.left ? -1.0f : 0.0f);
        if (in.ok && G.menuSel == 3) s.invertY = !s.invertY;
        if (in.ok && G.menuSel == 5) s.showFps = !s.showFps;
        if (dir != 0) {
            switch (G.menuSel) {
            case 0: s.sens = Clampf(s.sens + dir*0.1f, 0.2f, 4.0f); break;
            case 1: s.fov = Clampf(s.fov + dir*5.0f, 60.0f, 110.0f); break;
            case 2: s.volume = Clampf(s.volume + dir*0.1f, 0.0f, 1.0f); PlaySfx(SND_PICKUP, 0.5f); break;
            case 3: s.invertY = !s.invertY; break;
            case 4: s.viewR = Clampi(s.viewR + (int)dir, 3, 8); ApplyOptions(); break;
            case 5: s.showFps = !s.showFps; break;
            }
        }
        if (in.back || (in.ok && G.menuSel == 6)) { SaveSettings(); int back = (G.state == GS_PLAYING) ? UI_PAUSE : UI_MAIN; UiOpen(back); G.menuSel = 3; }
        break;
    }
    case UI_DIARY: {
        if (in.diary || in.back || in.pause) { UiOpen(UI_NONE); break; }
        int prevTab = G.tab;
        if (in.tabL || in.left) G.tab = Wrap(G.tab - 1, 4);
        if (in.tabR || in.right) G.tab = Wrap(G.tab + 1, 4);
        if (G.tab != prevTab) { G.listSel = 0; PlaySfx(SND_CLICK, 0.3f); }
        Player& p = G.p;
        if (G.tab == 0) {                                                       // STATUS: distribuir pontos
            ListNav(AT_COUNT);
            if (in.ok && p.points > 0 && p.attr[G.listSel] < 10) {
                p.attr[G.listSel]++; p.points--;
                if (G.listSel == AT_CON) { p.maxhp += 8.0f; p.hp += 8.0f; }
                PlaySfx(SND_LEVEL, 0.3f); Notify(std::string(ATTR_NAMES[G.listSel]) + " aumentou para " + std::to_string(p.attr[G.listSel]), 2);
            }
        } else if (G.tab == 1) {                                                // ITENS
            ListNav((int)p.inv.size());
            if (in.ok && !p.inv.empty()) UseItem(G.listSel);
            G.listSel = Clampi(G.listSel, 0, Maxi(0, (int)p.inv.size() - 1));
        } else if (G.tab == 2) { ListNav(Q_COUNT); }                            // MISSOES
        else { if (in.up) G.listSel = Clampi(G.listSel - 1, 0, 2); if (in.down) G.listSel = Clampi(G.listSel + 1, 0, 2); }   // MAPA (zoom)
        break;
    }
    case UI_DIALOG: {
        if (in.ok) { G.dlg.page++; PlaySfx(SND_CLICK, 0.25f); if (G.dlg.page >= (int)G.dlg.lines.size()) FinishDialog(); }
        else if (in.back) FinishDialog();
        break;
    }
    case UI_BARTER: {
        if (in.back) { UiOpen(UI_NONE); break; }
        if (in.left) G.shopCol = 0; if (in.right) G.shopCol = 1;
        int n = G.shopCol == 0 ? (int)G.shop.size() : (int)G.p.inv.size(); int& sel = G.shopSel[G.shopCol];
        if (in.up) sel = Wrap(sel - 1, n); if (in.down) sel = Wrap(sel + 1, n);
        if (in.ok) BarterTrade();
        break;
    }
    case UI_DEAD: {
        MenuNav(2);
        if (in.ok) { if (G.menuSel == 0) RespawnAfterDeath(); else BackToMenu(); }
        break;
    }
    default: break;
    }
}
