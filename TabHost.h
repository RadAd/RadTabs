#pragma once

HWND CreateTabHost(HINSTANCE hInstance, HWND hInitWnd);

HWND Lookup(HWND hWnd);
void CleanupTabWnd();

#define WM_ADD_HWND_TAB (WM_USER + 10)
#define WM_DEL_HWND_TAB (WM_USER + 11)
#define WM_UPDATE_HWND_TAB (WM_USER + 12)
#define WM_MOVE_HWND_TAB (WM_USER + 13)

#define HANDLE_WM_ADD_HWND_TAB(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)lParam), 0L)
#define HANDLE_WM_DEL_HWND_TAB(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)lParam), 0L)
#define HANDLE_WM_MOVE_HWND_TAB(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)lParam), 0L)
#define HANDLE_WM_UPDATE_HWND_TAB(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (HWND)lParam), 0L)

enum HotKey
{
    HK_SWITCH,
    HK_PREV,
    HK_NEXT,
    HK_TAB
};

