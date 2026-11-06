// =====================================================================
//  platform.cpp - camada Windows: janela, contexto OpenGL, entrada
//  (teclado + mouse + XInput carregado dinamicamente) e laco principal.
//
//  Laco: le entrada -> GameFrame(dt) -> RenderFrame -> (miniatura do
//  save, se houver pedido) -> HUD -> troca de buffers -> limpa eventos.
// =====================================================================
#include "gfx.h"
#include <xinput.h>

typedef BOOL (WINAPI *PFNSWAPINTERVAL)(int);
typedef DWORD (WINAPI *PFNXINPUTGETSTATE)(DWORD, XINPUT_STATE*);

static HWND  g_hwnd; static HDC g_hdc; static HGLRC g_rc;
static int   g_W = 1280, g_H = 720;
static bool  g_focused = true, g_quit = false, g_fullscreen = false, g_captured = false, g_cursorHidden = false;
static WINDOWPLACEMENT g_placement = { sizeof(WINDOWPLACEMENT) };
static PFNXINPUTGETSTATE g_xinput = 0;
static std::string g_chars; static bool g_backspace = false; static float g_wheel = 0;

// ---------------------------------------------------------------------
//  ENTRADA
// ---------------------------------------------------------------------
static void LoadXInput() {
    const char* names[3] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (int i = 0; i < 3 && !g_xinput; i++) { HMODULE m = LoadLibraryA(names[i]); if (m) g_xinput = (PFNXINPUTGETSTATE)GetProcAddress(m, "XInputGetState"); }
}
struct PadState { bool ok; float lx, ly, rx, ry, lt, rt; WORD btn; };
static void Stick(SHORT x, SHORT y, float dz, float& ox, float& oy) {
    float fx = (float)x/32767.0f, fy = (float)y/32767.0f, m = sqrtf(fx*fx + fy*fy);
    if (m < dz) { ox = oy = 0; return; }
    float k = Minf(1.0f, (m - dz)/(1.0f - dz)); ox = fx/m*k; oy = fy/m*k;
}
static PadState ReadPad() {
    PadState r; memset(&r, 0, sizeof(r)); if (!g_xinput) return r;
    XINPUT_STATE st; memset(&st, 0, sizeof(st)); if (g_xinput(0, &st) != ERROR_SUCCESS) return r;
    r.ok = true; const XINPUT_GAMEPAD& p = st.Gamepad;
    Stick(p.sThumbLX, p.sThumbLY, 0.22f, r.lx, r.ly); Stick(p.sThumbRX, p.sThumbRY, 0.20f, r.rx, r.ry);
    r.lt = (float)p.bLeftTrigger/255.0f; r.rt = (float)p.bRightTrigger/255.0f; r.btn = p.wButtons;
    return r;
}

static void SetFullscreen(bool on) {
    if (on == g_fullscreen) return; DWORD style = GetWindowLong(g_hwnd, GWL_STYLE);
    if (on) {
        MONITORINFO mi = { sizeof(mi) }; GetWindowPlacement(g_hwnd, &g_placement); GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        SetWindowLong(g_hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLong(g_hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW); SetWindowPlacement(g_hwnd, &g_placement);
        SetWindowPos(g_hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    g_fullscreen = on;
}

static bool Rep(int id, bool now, float dt) {                                  // auto-repeticao para navegar em menus
    static bool prev[16]; static float t[16]; bool r = false;
    if (now) { if (!prev[id]) { r = true; t[id] = 0.38f; } else { t[id] -= dt; if (t[id] <= 0) { r = true; t[id] = 0.07f; } } }
    prev[id] = now; return r;
}

static void SetMouseCapture(bool want) {
    if (want == g_captured) return;
    g_captured = want;
    if (want) {
        RECT r; GetClientRect(g_hwnd, &r); POINT c = { (r.right - r.left)/2, (r.bottom - r.top)/2 }; ClientToScreen(g_hwnd, &c); SetCursorPos(c.x, c.y);
        RECT cr; GetClientRect(g_hwnd, &cr); POINT tl = { cr.left, cr.top }, br = { cr.right, cr.bottom }; ClientToScreen(g_hwnd, &tl); ClientToScreen(g_hwnd, &br);
        RECT clip = { tl.x, tl.y, br.x, br.y }; ClipCursor(&clip);
        if (!g_cursorHidden) { ShowCursor(FALSE); g_cursorHidden = true; }
    } else { ClipCursor(NULL); if (g_cursorHidden) { ShowCursor(TRUE); g_cursorHidden = false; } }
}

static void PollInput(float dt) {
    static bool prevKey[256]; static WORD prevBtn = 0; static bool wasCaptured = false;
    bool cur[256]; for (int i = 0; i < 256; i++) cur[i] = g_focused && (GetAsyncKeyState(i) & 0x8000) != 0;
    #define KD(vk) (cur[(vk)])
    #define KE(vk) (cur[(vk)] && !prevKey[(vk)])
    PadState pad = ReadPad(); WORD btn = pad.ok ? pad.btn : 0;
    #define PD(m) ((btn & (m)) != 0)
    #define PE(m) (((btn & (m)) != 0) && ((prevBtn & (m)) == 0))
    Input& in = G.in;

    // ---- teclas de sistema ----
    if (KE(VK_F11)) SetFullscreen(!g_fullscreen);
    if (KE(VK_F1)) g_showHelp = !g_showHelp;
    in.debug = KE(VK_F3);

    // ---- mouse ----
    bool wantCap = g_focused && G.state == GS_PLAYING && G.ui == UI_NONE; SetMouseCapture(wantCap);
    float mdx = 0, mdy = 0;
    if (g_captured) {
        RECT r; GetClientRect(g_hwnd, &r); POINT c = { (r.right - r.left)/2, (r.bottom - r.top)/2 }; ClientToScreen(g_hwnd, &c); POINT p; GetCursorPos(&p);
        if (wasCaptured) { mdx = (float)(p.x - c.x); mdy = (float)(p.y - c.y); }
        SetCursorPos(c.x, c.y);
    }
    wasCaptured = g_captured;
    in.lookDX = mdx*0.0022f; in.lookDY = -mdy*0.0022f;

    // ---- movimento / olhar ----
    float mx = 0, my = 0;
    if (KD('A')) mx -= 1; if (KD('D')) mx += 1; if (KD('W')) my += 1; if (KD('S')) my -= 1;
    mx += pad.lx; my += pad.ly; if (G.drive) my += pad.rt - pad.lt; in.moveX = Clampf(mx, -1, 1); in.moveY = Clampf(my, -1, 1);
    in.padLookX = pad.rx*(0.4f + 0.6f*fabsf(pad.rx)); in.padLookY = pad.ry*(0.4f + 0.6f*fabsf(pad.ry));

    // ---- acoes (seguradas) ----
    in.fire = !G.drive && (KD(VK_LBUTTON) || pad.rt > 0.5f); in.aim = !G.drive && (KD(VK_RBUTTON) || pad.lt > 0.5f); in.brake = KD(VK_SPACE) || PD(XINPUT_GAMEPAD_B); in.sprint = KD(VK_SHIFT) || PD(XINPUT_GAMEPAD_LEFT_THUMB);
    // ---- acoes (eventos) ----
    bool typing = (G.ui == UI_NEWGAME);
    in.jump = KE(VK_SPACE) || PE(XINPUT_GAMEPAD_B); in.interact = KE('E') || PE(XINPUT_GAMEPAD_A); in.reload = KE('R') || PE(XINPUT_GAMEPAD_X);
    in.diary = KE(VK_TAB) || (KE('I') && !typing) || PE(XINPUT_GAMEPAD_Y); in.quicksave = KE(VK_F5) || PE(XINPUT_GAMEPAD_DPAD_LEFT); in.quickload = KE(VK_F9);
    in.camToggle = KE('V') || PE(XINPUT_GAMEPAD_RIGHT_THUMB); in.bash = KE('F') || PE(XINPUT_GAMEPAD_DPAD_RIGHT); in.heal = KE('H') || PE(XINPUT_GAMEPAD_DPAD_UP);
    in.sneakToggle = KE('C') || KE(VK_LCONTROL) || PE(XINPUT_GAMEPAD_DPAD_DOWN); in.pause = KE(VK_ESCAPE) || PE(XINPUT_GAMEPAD_START);
    in.nextWeapon = PE(XINPUT_GAMEPAD_RIGHT_SHOULDER); in.prevWeapon = PE(XINPUT_GAMEPAD_LEFT_SHOULDER);
    in.selectWeapon = 0; for (int i = 0; i < 4; i++) if (KE('1' + i)) in.selectWeapon = i + 1;
    in.wheel = g_wheel; g_wheel = 0;

    // ---- menus (com auto-repeticao) ----
    bool padUp = pad.ly > 0.6f || PD(XINPUT_GAMEPAD_DPAD_UP), padDn = pad.ly < -0.6f || PD(XINPUT_GAMEPAD_DPAD_DOWN);
    bool padLf = pad.lx < -0.6f || PD(XINPUT_GAMEPAD_DPAD_LEFT), padRt = pad.lx > 0.6f || PD(XINPUT_GAMEPAD_DPAD_RIGHT);
    in.up = Rep(0, KD(VK_UP) || (!typing && KD('W')) || padUp, dt) || in.wheel > 0.5f;
    in.down = Rep(1, KD(VK_DOWN) || (!typing && KD('S')) || padDn, dt) || in.wheel < -0.5f;
    in.left = Rep(2, KD(VK_LEFT) || (!typing && KD('A')) || padLf, dt); in.right = Rep(3, KD(VK_RIGHT) || (!typing && KD('D')) || padRt, dt);
    in.ok = KE(VK_RETURN) || PE(XINPUT_GAMEPAD_A) || (!typing && (KE('E') || KE(VK_SPACE))) || (G.ui == UI_DIALOG && KE(VK_LBUTTON));
    in.back = KE(VK_ESCAPE) || PE(XINPUT_GAMEPAD_B); in.del = KE(VK_DELETE) || PE(XINPUT_GAMEPAD_X);
    in.tabL = (!typing && KE('Q')) || PE(XINPUT_GAMEPAD_LEFT_SHOULDER); in.tabR = (!typing && KE('R')) || PE(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    if (G.ui == UI_DIARY && PE(XINPUT_GAMEPAD_Y)) in.diary = true;
    if (typing) { in.chars = g_chars; in.backspace = g_backspace; } else { in.chars.clear(); in.backspace = false; }
    g_chars.clear(); g_backspace = false;

    // ---- teclado x controle (para os textos de ajuda) ----
    bool padActive = pad.ok && (fabsf(pad.lx) > 0.3f || fabsf(pad.ly) > 0.3f || fabsf(pad.rx) > 0.3f || btn != 0 || pad.rt > 0.3f || pad.lt > 0.3f);
    bool kbActive = fabsf(mdx) + fabsf(mdy) > 2.0f; for (int i = 8; i < 256 && !kbActive; i++) if (cur[i] && i != VK_SHIFT && i != VK_LSHIFT && i != VK_RSHIFT) kbActive = true;
    if (padActive) in.usingPad = true; else if (kbActive) in.usingPad = false;

    memcpy(prevKey, cur, sizeof(cur)); prevBtn = btn;
    #undef KD
    #undef KE
    #undef PD
    #undef PE
}

// ---------------------------------------------------------------------
//  JANELA / OPENGL
// ---------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_SIZE: g_W = LOWORD(l); g_H = HIWORD(l); return 0;
    case WM_SETFOCUS: g_focused = true; return 0;
    case WM_KILLFOCUS: g_focused = false; SetMouseCapture(false); if (G.state == GS_PLAYING && G.ui == UI_NONE) UiOpen(UI_PAUSE); return 0;
    case WM_CLOSE: g_quit = true; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_CHAR: {
        unsigned c = (unsigned)w;
        if (c == 8) g_backspace = true;
        else if (c >= 32 && c != 127) { if (c < 128) g_chars += (char)c; else if (c < 256) { g_chars += (char)(0xC0 | (c >> 6)); g_chars += (char)(0x80 | (c & 0x3F)); } }
        return 0;
    }
    case WM_MOUSEWHEEL: g_wheel += (float)GET_WHEEL_DELTA_WPARAM(w)/120.0f; return 0;
    case WM_SYSCOMMAND: if ((w & 0xFFF0) == SC_KEYMENU || (w & 0xFFF0) == SC_SCREENSAVE || (w & 0xFFF0) == SC_MONITORPOWER) return 0; break;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProc(h, msg, w, l);
}

static bool CreateGLWindow(HINSTANCE inst) {
    WNDCLASSA wc; memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW; wc.lpfnWndProc = WndProc; wc.hInstance = inst; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.lpszClassName = "RPGGLClass";
    if (!RegisterClassA(&wc)) return false;
    RECT r = { 0, 0, g_W, g_H }; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA("RPGGLClass", "RPGGL  -  build de teste 0.2", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    if (!g_hwnd) return false;
    g_hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd; memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER; pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.iLayerType = PFD_MAIN_PLANE;
    int fmt = ChoosePixelFormat(g_hdc, &pfd); if (!fmt || !SetPixelFormat(g_hdc, fmt, &pfd)) return false;
    g_rc = wglCreateContext(g_hdc); if (!g_rc || !wglMakeCurrent(g_hdc, g_rc)) return false;
    PFNSWAPINTERVAL swap = (PFNSWAPINTERVAL)wglGetProcAddress("wglSwapIntervalEXT"); if (swap) swap(1);
    RECT cr; GetClientRect(g_hwnd, &cr); g_W = cr.right; g_H = cr.bottom;
    return true;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    timeBeginPeriod(1);
    if (!CreateGLWindow(inst)) { MessageBoxA(NULL, "Nao foi possivel criar a janela OpenGL.", "RPGGL - Erro", MB_ICONERROR); return 1; }
    LoadXInput(); InitRender(); InitHUD(g_hdc, g_H); InitAudio(); GameInit();
    int hudH = g_H;
    LARGE_INTEGER freq, last, now; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&last);
    double fpsAcc = 0; int fpsFrames = 0; MSG msg;
    while (!g_quit && !G.quit) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) g_quit = true; TranslateMessage(&msg); DispatchMessage(&msg); }
        QueryPerformanceCounter(&now); double dt = (double)(now.QuadPart - last.QuadPart)/(double)freq.QuadPart; last = now; if (dt > 0.1) dt = 0.1;
        PollInput((float)dt);
        GameFrame((float)dt);
        if (g_W > 0 && g_H > 0) {
            if (abs(g_H - hudH) > hudH/8) { InitHUD(g_hdc, g_H); hudH = g_H; }
            RenderFrame(g_W, g_H);
            if (G.saveReq) { std::vector<u8> th; int tw = 0, thh = 0; if (!CaptureThumb(g_W, g_H, th, tw, thh)) { th.clear(); tw = thh = 0; } GameCommitSave(th, tw, thh); }
            DrawHUD(g_W, g_H); SwapBuffers(g_hdc);
        } else Sleep(20);
        GameClearEdges();
        fpsAcc += dt; fpsFrames++; if (fpsAcc >= 0.5) { G.fps = (float)(fpsFrames/fpsAcc); fpsAcc = 0; fpsFrames = 0; }
    }
    SetMouseCapture(false);
    GameShutdown(); ShutdownAudio(); ShutdownHUD();
    wglMakeCurrent(NULL, NULL); wglDeleteContext(g_rc); ReleaseDC(g_hwnd, g_hdc); DestroyWindow(g_hwnd);
    timeEndPeriod(1);
    return 0;
}
