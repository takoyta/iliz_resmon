// iliz_resmon_config.exe: tiny settings dialog for iliz resmon. Pure WinAPI, no CRT (entry in src\nocrt.cpp).
// Every checkbox change is saved to iliz_resmon.ini at once and the running iliz_resmon is told to reread it.
#include "../src/common.h"
#include "../src/settings.h"
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
        MessageBoxW(d, L"Не удалось сохранить iliz_resmon.ini. Нет прав на запись в папку программы?",
                    L"iliz resmon", MB_OK | MB_ICONWARNING);
        return;
    }
    Notify();
}

static INT_PTR CALLBACK DlgProc(HWND d, UINT m, WPARAM w, LPARAM) {
    switch (m) {
    case WM_INITDIALOG: Load(d); return TRUE;
    case WM_COMMAND:
        if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL) { EndDialog(d, 0); return TRUE; }
        if (HIWORD(w) == BN_CLICKED && LOWORD(w) >= IDC_CPU && LOWORD(w) <= IDC_NET) { OnToggle(d, LOWORD(w)); return TRUE; }
        break;
    }
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    HANDLE mx = CreateMutexW(0, TRUE, L"iliz_resmon_config_once");
    if (mx && GetLastError() == ERROR_ALREADY_EXISTS) {  // already open: bring it to front
        HWND w = FindWindowW(L"#32770", L"iliz resmon: настройки");
        if (w) SetForegroundWindow(w);
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);   // dialogs rescale themselves
    wmReload = RegisterWindowMessageW(SETTINGS_RELOAD_MSG);
    DialogBoxParamW(hi, MAKEINTRESOURCEW(IDD_MAIN), 0, DlgProc, 0);
    return 0;
}
