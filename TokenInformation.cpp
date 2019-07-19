#include <Windows.h>

#include "TokenInformation.h"

LPVOID GetTokenInformation(HANDLE hToken, TOKEN_INFORMATION_CLASS eTokenClass)
{
    DWORD dwLength = 0;
    LPVOID ptu = NULL;

    if (!GetTokenInformation(
        hToken,         // handle to the access token
        eTokenClass,    // get information about the token's groups
        ptu,   // pointer to PTOKEN_USER buffer
        0,              // size of buffer
        &dwLength       // receives required buffer size
    ))
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return NULL;
    }

    ptu = (PTOKEN_USER) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwLength);

    if (!GetTokenInformation(
        hToken,         // handle to the access token
        eTokenClass,    // get information about the token's groups
        ptu,   // pointer to PTOKEN_USER buffer
        dwLength,       // size of buffer
        &dwLength       // receives required buffer size
    ))
    {
        HeapFree(GetProcessHeap(), 0, ptu);
        return NULL;
    }

    return ptu;
}

void DeleteTokenInformation(LPVOID ptu)
{
    if (ptu != nullptr)
        HeapFree(GetProcessHeap(), 0, ptu);
}

LPVOID GetTokenInformation(const DWORD procId, TOKEN_INFORMATION_CLASS eTokenClass)
{
    HANDLE hProcess = procId == 0 ? GetCurrentProcess() : OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, procId);
    if (hProcess == NULL)
        return NULL;

    HANDLE hToken = NULL;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
    {
        CloseHandle(hProcess);
        return NULL;
    }

    LPVOID ptu = GetTokenInformation(hToken, eTokenClass);

    CloseHandle(hToken);
    CloseHandle(hProcess);

    return ptu;
}

bool HasUIAccess(const DWORD procId)
{
    PDWORD pta = (PDWORD) GetTokenInformation(procId, TokenUIAccess);

    bool has = pta == nullptr || *pta != 0;

    DeleteTokenInformation(pta);

    return has;
}
