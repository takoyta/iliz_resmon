// User settings in iliz_resmon.ini next to the exe. Shared by iliz_resmon.exe and iliz_resmon_config.exe.
// [tray] cpu/mem/disk/net = 1/0; a missing file or key means "on".
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define SETTINGS_RELOAD_MSG L"iliz_resmon_reload"   // registered message: "reread the ini"
#define SETTINGS_WND_CLASS  L"iliz_resmon"          // main window class (APP_ID)

typedef struct { int cpu, mem, disk, net; } Settings;

static inline void SettingsDir(WCHAR *p) {          // folder of the running exe, with trailing backslash
    DWORD n = GetModuleFileNameW(0, p, MAX_PATH);
    while (n && p[n - 1] != L'\\') n--;
    p[n] = 0;
}

static inline void SettingsPath(WCHAR *p) { SettingsDir(p); lstrcatW(p, L"iliz_resmon.ini"); }

static inline void LoadSettings(Settings *s) {
    WCHAR p[MAX_PATH + 16]; SettingsPath(p);
    s->cpu  = GetPrivateProfileIntW(L"tray", L"cpu",  1, p) != 0;
    s->mem  = GetPrivateProfileIntW(L"tray", L"mem",  1, p) != 0;
    s->disk = GetPrivateProfileIntW(L"tray", L"disk", 1, p) != 0;
    s->net  = GetPrivateProfileIntW(L"tray", L"net",  1, p) != 0;
}

static inline BOOL SaveSettings(const Settings *s) {
    WCHAR p[MAX_PATH + 16]; SettingsPath(p);
    BOOL ok = TRUE;
    ok &= WritePrivateProfileStringW(L"tray", L"cpu",  s->cpu  ? L"1" : L"0", p);
    ok &= WritePrivateProfileStringW(L"tray", L"mem",  s->mem  ? L"1" : L"0", p);
    ok &= WritePrivateProfileStringW(L"tray", L"disk", s->disk ? L"1" : L"0", p);
    ok &= WritePrivateProfileStringW(L"tray", L"net",  s->net  ? L"1" : L"0", p);
    WritePrivateProfileStringW(0, 0, 0, p);         // flush the profile cache
    return ok;
}
