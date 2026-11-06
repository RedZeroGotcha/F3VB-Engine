// STUB minimo do mmsystem.h (winmm), so para checar sintaxe no Linux. Nao e o header real.
#pragma once
#define SND_FILENAME 0x00020000L
#define SND_ASYNC 0x0001
#define SND_NODEFAULT 0x0002
typedef unsigned int MCIERROR;
inline BOOL PlaySoundA(const char*, HANDLE, DWORD) { return 1; }
inline MCIERROR mciSendStringA(const char*, char*, UINT, HWND) { return 0; }
