// iliz_resmon_config.exe: tiny settings dialog for iliz resmon. Pure WinAPI, no CRT (entry in src\nocrt.cpp).
// Every change is saved at once (icons, update interval: iliz_resmon.ini; autostart: HKCU Run) and the running iliz_resmon is told to reread it.
#include "../src/common.h"
#include "../src/settings.h"
#include "../src/lang.h"
#include "config.h"

static UINT wmReload;
static const int ids[5] = {IDC_CPU, IDC_MEM, IDC_DISK, IDC_NET, IDC_GPU};
static const int intervals[3] = {500, 1000, 2000};      // combo items, in order

static void Notify() {                                   // every top-level window of our class (normally one)
    HWND w = 0;
    while ((w = FindWindowExW(0, w, SETTINGS_WND_CLASS, 0)) != 0) PostMessageW(w, wmReload, 0, 0);
}

static void Load(HWND d) {
    Settings s; LoadSettings(&s);
    int v[5] = {s.cpu, s.mem, s.disk, s.net, s.gpu};
    if (!v[0] && !v[1] && !v[2] && !v[3] && !v[4]) v[0] = 1;
    for (int i = 0; i < 5; i++) CheckDlgButton(d, ids[i], v[i] ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(d, IDC_TOPN, CB_SETCURSEL, s.topn - 3, 0);      // items 3..15
    for (int i = 0; i < 3; i++) if (intervals[i] == s.interval) SendDlgItemMessageW(d, IDC_INTERVAL, CB_SETCURSEL, i, 0);
    CheckDlgButton(d, IDC_AUTO, AutostartEnabled() ? BST_CHECKED : BST_UNCHECKED);
}

static void OnAutostart(HWND d) {                        // HKCU Run: same user even when we run elevated
    BOOL on = IsDlgButtonChecked(d, IDC_AUTO) == BST_CHECKED;
    if (!AutostartSet(on)) {
        CheckDlgButton(d, IDC_AUTO, on ? BST_UNCHECKED : BST_CHECKED);
        MessageBoxW(d, T(S_CFG_AUTO_FAIL), L"iliz resmon", MB_OK | MB_ICONWARNING);
    }
}

static void Save(HWND d, const int *v) {                 // v: icon checkboxes in ids[] order
    int sel = (int)SendDlgItemMessageW(d, IDC_INTERVAL, CB_GETCURSEL, 0, 0);
    int tn = (int)SendDlgItemMessageW(d, IDC_TOPN, CB_GETCURSEL, 0, 0);
    Settings s = {v[0], v[1], v[2], v[3], intervals[sel >= 0 && sel < 3 ? sel : 1], v[4], tn >= 0 ? tn + 3 : 10};
    if (!SaveSettings(&s)) {
        MessageBoxW(d, T(S_CFG_SAVE_FAIL),
                    L"iliz resmon", MB_OK | MB_ICONWARNING);
        return;
    }
    Notify();
}

static int Icons(HWND d, int *v) {                       // read the icon checkboxes; returns how many are on
    int on = 0;
    for (int i = 0; i < 5; i++) on += v[i] = IsDlgButtonChecked(d, ids[i]) == BST_CHECKED;
    return on;
}

static void OnToggle(HWND d, int id) {
    int v[5];
    if (!Icons(d, v)) {                                           // keep at least one icon: the menu lives on it
        CheckDlgButton(d, id, BST_CHECKED);
        MessageBeep(MB_ICONWARNING);
        return;
    }
    Save(d, v);
}

static void Texts(HWND d) {                              // the template is English; set the UI language here
    SetWindowTextW(d, T(S_CFG_TITLE));
    SetDlgItemTextW(d, IDC_GROUP, T(S_CFG_GROUP));
    SetDlgItemTextW(d, IDC_MEM, T(S_CFG_MEM));
    SetDlgItemTextW(d, IDC_DISK, T(S_CFG_DISK));
    SetDlgItemTextW(d, IDC_NET, T(S_CFG_NET));
    SetDlgItemTextW(d, IDC_GPU, T(S_CFG_GPU));
    SetDlgItemTextW(d, IDC_AUTO, T(S_CFG_AUTO));
    SetDlgItemTextW(d, IDC_INTERVAL_LBL, T(S_CFG_INTERVAL));
    for (int i = 0; i < 3; i++) SendDlgItemMessageW(d, IDC_INTERVAL, CB_ADDSTRING, 0, (LPARAM)T(S_CFG_I500 + i));
    SetDlgItemTextW(d, IDC_TOPN_LBL, T(S_CFG_TOPN));
    for (int i = 3; i <= 15; i++) { WCHAR n[4]; wsprintfW(n, L"%d", i); SendDlgItemMessageW(d, IDC_TOPN, CB_ADDSTRING, 0, (LPARAM)n); }
    SetDlgItemTextW(d, IDOK, T(S_CFG_CLOSE));
}

static INT_PTR CALLBACK DlgProc(HWND d, UINT m, WPARAM w, LPARAM) {
    switch (m) {
    case WM_INITDIALOG: Texts(d); Load(d); return TRUE;
    case WM_COMMAND:
        if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) { EndDialog(d, 0); return TRUE; }
        if (HIWORD(w) == CBN_SELCHANGE && (LOWORD(w) == IDC_INTERVAL || LOWORD(w) == IDC_TOPN)) { int v[5]; Icons(d, v); Save(d, v); return TRUE; }
        if (HIWORD(w) == BN_CLICKED && LOWORD(w) == IDC_AUTO) { OnAutostart(d); return TRUE; }
        if (HIWORD(w) == BN_CLICKED && ((LOWORD(w) >= IDC_CPU && LOWORD(w) <= IDC_NET) || LOWORD(w) == IDC_GPU)) { OnToggle(d, LOWORD(w)); return TRUE; }
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
