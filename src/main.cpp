// iliz resmon: CPU / memory / disk / network load in the tray. Pure WinAPI, no third-party libraries.
// main: windows, message loop, menu, single instance + elevated takeover.
#include "common.h"
#include "appinfo.h"
#include <shellapi.h>
#include "app.h"
#include "etw.h"
#include "procs.h"
#include "trayicon.h"
#include "popup.h"
#include "autostart.h"

enum { IDM_AUTO = 1, IDM_ADMIN, IDM_ABOUT, IDM_EXIT };

NtQSI_t NtQSI;
HWND hMain, hPop;
int popMode;
ULONGLONG tickPrev, tickNow;
static UINT wmTaskbar, wmQuit;

static void ShowAbout(HWND h) {
    WCHAR t[256];
    static const WCHAR *st[] = {L"не запущена", L"работает", L"недоступна", L"прервана, перезапуск"};
    wsprintfW(t, APP_NAME L"\nВерсия " APP_VERSION L"\n\n" APP_DESC L"\n\nТрассировка дисков/сети: %s", st[etwState]);
    if (etwErr) wsprintfW(t + lstrlenW(t), L" (ошибка %lu)", etwErr);
    if (etwLost) wsprintfW(t + lstrlenW(t), L"\nПотеряно событий: %lu", etwLost);
    MessageBoxW(h, t, APP_NAME, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

static void ShowMenu(HWND h, int x, int y) {
    HidePopup();
    HMENU menu = CreatePopupMenu();
    BOOL autoOn = AutostartEnabled(), elev = IsElevated();
    AppendMenuW(menu, MF_STRING | (autoOn ? MF_CHECKED : 0), IDM_AUTO, L"Запускать с Windows");
    if (!elev) AppendMenuW(menu, MF_STRING, IDM_ADMIN, L"Перезапустить от администратора");
    AppendMenuW(menu, MF_SEPARATOR, 0, 0);
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, APP_NAME L" v" APP_VERSION);
    AppendMenuW(menu, MF_SEPARATOR, 0, 0);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Выход");
    SetForegroundWindow(h);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, x, y, 0, h, 0);
    DestroyMenu(menu);
    PostMessageW(h, WM_NULL, 0, 0);
    if (cmd == IDM_ABOUT) ShowAbout(h);
    else if (cmd == IDM_AUTO) AutostartSet(!autoOn);
    else if (cmd == IDM_ADMIN) RelaunchElevated();          // the elevated instance asks us to quit
    else if (cmd == IDM_EXIT) DestroyWindow(h);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_TIMER: AppTick(); return 0;
    case WM_TRAY:                                        // NOTIFYICON_VERSION_4: event in LOWORD(l), icon id in HIWORD(l)
        switch (LOWORD(l)) {
        case NIN_POPUPOPEN:  ShowPopup(HIWORD(l)); break;
        case NIN_POPUPCLOSE: HidePopup(); break;
        case WM_CONTEXTMENU: ShowMenu(h, (short)LOWORD(w), (short)HIWORD(w)); break;
        }
        return 0;
    case WM_DESTROY:
        EtwStop(); RemoveIcons();
        PostQuitMessage(0); return 0;
    }
    if (m == wmTaskbar) { AddIcons(); return 0; }        // Explorer restarted
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
    hPop = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, APP_ID L"_pop", L"",
                           WS_POPUP, 0, 0, 10, 10, 0, 0, hi, 0);

    InitTray();
    tickNow = GetTickCount64();
    AppSample();                                         // baseline: lists are ready after the first tick
    AddIcons();
    SetTimer(hMain, 1, 1000, 0);
    MSG msg;
    while (GetMessageW(&msg, 0, 0, 0) > 0) DispatchMessageW(&msg);
    return 0;
}
