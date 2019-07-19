#pragma once

#include <cstdio>
#include <Shlwapi.h>

inline void DebugOut(const wchar_t* format, ...)
{
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, 1024, format, args);
    OutputDebugStringW(buffer);
    va_end(args);
}

inline BOOL StrEmpty(LPCTSTR str)
{
    return str[0] == TEXT('\0');
}

inline bool StrMatch(PCWSTR format, PCWSTR actual)
{
    return format == nullptr
        || StrEmpty(format)
        || StrCmp(format, TEXT("*")) == 0
        || StrCmp(actual, format) == 0;
    // TODO Use PathMatchSpec
}

inline HICON GetWindowIcon(HWND hWnd)
{
    HICON hIcon = NULL;
    if (hIcon == NULL)
        hIcon = (HICON) SendMessage(hWnd, WM_GETICON, ICON_SMALL, 0);
    if (hIcon == NULL)
        hIcon = (HICON) GetClassLongPtr(hWnd, GCLP_HICONSM);
    if (hIcon == NULL)
        hIcon = (HICON) GetClassLongPtr(hWnd, GCLP_HICON);
    return hIcon;
}

inline HWND SetWindowParent(HWND hWnd, HWND hParent)
{
    return reinterpret_cast<HWND>(SetWindowLongPtr(hWnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(hParent)));
}
