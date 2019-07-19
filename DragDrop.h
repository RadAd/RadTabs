#pragma once

#define DDN_CAN_DRAG    (WMN_FIRST + 0)
#define DDN_CAN_DROP    (WMN_FIRST + 1)
#define DDN_DO_DROP     (WMN_FIRST + 2)
#define DDN_CANCEL      (WMN_FIRST + 3)

struct RadDragDrap
{
    NMHDR hdr;
    POINT pt;
    POINT ptOffset;
    LPARAM lParam;
    HBITMAP hDragBmp;
};

void DragSubclass(HINSTANCE hInstance, HWND hWnd);
