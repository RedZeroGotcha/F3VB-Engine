// STUB minimo do Windows para checar a compilacao da camada Win32/OpenGL no Linux. NAO usado no build real.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
typedef unsigned long DWORD; typedef int BOOL; typedef unsigned short WORD; typedef unsigned char BYTE; typedef void* HANDLE; typedef void* HWND; typedef void* HDC; typedef void* HGLRC;
typedef void* HINSTANCE; typedef void* HMODULE; typedef void* HFONT; typedef void* HGDIOBJ; typedef void* HMONITOR; typedef void* LPVOID; typedef const char* LPCSTR; typedef char* LPSTR; typedef short SHORT;
typedef uintptr_t WPARAM; typedef intptr_t LPARAM; typedef intptr_t LRESULT; typedef unsigned int UINT; typedef long LONG; typedef void* HWAVEOUT; typedef unsigned int MMRESULT;
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
#define TRUE 1
#define FALSE 0
#define WINAPI
#define CALLBACK
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFF
#define ERROR_SUCCESS 0
#define LOWORD(l) ((WORD)((l) & 0xffff))
#define HIWORD(l) ((WORD)(((l) >> 16) & 0xffff))
#define GET_WHEEL_DELTA_WPARAM(w) ((short)HIWORD(w))
struct RECT { LONG left, top, right, bottom; }; struct POINT { LONG x, y; }; struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; };
struct WINDOWPLACEMENT { UINT length; };
struct MONITORINFO { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; };
struct WNDCLASSA { UINT style; LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM); int cbClsExtra, cbWndExtra; HINSTANCE hInstance; void* hIcon; void* hCursor; void* hbrBackground; LPCSTR lpszMenuName; LPCSTR lpszClassName; };
struct PIXELFORMATDESCRIPTOR { WORD nSize, nVersion; DWORD dwFlags; BYTE iPixelType, cColorBits, cRedBits, cRedShift, cGreenBits, cGreenShift, cBlueBits, cBlueShift, cAlphaBits, cAlphaShift, cAccumBits, cAccumRedBits, cAccumGreenBits, cAccumBlueBits, cAccumAlphaBits, cDepthBits, cStencilBits, cAuxBuffers, iLayerType, bReserved; DWORD dwLayerMask, dwVisibleMask, dwDamageMask; };
struct LARGE_INTEGER { long long QuadPart; };
struct SYSTEM_INFO { DWORD dwNumberOfProcessors; };
struct WAVEFORMATEX { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec; WORD nBlockAlign, wBitsPerSample, cbSize; };
struct WAVEHDR { LPSTR lpData; DWORD dwBufferLength, dwBytesRecorded, dwUser, dwFlags, dwLoops; WAVEHDR* lpNext; DWORD reserved; };
struct CRITICAL_SECTION { int x; }; struct CONDITION_VARIABLE { int x; };
struct WIN32_FIND_DATAA { DWORD dwFileAttributes; char cFileName[260]; };
#define WHDR_DONE 1
#define WAVE_FORMAT_PCM 1
#define WAVE_MAPPER ((UINT)-1)
#define CALLBACK_NULL 0
#define MMSYSERR_NOERROR 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define PFD_DRAW_TO_WINDOW 4
#define PFD_SUPPORT_OPENGL 32
#define PFD_DOUBLEBUFFER 1
#define PFD_TYPE_RGBA 0
#define PFD_MAIN_PLANE 0
#define WS_OVERLAPPEDWINDOW 0xCF0000
#define WS_VISIBLE 0x10000000
#define CW_USEDEFAULT ((int)0x80000000)
#define CS_OWNDC 0x20
#define CS_HREDRAW 2
#define CS_VREDRAW 1
#define IDC_ARROW 0
#define GWL_STYLE (-16)
#define SWP_NOOWNERZORDER 0x200
#define SWP_FRAMECHANGED 0x20
#define SWP_NOMOVE 2
#define SWP_NOSIZE 1
#define SWP_NOZORDER 4
#define HWND_TOP ((HWND)0)
#define MONITOR_DEFAULTTOPRIMARY 1
#define PM_REMOVE 1
#define WM_QUIT 0x12
#define WM_SIZE 5
#define WM_SETFOCUS 7
#define WM_KILLFOCUS 8
#define WM_CLOSE 0x10
#define WM_DESTROY 2
#define WM_CHAR 0x102
#define WM_MOUSEWHEEL 0x20A
#define WM_SYSCOMMAND 0x112
#define WM_ERASEBKGND 0x14
#define SC_KEYMENU 0xF100
#define SC_SCREENSAVE 0xF140
#define SC_MONITORPOWER 0xF170
#define MB_ICONERROR 0x10
#define FW_BOLD 700
#define ANSI_CHARSET 0
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define ANTIALIASED_QUALITY 4
#define DEFAULT_PITCH 0
#define FF_SWISS 32
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 8
#define FILE_ATTRIBUTE_DIRECTORY 16
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define VK_LBUTTON 1
#define VK_RBUTTON 2
#define VK_BACK 8
#define VK_TAB 9
#define VK_RETURN 13
#define VK_SHIFT 16
#define VK_CONTROL 17
#define VK_ESCAPE 27
#define VK_SPACE 32
#define VK_LEFT 37
#define VK_UP 38
#define VK_RIGHT 39
#define VK_DOWN 40
#define VK_DELETE 46
#define VK_F1 0x70
#define VK_F3 0x72
#define VK_F5 0x74
#define VK_F9 0x78
#define VK_F11 0x7A
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define GetTickCount() 0UL
inline void Sleep(DWORD) {} inline void timeBeginPeriod(UINT) {} inline void timeEndPeriod(UINT) {}
inline void InitializeCriticalSection(CRITICAL_SECTION*) {} inline void DeleteCriticalSection(CRITICAL_SECTION*) {} inline void EnterCriticalSection(CRITICAL_SECTION*) {} inline void LeaveCriticalSection(CRITICAL_SECTION*) {}
inline void InitializeConditionVariable(CONDITION_VARIABLE*) {} inline BOOL SleepConditionVariableCS(CONDITION_VARIABLE*, CRITICAL_SECTION*, DWORD) { return 1; } inline void WakeConditionVariable(CONDITION_VARIABLE*) {} inline void WakeAllConditionVariable(CONDITION_VARIABLE*) {}
inline HANDLE CreateThread(void*, size_t, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD*) { return 0; } inline DWORD WaitForSingleObject(HANDLE, DWORD) { return 0; } inline BOOL CloseHandle(HANDLE) { return 1; } inline BOOL SetThreadPriority(HANDLE, int) { return 1; }
inline void GetSystemInfo(SYSTEM_INFO* s) { s->dwNumberOfProcessors = 4; }
inline DWORD GetModuleFileNameA(HMODULE, char* b, DWORD) { strcpy(b, "./rpggl.exe"); return 11; } inline BOOL CreateDirectoryA(const char*, void*) { return 1; } inline BOOL MoveFileExA(const char*, const char*, DWORD) { return 1; }
inline HANDLE FindFirstFileA(const char*, WIN32_FIND_DATAA*) { return INVALID_HANDLE_VALUE; } inline BOOL FindNextFileA(HANDLE, WIN32_FIND_DATAA*) { return 0; } inline BOOL FindClose(HANDLE) { return 1; }
inline HMODULE LoadLibraryA(const char*) { return 0; } inline void* GetProcAddress(HMODULE, const char*) { return 0; }
inline short GetAsyncKeyState(int) { return 0; } inline BOOL GetCursorPos(POINT*) { return 1; } inline BOOL SetCursorPos(int, int) { return 1; } inline int ShowCursor(BOOL) { return 0; } inline BOOL ClipCursor(const RECT*) { return 1; }
inline BOOL GetClientRect(HWND, RECT*) { return 1; } inline BOOL ClientToScreen(HWND, POINT*) { return 1; }
inline LONG GetWindowLong(HWND, int) { return 0; } inline LONG SetWindowLong(HWND, int, LONG) { return 0; } inline BOOL GetWindowPlacement(HWND, WINDOWPLACEMENT*) { return 1; } inline BOOL SetWindowPlacement(HWND, const WINDOWPLACEMENT*) { return 1; }
inline HMONITOR MonitorFromWindow(HWND, DWORD) { return 0; } inline BOOL GetMonitorInfo(HMONITOR, MONITORINFO*) { return 1; } inline BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT) { return 1; }
inline void* LoadCursor(void*, int) { return 0; } inline int RegisterClassA(const WNDCLASSA*) { return 1; } inline BOOL AdjustWindowRect(RECT*, DWORD, BOOL) { return 1; }
inline HWND CreateWindowA(LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, void*, HINSTANCE, void*) { return (HWND)1; }
inline HDC GetDC(HWND) { return (HDC)1; } inline int ReleaseDC(HWND, HDC) { return 1; } inline BOOL DestroyWindow(HWND) { return 1; } inline int ChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR*) { return 1; } inline BOOL SetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR*) { return 1; } inline BOOL SwapBuffers(HDC) { return 1; }
inline HGLRC wglCreateContext(HDC) { return (HGLRC)1; } inline BOOL wglMakeCurrent(HDC, HGLRC) { return 1; } inline BOOL wglDeleteContext(HGLRC) { return 1; } inline void* wglGetProcAddress(const char*) { return 0; } inline BOOL wglUseFontBitmapsA(HDC, DWORD, DWORD, DWORD) { return 1; }
inline BOOL PeekMessage(MSG*, HWND, UINT, UINT, UINT) { return 0; } inline BOOL TranslateMessage(const MSG*) { return 1; } inline LRESULT DispatchMessage(const MSG*) { return 0; } inline void PostQuitMessage(int) {} inline LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline int MessageBoxA(HWND, const char*, const char*, UINT) { return 1; }
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* f) { f->QuadPart = 1000000; return 1; } inline BOOL QueryPerformanceCounter(LARGE_INTEGER* c) { c->QuadPart = 0; return 1; }
inline HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char*) { return (HFONT)1; } inline HGDIOBJ SelectObject(HDC, HGDIOBJ o) { return o; } inline BOOL DeleteObject(HGDIOBJ) { return 1; } inline BOOL GetCharWidth32A(HDC, UINT, UINT, int* w) { for (int i = 0; i < 256; i++) w[i] = 8; return 1; }
inline MMRESULT waveOutOpen(HWAVEOUT*, UINT, const WAVEFORMATEX*, DWORD, DWORD, DWORD) { return 1; } inline MMRESULT waveOutPrepareHeader(HWAVEOUT, WAVEHDR*, UINT) { return 0; } inline MMRESULT waveOutWrite(HWAVEOUT, WAVEHDR*, UINT) { return 0; } inline MMRESULT waveOutReset(HWAVEOUT) { return 0; } inline MMRESULT waveOutUnprepareHeader(HWAVEOUT, WAVEHDR*, UINT) { return 0; } inline MMRESULT waveOutClose(HWAVEOUT) { return 0; }
#define WINAPI_WinMain
