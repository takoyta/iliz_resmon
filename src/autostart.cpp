// Elevation helpers and autostart via HKCU Run key (no admin rights needed).
#include "common.h"
#include <shellapi.h>
#include "autostart.h"

#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_VAL L"CpuTray"

BOOL IsElevated() {
    HANDLE tk; TOKEN_ELEVATION e = {0}; DWORD n;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tk)) return FALSE;
    GetTokenInformation(tk, TokenElevation, &e, sizeof(e), &n);
    CloseHandle(tk);
    return e.TokenIsElevated != 0;
}

BOOL AutostartEnabled() {
    return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VAL, RRF_RT_REG_SZ, 0, 0, 0) == ERROR_SUCCESS;
}

void AutostartSet(BOOL on) {
    if (!on) { RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VAL); return; }
    WCHAR v[MAX_PATH + 2];
    DWORD n = GetModuleFileNameW(0, v + 1, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    v[0] = '"'; v[n + 1] = '"'; v[n + 2] = 0;
    RegSetKeyValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VAL, REG_SZ, v, (n + 3) * sizeof(WCHAR));
}

void RelaunchElevated() {
    WCHAR exe[MAX_PATH]; GetModuleFileNameW(0, exe, MAX_PATH);
    SHELLEXECUTEINFOW si = {sizeof(si)};
    si.fMask = SEE_MASK_FLAG_NO_UI; si.lpVerb = L"runas"; si.lpFile = exe; si.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&si);
}
