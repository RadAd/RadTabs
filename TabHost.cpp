#include <Windows.h>

#include "TabHost.h"

#include <CommCtrl.h>
#include <Windowsx.h>
#include <Shlwapi.h>
#include <dwmapi.h>
#include <map>
#include <cassert>

#include "IconManager.h"
#include "DragDrop.h"
#include "Utils.h"
#include "resource.h"

// TODO
// Maybe? Use CLOAK instead of ShowWindow(hWnd, SW_HIDE);

HBITMAP Capture(HWND hWnd, RECT rcClient)
{
    HBITMAP hbmScreen = NULL;

    HDC hdcWindow = GetWindowDC(hWnd);

    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);
    if (!hdcMemDC)
        goto done;

    hbmScreen = CreateCompatibleBitmap(hdcWindow, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
    if (!hbmScreen)
        goto done;

    SelectBitmap(hdcMemDC, hbmScreen);

    if (!BitBlt(hdcMemDC,
        0, 0, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
        hdcWindow,
        0, 0,
        SRCCOPY))
        goto done;

done:
    //DeleteBitmap(hbmScreen);
    DeleteObject(hdcMemDC);
    //ReleaseDC(NULL, hdcScreen);
    ReleaseDC(hWnd, hdcWindow);

    return hbmScreen;
}

// How to hide adminstrator windows
// https://stackoverflow.com/questions/13468331/showwindow-function-doesnt-work-when-target-application-is-run-as-administrator/13473274#13473274

static const TCHAR TABS_CLASS_NAME[] = TEXT("RadTabsHostClass");

static std::map<HWND, HWND> MapWndToTabWnd;

HWND Lookup(HWND hWnd)
{
    std::map<HWND, HWND>::const_iterator it = MapWndToTabWnd.find(hWnd);
    if (it == MapWndToTabWnd.end())
        return NULL;
    else
        return it->second;
}

void CleanupTabWnd()
{
    for (std::map<HWND, HWND>::const_iterator it = MapWndToTabWnd.begin(); it != MapWndToTabWnd.end(); ++it)
    {
        ShowWindow(it->first, SW_SHOW);
    }
}

HFONT CreateFont()
{
    NONCLIENTMETRICS ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    return CreateFontIndirect(&(ncm.lfMessageFont));
}

HFONT GetFont()
{
    static HFONT hFont = CreateFont();
    return hFont;
}


LRESULT CALLBACK TabsHostWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

ATOM RegisterTabHost(HINSTANCE hInstance)
{
    WNDCLASS wc = {};

    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    wc.lpfnWndProc = TabsHostWindowProc;
    wc.hIcon = NULL;
    wc.lpszClassName = TABS_CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    return RegisterClass(&wc);
}

struct TabsCreateParam
{
    HWND hInitWnd;
};

HWND CreateTabHost(HINSTANCE hInstance, HWND hInitWnd)
{
    static ATOM atom = RegisterTabHost(hInstance);

    TabsCreateParam cp = {};
    cp.hInitWnd = hInitWnd;

    HWND hTabsWnd = CreateWindowEx(
        WS_EX_NOACTIVATE,
        MAKEINTATOM(atom),
        TEXT("Rad Tabs Host"),
        WS_POPUP | /*WS_VISIBLE |*/ WS_CLIPSIBLINGS | WS_CHILD,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,       // Parent window
        NULL,       // Menu
        hInstance,  // Instance handle
        &cp         // Additional application data
    );

    if (hTabsWnd)
        ShowWindow(hTabsWnd, SW_SHOWNOACTIVATE);

    return hTabsWnd;
}

HWND IsDropWindow(POINT pt)
{
    HWND hWndDrop = WindowFromPoint(pt);
    if (hWndDrop == NULL)
        return NULL;

    TCHAR ClassName[1024];
    GetClassName(hWndDrop, ClassName, ARRAYSIZE(ClassName));

    DWORD dwStyle = GetWindowStyle(hWndDrop);

    if (StrCmp(ClassName, WC_TABCONTROL) == 0)
    {
        hWndDrop = GetParent(hWndDrop);
        GetClassName(hWndDrop, ClassName, ARRAYSIZE(ClassName));
    }

    //DebugOut(__FUNCTION__ TEXT(", %08X %s\n"), hWndDrop, ClassName);

    if (StrCmp(ClassName, TABS_CLASS_NAME) == 0)
        return hWndDrop;
    else
        return NULL;
}

LONG GetTabHeight(HWND hWndTab)
{
    RECT r = {};
    TabCtrl_AdjustRect(hWndTab, TRUE, &r);
    return r.bottom - r.top;
}

LONG GetTabWidth(HWND hWndTab)
{
    RECT r = {};
    TabCtrl_GetItemRect(hWndTab, 1, &r);
    TabCtrl_GetItemRect(hWndTab, 0, &r);
    return TabCtrl_GetItemCount(hWndTab) * (r.right - r.left) + 2 * r.left;
}

void PositionFollow(HWND hWnd, HWND hWndTab, HWND hWndMain)
{
#if 0
    RECT r = {};
    GetWindowRect(hWndMain, &r); // TODO Use DwmGetWindowAttribute
    const int invisible_border_offset = 8; // TODO
    InflateRect(&r, -invisible_border_offset, 0);
#else
    RECT r = {};
    DwmGetWindowAttribute(hWndMain, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r));
#endif
    const int height = GetTabHeight(hWndTab);
    const int width = GetTabWidth(hWndTab);

    r.right = min(r.right, r.left + width);
#if 0
    MoveWindow(hWnd, r.left, r.top - height, r.right - r.left, height, TRUE);
#else
    SetWindowPos(hWnd, hWndMain, r.left, r.top - height, r.right - r.left, height, SWP_NOACTIVATE);
    InvalidateRect(hWnd, nullptr, TRUE);
#endif
}

HWND GetTabWnd(HWND hWndTab, int i)
{
    TCITEM tie = {};
    tie.mask = TCIF_PARAM;
    if (TabCtrl_GetItem(hWndTab, i, &tie))
        return reinterpret_cast<HWND>(tie.lParam);
    else
        return NULL;
}

int AddTab(HWND hWndTab, HWND hWnd)
{
    TCHAR title[1024];
    GetWindowText(hWnd, title, ARRAYSIZE(title));

    TCITEM tie = {};
    tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
    //tie.dwStateMask = LVIS_OVERLAYMASK;
    //tie.dwState = ILD_OVERLAYMASK | INDEXTOOVERLAYMASK(1);
    tie.iImage = GetIconIndex(hWnd);
    tie.pszText = title;
    tie.lParam = reinterpret_cast<LPARAM>(hWnd);

    int pos = TabCtrl_GetItemCount(hWndTab);
    pos = TabCtrl_InsertItem(hWndTab, pos, &tie);

    if (pos >= 0)
    {
        assert(MapWndToTabWnd.find(GetParent(hWndTab)) == MapWndToTabWnd.end());
        MapWndToTabWnd[hWnd] = GetParent(hWndTab);
    }

    int sel = TabCtrl_GetCurSel(hWndTab);
    if (sel < 0)
    {
        TabCtrl_SetCurSel(hWndTab, 0);
        sel = 0;
    }

    if (sel == pos)
    {
        SetWindowParent(GetParent(hWndTab), hWnd);
        PositionFollow(GetParent(hWndTab), hWndTab, hWnd);
    }
    else
    {
        LRESULT l = SendMessage(hWnd, WM_NULL, 0, 0);
        ShowWindow(hWnd, SW_HIDE);
        PositionFollow(GetParent(hWndTab), hWndTab, GetTabWnd(hWndTab, sel));
    }

    return pos;
}

HWND DelTab(HWND hWndTab, int i)
{
    HWND hWnd = GetTabWnd(hWndTab, i);
    int sel = TabCtrl_GetCurSel(hWndTab);
    bool selected = sel  == i;
    TabCtrl_DeleteItem(hWndTab, i);
    DelIcon(hWnd);

    assert(MapWndToTabWnd.find(hWnd)->second == GetParent(hWndTab));
    MapWndToTabWnd.erase(hWnd);

    if (TabCtrl_GetItemCount(hWndTab) <= 0)
        FORWARD_WM_CLOSE(GetParent(hWndTab), PostMessage);
    else if (selected)
    {
        TabCtrl_SetCurSel(hWndTab, 0);

        HWND hOther = GetTabWnd(hWndTab, 0);

        RECT r = {};
        if (GetWindowRect(hWnd, &r))
        {
            SetWindowParent(GetParent(hWndTab), hOther);
            MoveWindow(hOther, r.left, r.top, r.right - r.left, r.bottom - r.top, FALSE);
            ShowWindow(hOther, SW_SHOW);
            SetForegroundWindow(hOther);
            //PositionFollow(GetParent(hWndTab), hWndTab, hOther);
        }
        else
        {
            // TODO This is the case when the hWnd is closed
            // Maybe I should move all windows in tab to keep them in sync for this case
            ShowWindow(hOther, SW_SHOW);
            SetForegroundWindow(hOther);
            //PositionFollow(GetParent(hWndTab), hWndTab, hOther);
        }
        PositionFollow(GetParent(hWndTab), hWndTab, GetTabWnd(hWndTab, TabCtrl_GetCurSel(hWndTab)));
    }
    else
        PositionFollow(GetParent(hWndTab), hWndTab, GetTabWnd(hWndTab, sel));

    return hWnd;
}

int FindTab(HWND hWndTab, HWND hFind)
{
    int count = TabCtrl_GetItemCount(hWndTab);
    for (int i = 0; i < count; ++i)
    {
        HWND hWnd = GetTabWnd(hWndTab, i);
        if (hWnd == hFind)
            return i;
    }
    return -1;
}

HWND TabHostGetUserData(HWND hWnd)
{
    return (HWND)GetWindowLongPtr(hWnd, GWLP_USERDATA);
}

LRESULT CALLBACK TagSubclassProc(HWND hWndTab, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        {
            int i = TabCtrl_GetCurSel(hWndTab);
            if (i >= 0)
            {
                HWND hOther = GetTabWnd(hWndTab, i);
                HWND hFG = GetForegroundWindow();

                if (hFG != hOther)
                {
                    SetForegroundWindow(hOther);
                }
            }
        }
        break;
    }
    return DefSubclassProc(hWndTab, uMsg, wParam, lParam);
}

BOOL TabsHostOnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    TabsCreateParam* cp = (TabsCreateParam*)lpCreateStruct->lpCreateParams;

    HINSTANCE hInstance = NULL;
    HWND hWndTab = CreateWindowEx(WS_EX_NOACTIVATE, WC_TABCONTROL, NULL,
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | TCS_TABS | TCS_FIXEDWIDTH | TCS_FORCELABELLEFT | TCS_FOCUSNEVER | TCS_TOOLTIPS,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWnd, NULL, hInstance, NULL);
    SetWindowSubclass(hWndTab, TagSubclassProc, 0, 0);
    DragSubclass(hInstance, hWndTab);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(hWndTab));

#if 0
    HWND hWndTooltip = CreateWindowEx(0, TOOLTIPS_CLASS, NULL,
        WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWnd, NULL, hInstance, NULL);
    TabCtrl_SetToolTips(hWndTab, hWndTooltip);
#endif

    SetWindowFont(hWndTab, GetFont(), TRUE);
    TabCtrl_SetImageList(hWndTab, GetImageList());

    AddTab(hWndTab, cp->hInitWnd);

    return TRUE; // FORWARD_WM_CREATE(hWnd, lpCreateStruct, DefWindowProc);
}

int TabsHostOnMouseActivate(HWND hWnd, HWND hwndTopLevel, UINT codeHitTest, UINT msg)
{
    return MA_NOACTIVATE;
}

// TODO Shouldn't be getting WM_ACTIVATEAPP
// I have been able to stop it when clicking on the outer window (using WS_EX_NOACTIVATE and WS_CHILD)
// But it still occurs when clicking on the child tab ctrl.
// New info: doesn't occur clicking on the child tab ctrl - not sure what stopped it
// Does get called for all tab host when set foreground window to main window
void TabsHostOnActivateApp(HWND hWnd, BOOL fActivate, DWORD dwThreadId)
{
    if (fActivate)
    {
        const HWND hWndTab = TabHostGetUserData(hWnd);

        int i = TabCtrl_GetCurSel(hWndTab);
        if (i >= 0)
        {
            HWND hOther = GetTabWnd(hWndTab, i);
            // TODO Note sure what is going on
            HWND hFG = GetForegroundWindow();

            if (hFG != hOther)
            {
                SetForegroundWindow(hOther);
            }
        }
    }
}

void TabsHostOnSize(HWND hWnd, UINT state, int cx, int cy)
{
    const HWND hWndTab = TabHostGetUserData(hWnd);
    MoveWindow(hWndTab, 0, 0, cx, cy, FALSE);
}

void TabsHostOnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    HBRUSH hBrush = (HBRUSH) GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND);
    FillRect(hdc, &ps.rcPaint, hBrush);
    EndPaint(hWnd, &ps);
}

LRESULT TabsHostOnNotify(HWND hWnd, int id, LPNMHDR nm)
{
    const HWND hWndTab = TabHostGetUserData(hWnd);
    static int LastSelection = -1;  // TODO Store in GWLP_USERDATA? Shouldnt be necessary as TCN_SELCHANGING/TCN_SELCHANGE should come as a pair

    switch (nm->code)
    {
    case TCN_SELCHANGING:
        {
            LastSelection = TabCtrl_GetCurSel(hWndTab);
            return FALSE;
        }
        break;
    case TCN_SELCHANGE:
        {
            int i = TabCtrl_GetCurSel(hWndTab);
            if (i >= 0 && i != LastSelection)
            {
                HWND hOther = GetTabWnd(hWndTab, i);
                HWND hLast = GetTabWnd(hWndTab, LastSelection);

                RECT r = {};
                GetWindowRect(hLast, &r);

                SetWindowParent(hWnd, hOther);
                MoveWindow(hOther, r.left, r.top, r.right - r.left, r.bottom - r.top, FALSE);
                ShowWindow(hOther, SW_SHOW);
                SetForegroundWindow(hOther);

                ShowWindow(hLast, SW_HIDE);
            }
            LastSelection = -1;
        }
        break;
    case TTN_GETDISPINFO:
        {
            LPNMTTDISPINFO lpnmtdi = (LPNMTTDISPINFO) nm;
            static TCHAR text[1024];
#if 0
            TCITEM tie = {};
            tie.mask = TCIF_TEXT;
            tie.pszText = text;
            tie.cchTextMax = ARRAYSIZE(text);
            TabCtrl_GetItem(hWndTab, lpnmtdi->hdr.idFrom, &tie);
#else
            HWND hWndOther = GetTabWnd(hWndTab, (int) lpnmtdi->hdr.idFrom);
            GetWindowText(hWndOther, text, ARRAYSIZE(text));
#endif
            lpnmtdi->lpszText = text;
        }
        break;
    case DDN_CAN_DRAG:
        {
            RadDragDrap* rdd = reinterpret_cast<RadDragDrap*>(nm);
            TCHITTESTINFO ht = {};
            ht.pt = rdd->pt;
            ScreenToClient(rdd->hdr.hwndFrom, &ht.pt);
            int i = TabCtrl_HitTest(rdd->hdr.hwndFrom, &ht);
            if (i < 0 || ht.flags == TCHT_NOWHERE)
                return FALSE;
            else
            {
                HINSTANCE hInstance = GetWindowInstance(hWnd);

                rdd->lParam = i + 1;
                HWND hOther = GetTabWnd(hWndTab, i);
                RECT r;
                TabCtrl_GetItemRect(hWndTab, i, &r);
                ClientToScreen(hWndTab, reinterpret_cast<POINT*>(&r.left));
                ClientToScreen(hWndTab, reinterpret_cast<POINT*>(&r.right));
                rdd->hDragBmp = Capture(hWndTab, r);
                return TRUE;
            }
        }
        break;
    case DDN_CAN_DROP:
        {
            RadDragDrap* rdd = reinterpret_cast<RadDragDrap*>(nm);
            HWND hWndDrop = IsDropWindow(rdd->pt);
            //return hWndDrop != NULL;
            return TRUE;
        }
        break;
    case DDN_DO_DROP:
        {
            RadDragDrap* rdd = reinterpret_cast<RadDragDrap*>(nm);
            HWND hWndDrop = IsDropWindow(rdd->pt);
            int i = static_cast<int>(rdd->lParam) - 1;

            if (hWndDrop == hWnd)
            {
                // Drag on itself
            }
            else if (hWndDrop != NULL)
            {
                HWND hWndAdd = DelTab(hWndTab, i);
                if (hWndAdd != NULL)
                    SendMessage(hWndDrop, WM_ADD_HWND_TAB, 0, (LPARAM) hWndAdd);
            }
            else
            {
                HINSTANCE hInstance = GetWindowInstance(hWnd);

                RECT rTabItem = {};
                TabCtrl_GetItemRect(hWndTab, i, &rTabItem);

                HWND hWndAdd = DelTab(hWndTab, i);

                RECT rTab = {};
                GetWindowRect(hWnd, &rTab);

                RECT r = {};
                GetWindowRect(hWndAdd, &r);

                MoveWindow(hWndAdd, rdd->pt.x - rdd->ptOffset.x + rTabItem.left, rdd->pt.y - rdd->ptOffset.y + rTabItem.top + rTab.bottom - rTab.top, r.right - r.left, r.bottom - r.top, FALSE);
                ShowWindow(hWndAdd, SW_SHOW);
                SetForegroundWindow(hWndAdd);
                CreateTabHost(hInstance, hWndAdd);
            }
            DeleteBitmap(rdd->hDragBmp);

            return hWndDrop != NULL;
        }
        break;
    case DDN_CANCEL:
        {
            RadDragDrap* rdd = reinterpret_cast<RadDragDrap*>(nm);
            DeleteBitmap(rdd->hDragBmp);
        }
        break;
    }
    return 0;
}

void TabHostOnParentNotify(HWND hWnd, UINT msg, HWND hwndChild, int idChild)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
        {
            const HWND hWndTab = TabHostGetUserData(hWnd);
            int i = TabCtrl_GetCurSel(hWndTab);
            if (i >= 0)
            {
                HWND hOther = GetTabWnd(hWndTab, i);
                HWND hFG = GetForegroundWindow();

                if (hFG != hOther)
                {
                    SetForegroundWindow(hOther);
                }
            }
        }
        break;
    }
}

void TabHostOnAddTab(HWND hWnd, HWND hOther)
{
    const HWND hWndTab = TabHostGetUserData(hWnd);
    AddTab(hWndTab, hOther);
}

void TabHostOnDelTab(HWND hWnd, HWND hOther)
{
    const HWND hWndTab = TabHostGetUserData(hWnd);
    int i = FindTab(hWndTab, hOther);
    if (i >= 0)
        DelTab(hWndTab, i);
}

void TabHostOnMoveTab(HWND hWnd, HWND hOther)
{
    const HWND hWndTab = TabHostGetUserData(hWnd);
    int i = TabCtrl_GetCurSel(hWndTab);
    if (i >= 0)
    {
        HWND hWndSel = GetTabWnd(hWndTab, i);

        if (hOther == hWndSel)
        {
            PositionFollow(hWnd, hWndTab, hOther);
        }
    }
}

void TabHostOnUpdateTab(HWND hWnd, HWND hOther)
{
    const HWND hWndTab = TabHostGetUserData(hWnd);
    int i = FindTab(hWndTab, hOther);

    if (i >= 0)
    {
        TCHAR title[1024];
        GetWindowText(hOther, title, ARRAYSIZE(title));

        Sleep(100); // No event for icon change but it usually follows a title change

        TCITEM tie = {};
        tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
        tie.iImage = GetIconIndex(hOther);
        tie.pszText = title;
        tie.lParam = reinterpret_cast<LPARAM>(hOther);
        TabCtrl_SetItem(hWndTab, i, &tie);
    }
}

void TabSwitchTab(HWND hWndTab, int i)
{
    NMHDR nm = {};
    nm.hwndFrom = hWndTab;
    nm.idFrom = GetWindowLongPtr(hWndTab, GWLP_ID);
    nm.code = TCN_SELCHANGING;
    FORWARD_WM_NOTIFY(GetParent(hWndTab), nm.idFrom, &nm, SendMessage);

    TabCtrl_SetCurSel(hWndTab, i);

    nm.code = TCN_SELCHANGE;
    FORWARD_WM_NOTIFY(GetParent(hWndTab), nm.idFrom, &nm, SendMessage);
}

void TabsHostOnHotKey(HWND hWnd, int idHotKey, UINT fuModifiers, UINT vk)
{
    const HWND hWndTab = TabHostGetUserData(hWnd);
    HWND hOther = GetForegroundWindow();
    switch (LOWORD(idHotKey))
    {
    case HK_SWITCH:
    {
        int i = HIWORD(idHotKey) - 1;
        int count = TabCtrl_GetItemCount(hWndTab);
        if (i >= 0 && i < count)
            TabSwitchTab(hWndTab, i);
    }
    break;
    case HK_PREV:
    {
        int i = TabCtrl_GetCurSel(hWndTab);
        int count = TabCtrl_GetItemCount(hWndTab);
        if (i >= 0 && count > 1)
            TabSwitchTab(hWndTab, (i + count - 1) % count);
    }
    break;
    case HK_NEXT:
    {
        int i = TabCtrl_GetCurSel(hWndTab);
        int count = TabCtrl_GetItemCount(hWndTab);
        if (i >= 0 && count > 1)
            TabSwitchTab(hWndTab, (i + 1) % count);
    }
    break;
    case HK_TAB:
    {
        int i = TabCtrl_GetCurSel(hWndTab);
        if (i >= 0)
            DelTab(hWndTab, i);
    }
    break;
    }
}

LRESULT CALLBACK TabsHostWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hWnd, WM_CREATE, TabsHostOnCreate);
        HANDLE_MSG(hWnd, WM_MOUSEACTIVATE, TabsHostOnMouseActivate);
        //HANDLE_MSG(hWnd, WM_ACTIVATEAPP, TabsHostOnActivateApp);
        HANDLE_MSG(hWnd, WM_SIZE, TabsHostOnSize);
        //HANDLE_MSG(hWnd, WM_PAINT, TabsHostOnPaint);
        HANDLE_MSG(hWnd, WM_NOTIFY, TabsHostOnNotify);
        //HANDLE_MSG(hWnd, WM_PARENTNOTIFY, TabHostOnParentNotify);
        HANDLE_MSG(hWnd, WM_HOTKEY, TabsHostOnHotKey);
        HANDLE_MSG(hWnd, WM_ADD_HWND_TAB, TabHostOnAddTab);
        HANDLE_MSG(hWnd, WM_DEL_HWND_TAB, TabHostOnDelTab);
        HANDLE_MSG(hWnd, WM_MOVE_HWND_TAB, TabHostOnMoveTab);
        HANDLE_MSG(hWnd, WM_UPDATE_HWND_TAB, TabHostOnUpdateTab);
    default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}
