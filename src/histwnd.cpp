// History window: one hour of a metric from history.cpp, GDI, double-buffered (no flicker). Esc closes.
#include "common.h"
#include "lang.h"
#include "histwnd.h"
#include "history.h"
#include "draw.h"
#include "format.h"

#define HW_CLASS APP_ID L"_hist"
static HWND hw; static int hwId, hwDpi = 96;

typedef struct { int a, b, net; COLORREF cMain, cBar, c2; const WCHAR *name; } SERIES;   // b < 0: no second line

static void Series(int id, SERIES *s) {
    s->b = -1; s->net = 0; s->c2 = 0;
    if (id == ID_NET) { s->a = H_RX; s->b = H_TX; s->net = 1; s->cMain = C_RX; s->cBar = C_RX_DARK; s->c2 = C_TX; s->name = T(S_CFG_NET); }
    else if (id == ID_MEM) { s->a = H_MEM; s->cMain = C_MEM; s->cBar = C_MEM_DARK; s->name = T(S_CFG_MEM); }
    else if (id == ID_GPU) { s->a = H_GPU; s->cMain = C_GPU; s->cBar = C_GPU_DARK; s->name = L"GPU"; }
    else if (id == ID_DISK) { s->a = H_DISK; s->cMain = C_DSK; s->cBar = C_DSK_DARK; s->name = T(S_CFG_DISK); }
    else { s->a = H_CPU; s->b = H_CPUMAX; s->cMain = C_CPU; s->cBar = C_CPU_DARK; s->c2 = C_CPU_PEAK; s->name = L"CPU"; }
}

static ULONG Val(const SERIES *s, int m, int i) { return s->net ? HistNet(m, i) : (ULONG)HistPct(m, i); }

// Max of series m over the samples of graph column x (slot HLONG-1 = newest, at the right edge).
static int ColMax(const SERIES *s, int m, int x, int W, int n, ULONG full, int H) {
    int lo = (int)((LONGLONG)x * HLONG / W), hi = (int)((LONGLONG)(x + 1) * HLONG / W);
    if (hi <= lo) hi = lo + 1;
    int first = HLONG - n; if (lo < first) lo = first;
    if (lo >= hi) return -1;                             // no data that far back
    ULONG v = 0;
    for (int k = lo; k < hi; k++) { ULONG t = Val(s, m, k - first); if (t > v) v = t; }
    return (int)((ULONGLONG)v * (H - 2) / full);
}

static void Paint(HWND h) {
    PAINTSTRUCT ps; HDC wdc = BeginPaint(h, &ps);
    RECT r; GetClientRect(h, &r);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, r.right, r.bottom); HGDIOBJ oldBmp = SelectObject(dc, bmp);
    int saveDpi = dpi; dpi = hwDpi;                      // draw.h scaling + font for this window's monitor
    HGDIOBJ oldFont = SelectObject(dc, DrawFont()); SetBkMode(dc, TRANSPARENT);
    SERIES s; Series(hwId, &s);
    HBRUSH bg = CreateSolidBrush(C_POP_BG); FillRect(dc, &r, bg); DeleteObject(bg);

    int n = HistLen();
    ULONG full = s.net ? 1024 : 100, pa = 0, pb = 0;     // scale; peaks of a and b over the hour
    for (int i = 0; i < n; i++) {
        ULONG x = Val(&s, s.a, i); if (x > pa) pa = x;
        if (s.b >= 0) { ULONG y = Val(&s, s.b, i); if (y > pb) pb = y; }
    }
    if (s.net) { if (pa > full) full = pa; if (pb > full) full = pb; }

    int L = SC(10), R = r.right - SC(10), T0 = SC(28), B = r.bottom - SC(22);
    if (R - L < 10 || B - T0 < 10) goto done;
    {
    WCHAR t[96], a[24], b[24];
    RECT tr = {L, SC(4), R, T0 - SC(4)};
    wsprintfW(t, T(S_HIST_TITLE), s.name);
    SetTextColor(dc, s.cMain); DrawTextW(dc, t, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    if (s.net) { FmtRate(a, pa); FmtRate(b, pb); wsprintfW(t, L"%s / %s", a, b); }
    else if (s.b >= 0) wsprintfW(t, L"%d%% / %d%%", pa, pb);
    else wsprintfW(t, L"%d%%", pa);
    WCHAR pk[64]; wsprintfW(pk, T(S_HIST_PEAK), t);
    SetTextColor(dc, C_DIM); DrawTextW(dc, pk, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    int W = R - L, H = B - T0;
    RECT g = {L, T0, R, B}; HBRUSH gb = CreateSolidBrush(C_GRAPH_BG); FillRect(dc, &g, gb); DeleteObject(gb);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCPenColor(dc, C_GRID);
    for (int q = 1; q < 4; q++) { int y = B - 1 - q * (H - 2) / 4; MoveToEx(dc, L, y, 0); LineTo(dc, R, y); }
    int step = W >= SC(480) ? 10 : W >= SC(240) ? 15 : 30;   // minutes between time labels
    SetTextColor(dc, C_DIM);
    for (int m = 0; m <= 60; m += step) {
        int x = R - 1 - m * (W - 1) / 60;
        if (m && m < 60) { MoveToEx(dc, x, T0, 0); LineTo(dc, x, B); }
        if (m) wsprintfW(t, T(S_MIN_AGO), m); else lstrcpyW(t, T(S_NOW));
        RECT lr = {x - SC(40), B + SC(3), x + SC(40), r.bottom};
        UINT al = m == 60 ? DT_LEFT : m == 0 ? DT_RIGHT : DT_CENTER;
        if (m == 60) lr.left = x; if (m == 0) lr.right = x + 1;
        DrawTextW(dc, t, -1, &lr, DT_SINGLELINE | DT_TOP | al);
    }

    HBRUSH bar = CreateSolidBrush(s.cBar);               // series a: filled columns, top edge in cMain
    for (int pass = 0; pass < 3; pass++) {
        int m = pass == 2 ? s.b : s.a; if (m < 0) continue;
        SetDCPenColor(dc, pass == 2 ? s.c2 : s.cMain);
        int prev = 0;
        for (int x = 0; x < W; x++) {
            int v = ColMax(&s, m, x, W, n, full, H);
            if (v < 0) { prev = 0; continue; }
            int y = B - 1 - v;
            if (pass == 0) { RECT c = {L + x, y, L + x + 1, B}; FillRect(dc, &c, bar); }
            else { if (prev) LineTo(dc, L + x, y); else MoveToEx(dc, L + x, y, 0); prev = 1; }
        }
        if (pass == 1 && s.b < 0) break;
    }
    DeleteObject(bar);
    SelectObject(dc, oldPen);
    RECT lr = {L + SC(4), T0 + SC(2), R - SC(4), B};    // scale label
    if (s.net) FmtRate(t, full); else lstrcpyW(t, L"100%");
    SetTextColor(dc, C_DIM); DrawTextW(dc, t, -1, &lr, DT_SINGLELINE | DT_TOP | DT_RIGHT);
    }
done:
    BitBlt(wdc, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldFont); SelectObject(dc, oldBmp); DeleteObject(bmp); DeleteDC(dc);
    dpi = saveDpi;
    EndPaint(h, &ps);
}

static void Title(HWND h) {
    SERIES s; Series(hwId, &s); WCHAR t[96];
    wsprintfW(t, T(S_HIST_TITLE), s.name); SetWindowTextW(h, t);
}

static LRESULT CALLBACK HistProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: Paint(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: InvalidateRect(h, 0, FALSE); return 0;
    case WM_KEYDOWN: if (w == VK_ESCAPE) DestroyWindow(h); return 0;
    case WM_DPICHANGED: {
        hwDpi = HIWORD(w); RECT *nr = (RECT*)l;
        SetWindowPos(h, 0, nr->left, nr->top, nr->right - nr->left, nr->bottom - nr->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0; }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO*)l; mm->ptMinTrackSize.x = MulDiv(320, hwDpi, 96); mm->ptMinTrackSize.y = MulDiv(160, hwDpi, 96);
        return 0; }
    case WM_DESTROY: hw = 0; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void HistWndShow(int id) {
    hwId = id;
    if (hw) { Title(hw); InvalidateRect(hw, 0, FALSE); ShowWindow(hw, SW_SHOWNORMAL); SetForegroundWindow(hw); return; }
    static BOOL reg;
    HINSTANCE hi = GetModuleHandleW(0);
    if (!reg) {
        WNDCLASSW wc = {0}; wc.lpfnWndProc = HistProc; wc.hInstance = hi; wc.lpszClassName = HW_CLASS;
        wc.hCursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW); wc.hIcon = LoadIconW(0, (LPCWSTR)IDI_APPLICATION);
        reg = RegisterClassW(&wc) != 0;
    }
    POINT pt; GetCursorPos(&pt);                         // open on the monitor of the clicked icon, near the tray corner
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
    RECT wa = mi.rcWork;
    DWORD st = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX;
    hw = CreateWindowExW(0, HW_CLASS, L"", st, wa.left, wa.top, 100, 100, 0, 0, hi, 0);
    if (!hw) return;
    hwDpi = GetDpiForWindow(hw); if (!hwDpi) hwDpi = 96;
    RECT r = {0, 0, MulDiv(640, hwDpi, 96), MulDiv(260, hwDpi, 96)};
    AdjustWindowRectExForDpi(&r, st, FALSE, 0, hwDpi);
    int W = r.right - r.left, H = r.bottom - r.top, gap = MulDiv(8, hwDpi, 96);
    int x = pt.x - W / 2, y = pt.y - H - gap;            // above the cursor, clamped to the work area
    if (x + W > wa.right - gap) x = wa.right - gap - W;
    if (x < wa.left + gap) x = wa.left + gap;
    if (y < wa.top + gap) y = pt.y + gap;
    if (y + H > wa.bottom - gap) y = wa.bottom - gap - H;
    if (y < wa.top) y = wa.top;
    SetWindowPos(hw, 0, x, y, W, H, SWP_NOZORDER);
    Title(hw);
    ShowWindow(hw, SW_SHOWNORMAL); SetForegroundWindow(hw);
}

void HistWndTick() { if (hw && !IsIconic(hw)) InvalidateRect(hw, 0, FALSE); }
