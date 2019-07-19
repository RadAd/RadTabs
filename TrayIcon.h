#pragma once

void TrayIconSubclass(HWND hWnd);
void TrayIconCreate(HWND hWnd, HICON hIcon, LPCTSTR szTip);
HMENU TrayIconSetMenu(HWND hWnd, HMENU hMenu);
