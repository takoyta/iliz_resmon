// User settings in iliz_resmon.ini next to the exe. Shared by iliz_resmon.exe and iliz_resmon_config.exe.
// [tray] cpu/mem/disk/net = 1/0 (missing = on), gpu = 1/0 (missing = off), interval_ms = 500/1000/2000 (default 1000),
// top_n = process rows in the popups, 3..15 (default 10).
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define SETTINGS_RELOAD_MSG L"iliz_resmon_reload"   // registered message: "reread the ini"
#define SETTINGS_WND_CLASS  L"iliz_resmon"          // main window class (APP_ID)

typedef struct { int cpu, mem, disk, net, interval, gpu, topn; } Settings;   // interval: tick period, ms

static inline int SettingsTopN(int v) { return v < 3 ? 3 : v > 15 ? 15 : v; }
static inline int SettingsInterval(int v) { return v < 750 ? 500 : v < 1500 ? 1000 : 2000; }   // snap to 0.5 / 1 / 2 s

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
    s->gpu  = GetPrivateProfileIntW(L"tray", L"gpu",  0, p) != 0;
    s->interval = SettingsInterval(GetPrivateProfileIntW(L"tray", L"interval_ms", 1000, p));
    s->topn = SettingsTopN(GetPrivateProfileIntW(L"tray", L"top_n", 10, p));
}

static inline BOOL SaveSettings(const Settings *s) {
    WCHAR p[MAX_PATH + 16]; SettingsPath(p);
    BOOL ok = TRUE;
    ok &= WritePrivateProfileStringW(L"tray", L"cpu",  s->cpu  ? L"1" : L"0", p);
    ok &= WritePrivateProfileStringW(L"tray", L"mem",  s->mem  ? L"1" : L"0", p);
    ok &= WritePrivateProfileStringW(L"tray", L"disk", s->disk ? L"1" : L"0", p);
    ok &= WritePrivateProfileStringW(L"tray", L"net",  s->net  ? L"1" : L"0", p);
    ok &= WritePrivateProfileStringW(L"tray", L"gpu",  s->gpu  ? L"1" : L"0", p);
    WCHAR iv[12]; wsprintfW(iv, L"%d", SettingsInterval(s->interval));
    ok &= WritePrivateProfileStringW(L"tray", L"interval_ms", iv, p);
    wsprintfW(iv, L"%d", SettingsTopN(s->topn));
    ok &= WritePrivateProfileStringW(L"tray", L"top_n", iv, p);
    WritePrivateProfileStringW(0, 0, 0, p);         // flush the profile cache
    return ok;
}

// Autostart: HKCU\...\Run value "iliz_resmon" = quoted path of iliz_resmon.exe in this exe's folder (no admin needed).
#define SETTINGS_RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define SETTINGS_RUN_VAL L"iliz_resmon"

static inline BOOL AutostartEnabled() {
    return RegGetValueW(HKEY_CURRENT_USER, SETTINGS_RUN_KEY, SETTINGS_RUN_VAL, RRF_RT_REG_SZ, 0, 0, 0) == ERROR_SUCCESS;
}

static inline BOOL AutostartSet(BOOL on) {
    if (!on) {
        LONG r = RegDeleteKeyValueW(HKEY_CURRENT_USER, SETTINGS_RUN_KEY, SETTINGS_RUN_VAL);
        return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
    }
    WCHAR v[MAX_PATH + 32];
    v[0] = L'"'; SettingsDir(v + 1); lstrcatW(v, L"iliz_resmon.exe\"");
    return RegSetKeyValueW(HKEY_CURRENT_USER, SETTINGS_RUN_KEY, SETTINGS_RUN_VAL, REG_SZ, v,
                           (lstrlenW(v) + 1) * sizeof(WCHAR)) == ERROR_SUCCESS;
}
