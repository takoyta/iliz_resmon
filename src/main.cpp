// iliz resmon: CPU / memory / disk / network load in the tray. Pure WinAPI, no third-party libraries.
// main: windows, message loop, menu, single instance + elevated takeover.
#include "common.h"
#include "lang.h"
#include "appinfo.h"
#include <shellapi.h>
#include "app.h"
#include "etw.h"
#include "procs.h"
#include "trayicon.h"
#include "popup.h"
#include "histwnd.h"
#include "autostart.h"
#include "iliz_updater_client.h"
#include "settings.h"

enum { IDM_ADMIN = 1, IDM_CONFIG, IDM_UPDATE, IDM_ABOUT, IDM_EXIT };
enum { WM_APP_UPDATE = WM_APP + 7, TIMER_TICK = 1, TIMER_UPDATE = 2 };

NtQSI_t NtQSI;
HWND hMain, hPop;
int popMode;
ULONGLONG tickPrev, tickNow;
static UINT wmTaskbar, wmQuit, wmReload;
static WCHAR updLatest[64];                              // newer version found by the background check, or empty
static BOOL updBusy;                                     // a background check is running

static void CheckUpdates() {                             // the updater itself skips it until check_interval_hours pass
    if (!updBusy) updBusy = IlizUpdaterCheckAsync(hMain, WM_APP_UPDATE, L"--if-due");
}

static void OnUpdateInfo(IlizUpdateInfo *u) {
    updBusy = FALSE;
    if (!u) return;
    if (u->exitCode == ILIZ_UPD_UPDATE_AVAILABLE && u->latest[0])
        MultiByteToWideChar(CP_UTF8, 0, u->latest, -1, updLatest, 64);
    else if (u->exitCode == ILIZ_UPD_OK) updLatest[0] = 0;
    IlizUpdaterFree(u);
}

static void LaunchUpdater(HWND h) {                      // interactive: shows versions and notes, asks, then closes us
    if (!IlizUpdaterLaunch(FALSE))
        MessageBoxW(h, T(S_UPD_FAIL),
                    APP_NAME, MB_OK | MB_ICONWARNING | MB_TOPMOST);
}

static BOOL ConfigPath(WCHAR *p) {                       // iliz_resmon_config.exe next to our exe
    SettingsDir(p); lstrcatW(p, L"iliz_resmon_config.exe");
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

static void ShowAbout(HWND h) {
    WCHAR t[256];
    const WCHAR *st[] = {T(S_ETW_OFF), T(S_ETW_ON), T(S_ETW_FAIL), T(S_ETW_RESTART)};
    wsprintfW(t, T(S_ABOUT), APP_NAME, APP_VERSION, T(S_DESC), st[etwState]);
    if (etwErr) wsprintfW(t + lstrlenW(t), T(S_ABOUT_ERR), etwErr);
    if (etwLost) wsprintfW(t + lstrlenW(t), T(S_ABOUT_LOST), etwLost);
    MessageBoxW(h, t, APP_NAME, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

static void ShowMenu(HWND h, int x, int y) {
    HidePopup();
    HMENU menu = CreatePopupMenu();
    BOOL elev = IsElevated();
    if (EtwNote()) { AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, T(EtwNote())); }   // why per-process traffic is missing
    if (!elev) AppendMenuW(menu, MF_STRING, IDM_ADMIN, T(S_M_ADMIN));
    if (EtwNote()) AppendMenuW(menu, MF_SEPARATOR, 0, 0);
    WCHAR cfgPath[MAX_PATH];
    BOOL hasCfg = ConfigPath(cfgPath);
    if (hasCfg) AppendMenuW(menu, MF_STRING, IDM_CONFIG, T(S_M_CONFIG));
    WCHAR updPath[MAX_PATH], updText[96];
    if (IlizUpdaterPath(updPath, MAX_PATH)) {            // no updater next to the exe (dev build): no menu item
        if (updLatest[0]) wsprintfW(updText, T(S_M_UPDATE_TO), updLatest);
        else lstrcpyW(updText, T(S_M_CHECK));
        AppendMenuW(menu, MF_STRING, IDM_UPDATE, updText);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, 0);
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, APP_NAME L" v" APP_VERSION);
    AppendMenuW(menu, MF_SEPARATOR, 0, 0);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, T(S_M_EXIT));
    SetForegroundWindow(h);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, x, y, 0, h, 0);
    DestroyMenu(menu);
    PostMessageW(h, WM_NULL, 0, 0);
    if (cmd == IDM_ABOUT) ShowAbout(h);
    else if (cmd == IDM_CONFIG) ShellExecuteW(h, 0, cfgPath, 0, 0, SW_SHOWNORMAL);
    else if (cmd == IDM_UPDATE) LaunchUpdater(h);
    else if (cmd == IDM_ADMIN) RelaunchElevated();          // the elevated instance asks us to quit
    else if (cmd == IDM_EXIT) DestroyWindow(h);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_TIMER: if (w == TIMER_UPDATE) CheckUpdates(); else AppTick(); return 0;
    case WM_APP_UPDATE: OnUpdateInfo((IlizUpdateInfo*)l); return 0;
    case WM_TRAY:                                        // NOTIFYICON_VERSION_4: event in LOWORD(l), icon id in HIWORD(l)
        switch (LOWORD(l)) {
        case NIN_POPUPOPEN:  ShowPopup(HIWORD(l)); break;
        case NIN_POPUPCLOSE: HidePopup(); break;
        case NIN_SELECT: case NIN_KEYSELECT: HidePopup(); HistWndShow(HIWORD(l)); break;   // left click: 60-min history
        case WM_CONTEXTMENU: ShowMenu(h, (short)LOWORD(w), (short)HIWORD(w)); break;
        }
        return 0;
    case WM_DESTROY:
        EtwStop(); RemoveIcons();
        PostQuitMessage(0); return 0;
    }
    if (m == wmTaskbar) { AddIcons(); return 0; }        // Explorer restarted
    if (m == wmReload) {                                 // from iliz_resmon_config.exe
        HidePopup(); ReloadTraySettings();
        SetTimer(h, TIMER_TICK, TrayInterval(), 0);      // same id: replaces (restarts) the tick timer
        return 0;
    }   // from iliz_resmon_config.exe
    if (m == wmQuit) { DestroyWindow(h); return 0; }      // elevated instance takes over
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    wmQuit = RegisterWindowMessageW(APP_ID L"_quit");
    HANDLE mx = CreateMutexW(0, TRUE, APP_ID L"_once");
    if (!mx || GetLastError() == ERROR_ALREADY_EXISTS) {  // another instance (or one we can't open)
        if (!mx || !IsElevated()) return 0;
        DWORD r = WAIT_TIMEOUT;                          // ask it to quit; it may still be starting up
        for (int i = 0; i < 12 && r == WAIT_TIMEOUT; i++) {
            HWND old = FindWindowW(APP_ID, 0);
            if (old) PostMessageW(old, wmQuit, 0, 0);
            r = WaitForSingleObject(mx, 250);
        }
        if (r != WAIT_OBJECT_0 && r != WAIT_ABANDONED) return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    NtQSI = (NtQSI_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation");
    wmTaskbar = RegisterWindowMessageW(L"TaskbarCreated");
    AppInit();

    WNDCLASSW wc = {0}; wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = APP_ID;
    RegisterClassW(&wc);
    wc.lpfnWndProc = PopProc; wc.lpszClassName = APP_ID L"_pop"; wc.hCursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW);
    RegisterClassW(&wc);
    // Hidden top-level window (not HWND_MESSAGE): message-only windows miss the TaskbarCreated broadcast.
    hMain = CreateWindowExW(WS_EX_TOOLWINDOW, APP_ID, L"", WS_POPUP, 0, 0, 0, 0, 0, 0, hi, 0);
    ChangeWindowMessageFilterEx(hMain, wmQuit, MSGFLT_ALLOW, 0);
    wmReload = RegisterWindowMessageW(SETTINGS_RELOAD_MSG);  // the configurator runs non-elevated: let it through UIPI
    ChangeWindowMessageFilterEx(hMain, wmReload, MSGFLT_ALLOW, 0);
    hPop = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, APP_ID L"_pop", L"",
                           WS_POPUP, 0, 0, 10, 10, 0, 0, hi, 0);

    InitTray();
    LoadTraySettings();
    tickNow = GetTickCount64();
    AppSample();                                         // baseline: lists are ready after the first tick
    AddIcons();
    SetTimer(hMain, TIMER_TICK, TrayInterval(), 0);
    IlizUpdaterWatchQuit(hMain, WM_CLOSE);               // the updater asks us to exit before installing
    CheckUpdates();
    SetTimer(hMain, TIMER_UPDATE, 6 * 3600 * 1000, 0);
    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0) DispatchMessageW(&msg);
    return 0;
}
