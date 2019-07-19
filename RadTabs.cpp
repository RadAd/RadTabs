#include <Windows.h>
#include <Windowsx.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include <tchar.h>

#include "WinEvent.h"
#include "TabHost.h"
#include "DragDrop.h"
#include "TrayIcon.h"
#include "TokenInformation.h"
#include "Utils.h"
#include "resource.h"

#include <cstdio>
#include <cassert>

// TODO Make single instance

bool CheckUIAccess(const DWORD procId)
{
    static bool thishas = HasUIAccess((DWORD) 0);

    bool has = HasUIAccess(procId);

    return thishas || !has;
}

HWND CreateMainWnd(HINSTANCE hInstance);

struct WndMatch
{
    PCWSTR Title;
    PCWSTR Class;
};

TCHAR WndTypeStr[1024] = {};
WndMatch WndTypes[100] = {};

bool WndTypeMatch(HWND hWnd)
{
    TCHAR Title[1024];
    GetWindowText(hWnd, Title, ARRAYSIZE(Title));
    TCHAR ClassName[1024];
    GetClassName(hWnd, ClassName, ARRAYSIZE(ClassName));

    for (int i = 0; WndTypes[i].Title != nullptr || WndTypes[i].Class != nullptr; ++i)
    {
        if (StrMatch(WndTypes[i].Title, Title) && StrMatch(WndTypes[i].Class, ClassName))
            return true;
    }
    return false;
}

bool ShouldAddTab(HWND hWnd)
{
    DWORD dwProcId = 0;
    GetWindowThreadProcessId(hWnd, &dwProcId);

    return IsWindowVisible(hWnd) && WndTypeMatch(hWnd) && CheckUIAccess(dwProcId);
}

BOOL CALLBACK InitTabWindows(HWND hWnd, LPARAM lParam)
{
    HINSTANCE hInstance = reinterpret_cast<HINSTANCE>(lParam);

    if (ShouldAddTab(hWnd))
        CreateTabHost(hInstance, hWnd);

    return TRUE;
}

void LoadSettings()
{
    HKEY hKey;
    if (RegOpenKey(HKEY_CURRENT_USER, TEXT("Software\\RadSoft\\RadTabs"), &hKey) == ERROR_SUCCESS)
    {
        DWORD type;
        DWORD size = ARRAYSIZE(WndTypeStr) * sizeof(TCHAR);
        RegQueryValueEx(hKey, TEXT("WndTypes"), 0, &type, (LPBYTE) WndTypeStr, &size);
        RegCloseKey(hKey);
    }

    if (StrEmpty(WndTypeStr))
    {
        StrCpy(WndTypeStr,
            TEXT("*|ConsoleWindowClass\0")
            TEXT("*|Notepad\0"));
    }

    int i = 0;
    for (PTSTR p = WndTypeStr; *p != TEXT('\0'); ++p)
    {
        WndMatch& wm = WndTypes[i++];
        wm.Title = p;
        for (; *p != TEXT('\0'); ++p)
        {
            if (*p == TEXT('|'))
            {
                *p = TEXT('\0');
                wm.Class = p + 1;
            }
        }
    }
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR pCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES; // | ICC_STANDARD_CLASSES;
    BOOL ret = InitCommonControlsEx(&icex);

    LoadSettings();

    HWND hWndMain = CreateMainWnd(hInstance);

    if (hWndMain == NULL)
    {
        // TODO Error
        return EXIT_FAILURE;
    }

    HWINEVENTHOOK hooks[] = {
        RegisterWinEvent(hWndMain, EVENT_OBJECT_CREATE, EVENT_OBJECT_END),
        RegisterWinEvent(hWndMain, EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND)
    };

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    for (HWINEVENTHOOK hook : hooks)
        UnhookWinEvent(hook);
    return EXIT_SUCCESS;
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

ATOM RegisterMainClass(HINSTANCE hInstance)
{
    WNDCLASS wc = {};

    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    wc.lpfnWndProc = MainWindowProc;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpszClassName = TEXT("RadTabsMainClass");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    return RegisterClass(&wc);
}

HWND CreateMainWnd(HINSTANCE hInstance)
{
    static ATOM atom = RegisterMainClass(hInstance);

    HWND hWndMain = CreateWindowEx(
        0,
        MAKEINTATOM(atom),
        TEXT("Rad Tabs"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    return hWndMain;
}

BOOL MainWindowOnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    HINSTANCE hInstance = GetWindowInstance(hWnd);

    TrayIconSubclass(hWnd);
    TrayIconCreate(hWnd, GetWindowIcon(hWnd), TEXT("RadTabs"));
    TrayIconSetMenu(hWnd, LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MAIN)));

    RegisterHotKey(hWnd, HK_TAB, MOD_CONTROL | MOD_ALT, TEXT('T'));

    EnumWindows(InitTabWindows, reinterpret_cast<LPARAM>(hInstance));

    return TRUE;// FORWARD_WM_CREATE(hWnd, lpCreateStruct, DefWindowProc);
}

void MainWindowOnDestroy(HWND hWnd)
{
    CleanupTabWnd();

    PostQuitMessage(0);
}

void MainWindowOnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    HBRUSH hBrush = (HBRUSH) GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND);
    FillRect(hdc, &ps.rcPaint, hBrush);
    EndPaint(hWnd, &ps);
}

void MainWindowOnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_MAIN_EXIT:
        FORWARD_WM_CLOSE(hWnd, SendMessage);
        break;
    }
}

#define HANDLE_WM_WIN_EVENT(hwnd, wParam, lParam, fn) \
    ((fn)((hwnd), (DWORD)wParam, (HWND)lParam), 0L)
void MainWindowOnWinEvent(HWND hWnd, DWORD event, HWND hOther)
{
    //if (Lookup(hOther) != NULL)
        //DebugOut(__FUNCTION__ TEXT(" %08x %04x\n"), hOther, event);
    switch (event)
    {
    case EVENT_OBJECT_CREATE:
    case EVENT_OBJECT_SHOW:
    {
        HWND hTabsWnd = Lookup(hOther);
        if (ShouldAddTab(hOther) && hTabsWnd == NULL)
        {
            HINSTANCE hInstance = GetWindowInstance(hWnd);
            CreateTabHost(hInstance, hOther);
        }
    }
    break;
    case EVENT_OBJECT_DESTROY:
    {
        HWND hTabsWnd = Lookup(hOther);
        if (hTabsWnd != NULL)
            PostMessage(hTabsWnd, WM_DEL_HWND_TAB, 0, (LPARAM)hOther);
    }
    break;
    case EVENT_OBJECT_LOCATIONCHANGE:
    {
        // TODO Only move if visible and tab is the active one
        HWND hTabsWnd = Lookup(hOther);
        if (hTabsWnd != NULL)
            PostMessage(hTabsWnd, WM_MOVE_HWND_TAB, 0, (LPARAM)hOther);
    }
    break;
    case EVENT_OBJECT_NAMECHANGE:
    {
        HWND hTabsWnd = Lookup(hOther);
        if (hTabsWnd != NULL)
            PostMessage(hTabsWnd, WM_UPDATE_HWND_TAB, 0, (LPARAM)hOther);
    }
    break;
    case EVENT_SYSTEM_FOREGROUND:
    {
        HWND hTabsWnd = Lookup(hOther);
        if (hTabsWnd == NULL)
        {
            for (int i = 1; i <= 9; i++)
                UnregisterHotKey(hWnd, MAKELONG(HK_SWITCH, i));
            UnregisterHotKey(hWnd, HK_PREV);
            UnregisterHotKey(hWnd, HK_NEXT);
        }
        else
        {
            // TODO Make this configurable
            for (int i = 1; i <= 9; i++)
                RegisterHotKey(hWnd, MAKELONG(HK_SWITCH, i), MOD_CONTROL | MOD_ALT, TEXT('0') + i);
            RegisterHotKey(hWnd, HK_PREV, MOD_CONTROL | MOD_ALT, VK_LEFT);
            RegisterHotKey(hWnd, HK_NEXT, MOD_CONTROL | MOD_ALT, VK_RIGHT);
            RegisterHotKey(hWnd, HK_PREV, MOD_CONTROL | MOD_SHIFT, VK_TAB);
            RegisterHotKey(hWnd, HK_NEXT, MOD_CONTROL, VK_TAB);
        }
    }
    break;
    }
}

void MainWindowOnHotKey(HWND hWnd, int idHotKey, UINT fuModifiers, UINT vk)
{
    HWND hOther = GetForegroundWindow();
    HWND hTabsWnd = Lookup(hOther);
    if (hTabsWnd != NULL)
        FORWARD_WM_HOTKEY(hTabsWnd, idHotKey, fuModifiers, vk, PostMessage);
    else if (idHotKey == HK_TAB)
    {
        if (IsWindowVisible(hOther))
            CreateTabHost(GetWindowInstance(hWnd), hOther);
    }
}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hWnd, WM_CREATE, MainWindowOnCreate);
        HANDLE_MSG(hWnd, WM_DESTROY, MainWindowOnDestroy);
        //HANDLE_MSG(hWnd, WM_PAINT, MainWindowOnPaint);
        HANDLE_MSG(hWnd, WM_COMMAND, MainWindowOnCommand);
        HANDLE_MSG(hWnd, WM_HOTKEY, MainWindowOnHotKey);
        HANDLE_MSG(hWnd, WM_WIN_EVENT, MainWindowOnWinEvent);
    default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}
