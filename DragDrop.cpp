#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>

#include "DragDrop.h"

HWND CreateDragWindow(HINSTANCE hInstance);
void SetDragWindowBitmap(HWND hDragWnd, HBITMAP hBitmap, POINT pt);

struct DragSubclassData
{
    HINSTANCE hInstance;
    bool dragging;
    POINT ptOffset;
    LPARAM lParam;
    HWND hDragWnd;
    HBITMAP hDragBmp;
};

LRESULT DragNotifyParent(UINT code, HWND hWnd, POINT pt, DragSubclassData* pData)
{
    RadDragDrap nm = {};
    nm.hdr.hwndFrom = hWnd;
    nm.hdr.idFrom = GetWindowLong(hWnd, GWL_ID);
    nm.hdr.code = code;
    nm.pt = pt;
    ClientToScreen(hWnd, &nm.pt);
    nm.ptOffset = pData->ptOffset;
    nm.lParam = pData->lParam;
    nm.hDragBmp = pData->hDragBmp;
    LRESULT r = FORWARD_WM_NOTIFY(GetParent(hWnd), nm.hdr.idFrom, &nm, SendMessage);
    pData->lParam = nm.lParam;
    pData->hDragBmp = nm.hDragBmp;
    return r;
}

// http://www.suodenjoki.dk/us/productions/articles/dragdroptab.htm
// Drag Drop
LRESULT CALLBACK DragSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    DragSubclassData* data = reinterpret_cast<DragSubclassData*>(dwRefData);
    switch (uMsg)
    {
    case WM_DESTROY:
        delete data;
        SetWindowSubclass(hWnd, DragSubclassProc, uIdSubclass, (DWORD_PTR) nullptr);
        break;
    case WM_LBUTTONDOWN:
        {
            const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (!data->dragging)
            {
                data->ptOffset = pt;
                BOOL bCan = (BOOL) DragNotifyParent(DDN_CAN_DRAG, hWnd, pt, data);
                if (bCan && DragDetect(hWnd, pt))
                {
                    data->dragging = true;
                    SetCapture(hWnd);
                    SetFocus(hWnd);

                    if (data->hDragBmp != NULL)
                    {
                        POINT ptDt = pt;
                        ClientToScreen(hWnd, &ptDt);
                        data->hDragWnd = CreateDragWindow(data->hInstance);
                        SetDragWindowBitmap(data->hDragWnd, data->hDragBmp, ptDt);
                    }
                    return 0;
                }
            }
        }
        break;
    case WM_LBUTTONUP:
        {
            const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (data->dragging)
            {
                DragNotifyParent(DDN_DO_DROP, hWnd, pt, data);
                ReleaseCapture();
            }
        }
        break;
    case WM_CANCELMODE:
        {
            if (data->dragging)
            {
                DragNotifyParent(DDN_CANCEL, hWnd, POINT {}, data);
                ReleaseCapture();
            }
        }
        break;
    case WM_CAPTURECHANGED:
        {
            if (data->dragging)
            {
                data->dragging = false;
                if (data->hDragWnd != NULL)
                {
                    FORWARD_WM_CLOSE(data->hDragWnd, PostMessage);
                    data->hDragWnd = NULL;
                }
                data->lParam = 0;
                data->hDragBmp = NULL;
            }
        }
        break;
    case WM_MOUSEMOVE:
        {
            const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (data->dragging)
            {
                if (DragNotifyParent(DDN_CAN_DROP, hWnd, pt, data))
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                else
                    SetCursor(LoadCursor(NULL, IDC_NO));

                if (data->hDragWnd != NULL)
                {
                    POINT ptDt = pt;
                    ClientToScreen(hWnd, &ptDt);
                    SetDragWindowBitmap(data->hDragWnd, data->hDragBmp, ptDt);
                }
            }
        }
        break;
    case WM_CHAR:
        {
            //DebugOut(TEXT("WM_CHAR, %02X\n"), wParam);
            if (wParam == VK_ESCAPE && data->dragging)
            {
                DragNotifyParent(DDN_CANCEL, hWnd, POINT {}, data);
                ReleaseCapture();
            }
        }
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void DragSubclass(HINSTANCE hInstance, HWND hWnd)
{
    SetWindowSubclass(hWnd, DragSubclassProc, 0, (DWORD_PTR) new DragSubclassData({ hInstance }));
}

LRESULT CALLBACK DragWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_NCHITTEST: return HTTRANSPARENT;
    default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

ATOM RegisterDragWindow(HINSTANCE hInstance)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = DragWindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = NULL; // LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_DRAGICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = TEXT("RadDrag");
    return RegisterClass(&wc);
}

HWND CreateDragWindow(HINSTANCE hInstance)
{
    static ATOM atom = RegisterDragWindow(hInstance);
    HWND hDragWnd = CreateWindowEx(WS_EX_LAYERED, MAKEINTATOM(atom), NULL,
        WS_POPUP /*| WS_VISIBLE*/,
        0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (hDragWnd)
        ShowWindow(hDragWnd, SW_SHOWNOACTIVATE);
    return hDragWnd;
}

void SetDragWindowBitmap(HWND hDragWnd, HBITMAP hBitmap, POINT pt)
{
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    SIZE sz = { bm.bmWidth, bm.bmHeight };

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmpOld = SelectBitmap(hdcMem, hBitmap);

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 128;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT ptZero = {};

    pt.x -= sz.cx / 2;
    pt.y -= sz.cy / 2;

    UpdateLayeredWindow(hDragWnd, hdcScreen, &pt, &sz,
        hdcMem, &ptZero, RGB(0, 0, 0), &blend, ULW_ALPHA);

    SelectBitmap(hdcMem, hbmpOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}
