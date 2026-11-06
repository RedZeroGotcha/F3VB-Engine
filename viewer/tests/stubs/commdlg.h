#pragma once
#include "windows.h"
struct OPENFILENAMEA { DWORD lStructSize; HWND hwndOwner; HINSTANCE hInstance; LPCSTR lpstrFilter; char* lpstrCustomFilter; DWORD nMaxCustFilter, nFilterIndex;
  char* lpstrFile; DWORD nMaxFile; char* lpstrFileTitle; DWORD nMaxFileTitle; LPCSTR lpstrInitialDir, lpstrTitle; DWORD Flags; WORD nFileOffset, nFileExtension; LPCSTR lpstrDefExt; };
#define OFN_FILEMUSTEXIST 0x1000
#define OFN_PATHMUSTEXIST 0x800
#define OFN_EXPLORER 0x80000
#define OFN_NOCHANGEDIR 8
#define OFN_ALLOWMULTISELECT 0x200
BOOL GetOpenFileNameA(OPENFILENAMEA*);
