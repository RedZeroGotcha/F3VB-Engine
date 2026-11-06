#pragma once
#include "windows.h"
UINT DragQueryFileA(HDROP, UINT, char*, UINT);
void DragFinish(HDROP);
void DragAcceptFiles(HWND, BOOL);
