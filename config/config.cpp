// iliz_resmon_config.exe: tiny settings dialog for iliz resmon. Pure WinAPI, no CRT (entry in src\nocrt.cpp).
// Every checkbox change is saved (icons: ini, autostart: HKCU Run) to iliz_resmon.ini at once and the running iliz_resmon is told to reread it.
#include "../src/common.h"
#include "../src/settings.h"
#include "../src/lang.h"
#include "config.h"

static UINT wmReload;
static const int ids[4] = {IDC_CPU, IDC_MEM, IDC_DISK, IDC_NET};

static void Notify() {                                   // every top-level window of our class (normally one)
    HWND w = 0;
    while ((w = FindWindowExW(0, w, SETTINGS_WND_CLASS, 0)) != 0) PostMessageW(w, wmReload, 0, 0);
}

static void Load(HWND d) {
    Settings s; LoadSettings(&s);
    int v[4] = {s.cpu, s.mem, s.disk, s.net};
    if (!v[0] && !v[1] && !v[2] && !v[3]) v[0] = 1;
    for (int i = 0; i < 4; i++) CheckDlgButton(d, ids[i], v[i] ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(d, IDC_AUTO, AutostartEnabled() ? BST_CHECKED : BST_UNCHECKED);
}

static void OnAutostart(HWND d) {                        // HKCU Run: same user even when we run elevated
    BOOL on = IsDlgButtonChecked(d, IDC_AUTO) == BST_CHECKED;
    if (!AutostartSet(on)) {
        CheckDlgButton(d, IDC_AUTO, on ? BST_UNCHECKED : BST_CHECKED);
        MessageBoxW(d, T(S_CFG_AUTO_FAIL), L"iliz resmon", MB_OK | MB_ICONWARNING);
    }
}

static void OnToggle(HWND d, int id) {
    int v[4], on = 0;
    for (int i = 0; i < 4; i++) on += v[i] = IsDlgButtonChecked(d, ids[i]) == BST_CHECKED;
    if (!on) {                                           // keep at least one icon: the menu lives on it
        CheckDlgButton(d, id, BST_CHECKED);
        MessageBeep(MB_ICONWARNING);
        return;
    }
    Settings s = {v[0], v[1], v[2], v[3]};
    if (!SaveSettings(&s)) {
        MessageBoxW(d, T(S_CFG_SAVE_FAIL),
                    L"iliz resmon", MB_OK | MB_ICONWARNING);
        return;
    }
    Notify();
}

static void Texts(HWND d) {                              // the template is English; set the UI language here
    SetWindowTextW(d, T(S_CFG_TITLE));
    SetDlgItemTextW(d, IDC_GROUP, T(S_CFG_GROUP));
    SetDlgItemTextW(d, IDC_MEM, T(S_CFG_MEM));
    SetDlgItemTextW(d, IDC_DISK, T(S_CFG_DISK));
    SetDlgItemTextW(d, IDC_NET, T(S_CFG_NET));
    SetDlgItemTextW(d, IDC_AUTO, T(S_CFG_AUTO));
    SetDlgItemTextW(d, IDOK, T(S_CFG_CLOSE));
}

static INT_PTR CALLBACK DlgProc(HWND d, UINT m, WPARAM w, LPARAM) {
    switch (m) {
    case WM_INITDIALOG: Texts(d); Load(d); return TRUE;
    case WM_COMMAND:
        if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) { EndDialog(d, 0); return TRUE; }
        if (HIWORD(w) == BN_CLICKED && LOWORD(w) == IDC_AUTO) { OnAutostart(d); return TRUE; }
        if (HIWORD(w) == BN_CLICKED && LOWORD(w) >= IDC_CPU && LOWORD(w) <= IDC_NET) { OnToggle(d, LOWORD(w)); return TRUE; }
        break;
    }
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    HANDLE mx = CreateMutexW(0, TRUE, L"iliz_resmon_config_once");
    if (mx && GetLastError() == ERROR_ALREADY_EXISTS) {  // already open: bring it to front
        HWND w = FindWindowW(L"#32770", T(S_CFG_TITLE));
        if (w) SetForegroundWindow(w);
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);   // dialogs rescale themselves
    wmReload = RegisterWindowMessageW(SETTINGS_RELOAD_MSG);
    DialogBoxParamW(hi, MAKEINTRESOURCEW(IDD_MAIN), 0, DlgProc, 0);
    return 0;
}
