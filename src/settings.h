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

// One row per ini key: Load/Save/defaults are driven by this table. max == 1: a 0/1 flag (any non-zero = 1).
typedef struct { const WCHAR *key; int off, def, min, max; } SettingDef;
static const SettingDef settingDefs[] = {
    {L"cpu",  FIELD_OFFSET(Settings, cpu),  1, 0, 1},
    {L"mem",  FIELD_OFFSET(Settings, mem),  1, 0, 1},
    {L"disk", FIELD_OFFSET(Settings, disk), 1, 0, 1},
    {L"net",  FIELD_OFFSET(Settings, net),  1, 0, 1},
    {L"gpu",  FIELD_OFFSET(Settings, gpu),  0, 0, 1},
    {L"interval_ms", FIELD_OFFSET(Settings, interval), 1000, 500, 2000},
    {L"top_n", FIELD_OFFSET(Settings, topn), 10, 3, 15},
};
#define SETTINGS_N ((int)(sizeof(settingDefs) / sizeof(settingDefs[0])))

static inline int SettingsInterval(int v) { return v < 750 ? 500 : v < 1500 ? 1000 : 2000; }   // snap to 0.5 / 1 / 2 s

static inline int *SettingsField(Settings *s, int i) { return (int*)((BYTE*)s + settingDefs[i].off); }

static inline int SettingsClamp(int i, int v) {
    const SettingDef *d = &settingDefs[i];
    if (d->max == 1) return v != 0;
    v = v < d->min ? d->min : v > d->max ? d->max : v;
    return d->off == FIELD_OFFSET(Settings, interval) ? SettingsInterval(v) : v;
}

static inline void SettingsDefaults(Settings *s) { for (int i = 0; i < SETTINGS_N; i++) *SettingsField(s, i) = settingDefs[i].def; }

static inline void SettingsDir(WCHAR *p) {          // folder of the running exe, with trailing backslash
    DWORD n = GetModuleFileNameW(0, p, MAX_PATH);
    while (n && p[n - 1] != L'\\') n--;
    p[n] = 0;
}

static inline void SettingsPath(WCHAR *p) { SettingsDir(p); lstrcatW(p, L"iliz_resmon.ini"); }

static inline BOOL SettingsParse(const WCHAR *t, int *v) {   // optional spaces and sign, digits, spaces; else FALSE
    while (*t == ' ' || *t == '\t') t++;
    int neg = *t == '-'; if (*t == '-' || *t == '+') t++;
    if (*t < '0' || *t > '9') return FALSE;
    int x = 0;
    for (; *t >= '0' && *t <= '9'; t++) if (x < 100000000) x = x * 10 + (*t - '0');
    while (*t == ' ' || *t == '\t') t++;
    if (*t) return FALSE;
    *v = neg ? -x : x;
    return TRUE;
}

static inline void LoadSettings(Settings *s) {      // missing or non-numeric value: default; out of range: clamped
    WCHAR p[MAX_PATH + 16]; SettingsPath(p);
    for (int i = 0; i < SETTINGS_N; i++) {
        WCHAR t[24]; int v;
        GetPrivateProfileStringW(L"tray", settingDefs[i].key, L"", t, 24, p);
        *SettingsField(s, i) = SettingsParse(t, &v) ? SettingsClamp(i, v) : settingDefs[i].def;
    }
}

static inline BOOL SaveSettings(const Settings *s) {
    WCHAR p[MAX_PATH + 16]; SettingsPath(p);
    BOOL ok = TRUE;
    for (int i = 0; i < SETTINGS_N; i++) {
        WCHAR t[12]; wsprintfW(t, L"%d", SettingsClamp(i, *SettingsField((Settings*)s, i)));
        ok &= WritePrivateProfileStringW(L"tray", settingDefs[i].key, t, p);
    }
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
