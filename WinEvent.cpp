#include <windows.h>
#include "WinEvent.h"

HWND g_hWndMain = NULL;

static void CALLBACK WinEventProc(
    HWINEVENTHOOK hook,
    DWORD event,
    HWND hWnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD time)
{
    if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
    {
        PostMessage(g_hWndMain, WM_WIN_EVENT, event, (LPARAM)hWnd);
    }
}

HWINEVENTHOOK RegisterWinEvent(_In_ HWND hWnd, _In_ DWORD eventMin, _In_ DWORD eventMax)
{
    g_hWndMain = hWnd; // TODO Allow more than one?
    HWINEVENTHOOK hook = SetWinEventHook(
        eventMin,
        eventMax,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT);
    return hook;
}
