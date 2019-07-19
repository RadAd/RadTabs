#pragma once

LPVOID GetTokenInformation(const DWORD procId, TOKEN_INFORMATION_CLASS eTokenClass);

bool HasUIAccess(const DWORD procId);
