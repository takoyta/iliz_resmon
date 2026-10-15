// Elevation helpers. Autostart (HKCU Run) moved to settings.h, used by the configurator.
#include "common.h"
#include <shellapi.h>
#include "autostart.h"

BOOL IsElevated() {
    HANDLE tk; TOKEN_ELEVATION e = {0}; DWORD n;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tk)) return FALSE;
    GetTokenInformation(tk, TokenElevation, &e, sizeof(e), &n);
    CloseHandle(tk);
    return e.TokenIsElevated != 0;
}

void RelaunchElevated() {
    WCHAR exe[MAX_PATH]; GetModuleFileNameW(0, exe, MAX_PATH);
    SHELLEXECUTEINFOW si = {sizeof(si)};
    si.fMask = SEE_MASK_FLAG_NO_UI; si.lpVerb = L"runas"; si.lpFile = exe; si.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&si);
}
