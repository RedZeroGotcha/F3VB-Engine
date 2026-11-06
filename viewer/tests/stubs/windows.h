// STUB minimo do Windows.h, SO para checar a sintaxe do viewer_main.cpp no Linux (-fsyntax-only). Nao e o header real.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
typedef void* HANDLE; typedef HANDLE HWND; typedef HANDLE HDC; typedef HANDLE HGLRC; typedef HANDLE HINSTANCE; typedef HANDLE HMENU;
typedef HANDLE HFONT; typedef HANDLE HGDIOBJ; typedef HANDLE HCURSOR; typedef HANDLE HICON; typedef HANDLE HBRUSH; typedef HANDLE HDROP;
typedef unsigned int UINT; typedef unsigned long DWORD; typedef unsigned short WORD; typedef int BOOL; typedef long LONG; typedef char* LPSTR;
typedef const char* LPCSTR; typedef std::uintptr_t UINT_PTR; typedef std::uintptr_t WPARAM; typedef std::intptr_t LPARAM; typedef std::intptr_t LRESULT;
typedef unsigned char BYTE;
#define WINAPI
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define LOWORD(l) (static_cast<WORD>(static_cast<std::uintptr_t>(l) & 0xffff))
#define HIWORD(l) (static_cast<WORD>((static_cast<std::uintptr_t>(l) >> 16) & 0xffff))
struct RECT { LONG left, top, right, bottom; };
struct POINT { LONG x, y; };
union LARGE_INTEGER { struct { DWORD LowPart; LONG HighPart; }; long long QuadPart; };
struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; };
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
struct WNDCLASSA { UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCSTR lpszMenuName, lpszClassName; };
struct PIXELFORMATDESCRIPTOR { WORD nSize, nVersion; DWORD dwFlags; BYTE iPixelType, cColorBits, cRedBits, cRedShift, cGreenBits, cGreenShift, cBlueBits, cBlueShift,
    cAlphaBits, cAlphaShift, cAccumBits, cAccumRedBits, cAccumGreenBits, cAccumBlueBits, cAccumAlphaBits, cDepthBits, cStencilBits, cAuxBuffers, iLayerType, bReserved;
    DWORD dwLayerMask, dwVisibleMask, dwDamageMask; };
#define CS_OWNDC 0x20
#define CS_HREDRAW 2
#define CS_VREDRAW 1
#define CW_USEDEFAULT (static_cast<int>(0x80000000))
#define WS_OVERLAPPEDWINDOW 0x00CF0000L
#define WS_EX_ACCEPTFILES 0x10L
#define IDC_ARROW (reinterpret_cast<LPCSTR>(32512))
#define PFD_DRAW_TO_WINDOW 4
#define PFD_SUPPORT_OPENGL 0x20
#define PFD_DOUBLEBUFFER 1
#define PFD_TYPE_RGBA 0
#define PFD_MAIN_PLANE 0
#define PM_REMOVE 1
#define WM_QUIT 0x12
#define WM_SIZE 5
#define WM_COMMAND 0x111
#define WM_DROPFILES 0x233
#define WM_LBUTTONDOWN 0x201
#define WM_LBUTTONUP 0x202
#define WM_RBUTTONDOWN 0x204
#define WM_RBUTTONUP 0x205
#define WM_MBUTTONDOWN 0x207
#define WM_MBUTTONUP 0x208
#define WM_MOUSEMOVE 0x200
#define WM_MOUSEWHEEL 0x20A
#define WM_KEYDOWN 0x100
#define WM_DESTROY 2
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_F1 0x70
#define VK_OEM_4 0xDB
#define VK_OEM_6 0xDD
#define MF_STRING 0
#define MF_POPUP 0x10
#define MF_SEPARATOR 0x800
#define MB_OK 0
#define MB_ICONERROR 0x10
#define FW_NORMAL 400
#define ANSI_CHARSET 0
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define FIXED_PITCH 1
#define FF_MODERN 0x30
typedef WORD ATOM;
ATOM RegisterClassA(const WNDCLASSA*);
HCURSOR LoadCursorA(HINSTANCE, LPCSTR);
BOOL AdjustWindowRect(RECT*, DWORD, BOOL);
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void*);
HDC GetDC(HWND);
int ReleaseDC(HWND, HDC);
int ChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR*);
BOOL SetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR*);
HGLRC wglCreateContext(HDC);
BOOL wglMakeCurrent(HDC, HGLRC);
BOOL wglDeleteContext(HGLRC);
typedef int (WINAPI *PROC)();
PROC wglGetProcAddress(const char*);
#ifndef APIENTRY
#define APIENTRY
#endif
DWORD GetTickCount();
BOOL wglUseFontBitmapsA(HDC, DWORD, DWORD, DWORD);
BOOL SwapBuffers(HDC);
HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
BOOL DeleteObject(HGDIOBJ);
BOOL ShowWindow(HWND, int);
BOOL UpdateWindow(HWND);
BOOL SetWindowTextA(HWND, LPCSTR);
BOOL DestroyWindow(HWND);
void PostQuitMessage(int);
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
BOOL PeekMessageA(MSG*, HWND, UINT, UINT, UINT);
BOOL TranslateMessage(const MSG*);
LRESULT DispatchMessageA(const MSG*);
HWND SetCapture(HWND);
BOOL ReleaseCapture();
BOOL QueryPerformanceFrequency(LARGE_INTEGER*);
BOOL QueryPerformanceCounter(LARGE_INTEGER*);
void Sleep(DWORD);
int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
HMENU CreateMenu();
HMENU CreatePopupMenu();
BOOL AppendMenuA(HMENU, UINT, UINT_PTR, LPCSTR);
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
// --- v12: APIs usadas pelo log (existem no Windows.h real)
#define VK_F2 0x71
#define VK_OEM_COMMA 0xBC
#define VK_OEM_PERIOD 0xBE
inline DWORD GetModuleFileNameA(HANDLE, char* b, DWORD n) { if (n) b[0] = 0; return 0; }
inline BOOL CreateDirectoryA(const char*, void*) { return 1; }
inline DWORD GetTempPathA(DWORD n, char* b) { if (n) b[0] = 0; return 0; }
// --- v15 (mapviewer): APIs que existem no Windows.h real
#define VK_TAB 0x09
#define VK_SHIFT 0x10
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_NUMPAD1 0x61
#define VK_NUMPAD9 0x69
#define VK_HOME 0x24
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(static_cast<long long>(-1)))
struct WIN32_FIND_DATAA { DWORD dwFileAttributes; char cFileName[260]; };
inline HANDLE FindFirstFileA(const char*, WIN32_FIND_DATAA*) { return INVALID_HANDLE_VALUE; }
inline BOOL FindNextFileA(HANDLE, WIN32_FIND_DATAA*) { return 0; }
inline BOOL FindClose(HANDLE) { return 1; }
inline DWORD GetEnvironmentVariableA(const char*, char* b, DWORD n) { if (n) b[0] = 0; return 0; }
inline short GetKeyState(int) { return 0; }
inline int _stricmp(const char* a, const char* b) { return std::strcmp(a, b); }
// --- v19 (jogo F3VB): APIs que existem no Windows.h real
typedef std::intptr_t LONG_PTR;
typedef HANDLE HMONITOR;
#define WS_POPUP 0x80000000L
#define WS_VISIBLE 0x10000000L
#define GWL_STYLE (-16)
#define HWND_TOP (reinterpret_cast<HWND>(0))
#define SWP_FRAMECHANGED 0x20
#define SWP_NOZORDER 0x4
#define MONITOR_DEFAULTTONEAREST 2
#define HTCLIENT 1
#define WM_SETCURSOR 0x20
#define WM_CHAR 0x102
#define WM_SYSKEYDOWN 0x104
#define VK_RETURN 0x0D
#define VK_BACK 0x08
#define VK_F3 0x72
#define VK_F8 0x77
#define MB_ICONWARNING 0x30
#define INVALID_FILE_ATTRIBUTES (static_cast<DWORD>(-1))
struct MONITORINFO { DWORD cbSize; RECT rcMonitor, rcWork; DWORD dwFlags; };
inline LONG_PTR GetWindowLongPtrA(HWND, int) { return 0; }
inline LONG_PTR SetWindowLongPtrA(HWND, int, LONG_PTR) { return 0; }
inline BOOL GetWindowRect(HWND, RECT*) { return 1; }
inline HMONITOR MonitorFromWindow(HWND, DWORD) { return nullptr; }
inline BOOL GetMonitorInfoA(HMONITOR, MONITORINFO*) { return 1; }
inline BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return 1; }
inline HCURSOR SetCursor(HCURSOR) { return nullptr; }
inline short GetAsyncKeyState(int) { return 0; }
inline HWND GetForegroundWindow() { return nullptr; }
inline DWORD GetFileAttributesA(const char*) { return INVALID_FILE_ATTRIBUTES; }
