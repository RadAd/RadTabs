#pragma once

#define WM_WIN_EVENT (WM_USER + 2)

HWINEVENTHOOK RegisterWinEvent(_In_ HWND hWnd, _In_ DWORD eventMin, _In_ DWORD eventMax);
