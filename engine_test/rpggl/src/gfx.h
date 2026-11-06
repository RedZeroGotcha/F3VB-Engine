// =====================================================================
//  gfx.h - interface dos modulos que dependem de Windows / OpenGL
//  (render.cpp, hud.cpp, audio.cpp, platform.cpp).  A logica do jogo
//  (world/game/save/ui) NAO inclui este arquivo.
// =====================================================================
#pragma once
#include "game.h"
#include <windows.h>
#include <mmsystem.h>      // waveOut*, timeBeginPeriod (excluido por WIN32_LEAN_AND_MEAN)
#include <GL/gl.h>
#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

// render.cpp - mundo 3D
void InitRender();
void RenderFrame(int W, int H);
bool CaptureThumb(int W, int H, std::vector<u8>& out, int& tw, int& th);

// hud.cpp - interface 2D
void InitHUD(HDC hdc, int H);
void ShutdownHUD();
void DrawHUD(int W, int H);
extern bool g_showHelp;

// audio.cpp
void InitAudio();
void ShutdownAudio();
