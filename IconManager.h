#pragma once

#include <Windows.h>
#include <CommCtrl.h>

HIMAGELIST GetImageList();
int GetIconIndex(HWND hWnd);
void DelIcon(HWND hWnd);

