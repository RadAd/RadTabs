#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <Shlwapi.h>

#include "TrayIcon.h"

#define WM_TRAY_ICON (WM_USER + 5)

struct TrayIconSubclassData
{
    HMENU hMenu;
};

LRESULT CALLBACK TrayIconSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    TrayIconSubclassData* data = reinterpret_cast<TrayIconSubclassData*>(dwRefData);
    switch (uMsg)
    {
    case WM_DESTROY:
        {
            NOTIFYICONDATA nid = {};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hWnd;
            nid.uID = 1;
            Shell_NotifyIcon(NIM_DELETE, &nid);

            delete data;
            SetWindowSubclass(hWnd, TrayIconSubclassProc, uIdSubclass, (DWORD_PTR) nullptr);
        }
        break;
    case WM_TRAY_ICON:
        {
            int code = (int) LOWORD(lParam);
            int id = (int) HIWORD(lParam);
            int x = GET_X_LPARAM(wParam);
            int y = GET_Y_LPARAM(wParam);
            if (code == WM_CONTEXTMENU || code == WM_RBUTTONDOWN)
            {
                HINSTANCE hInstance = GetWindowInstance(hWnd);
                //HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAIN));

                SetForegroundWindow(hWnd);
                TrackPopupMenu(GetSubMenu(data->hMenu, 0), 0, x, y, 0, hWnd, nullptr);
            }
        }
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void TrayIconSubclass(HWND hWnd)
{
    SetWindowSubclass(hWnd, TrayIconSubclassProc, 0, (DWORD_PTR) new TrayIconSubclassData({}));
}

void TrayIconCreate(HWND hWnd, HICON hIcon, LPCTSTR szTip)
{
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.uCallbackMessage = WM_TRAY_ICON;
    nid.hIcon = hIcon != NULL ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    StrCpy(nid.szTip, szTip);
    Shell_NotifyIcon(NIM_ADD, &nid);
    Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

HMENU TrayIconSetMenu(HWND hWnd, HMENU hMenu)
{
    DWORD_PTR dwRefData = 0;
    GetWindowSubclass(hWnd, TrayIconSubclassProc, 0, &dwRefData);
    TrayIconSubclassData* data = reinterpret_cast<TrayIconSubclassData*>(dwRefData);
    HMENU hOldMenu = data->hMenu;
    data->hMenu = hMenu;
    return hOldMenu;
}
