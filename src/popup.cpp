// Hover popup window: placement near the tray icon (per-monitor DPI), repaint, dispatch to views.
#include "common.h"
#include <shellapi.h>
#include "popup.h"
#include "views.h"
#include "disk.h"

static int rows;                           // row count at last placement

static void Place() {
    NOTIFYICONIDENTIFIER ni = {sizeof(ni)}; ni.hWnd = hMain; ni.uID = popMode;
    RECT ir;
    if (Shell_NotifyIconGetRect(&ni, &ir) != S_OK) { POINT c; GetCursorPos(&c); SetRect(&ir, c.x, c.y, c.x + 1, c.y + 1); }
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&ir, MONITOR_DEFAULTTONEAREST), &mi);
    RECT w = mi.rcWork, m = mi.rcMonitor;
    SetWindowPos(hPop, 0, w.left, w.top, 1, 1, SWP_NOACTIVATE | SWP_NOZORDER);   // pick up that monitor's DPI
    dpi = GetDpiForWindow(hPop); if (!dpi) dpi = 96;
    rows = ViewRows(popMode);
    int PW = ViewWidth(popMode), PH = ROWH * rows + SC(10), gap = SC(6);
    int px = (ir.left + ir.right) / 2 - PW / 2, py;
    if (w.bottom < m.bottom)      py = w.bottom - PH - gap;   // taskbar at bottom
    else if (w.top > m.top)       py = w.top + gap;           // top
    else if (w.left > m.left)   { px = w.left + gap; py = ir.top; }
    else if (w.right < m.right) { px = w.right - PW - gap; py = ir.top; }
    else                          py = ir.top - PH - gap;     // auto-hide
    if (px + PW > w.right) px = w.right - PW - gap;
    if (px < w.left) px = w.left + gap;
    if (py + PH > w.bottom) py = w.bottom - PH - gap;
    if (py < w.top) py = w.top + gap;
    SetWindowPos(hPop, HWND_TOPMOST, px, py, PW, PH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hPop, 0, TRUE);
}

void ShowPopup(int id) {
    if (id == ID_DISK && !ndisk) return;
    popMode = id;                                        // all lists are kept up to date every tick
    Place();
}

void HidePopup() { ShowWindow(hPop, SW_HIDE); popMode = 0; }

void PopupTick() {
    if (ViewRows(popMode) != rows) Place();              // trace may come up/down -> resize
    InvalidateRect(hPop, 0, TRUE);
}

static void Paint(HWND h) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    RECT r; GetClientRect(h, &r);
    COLORREF cMain, cBar; ViewColors(popMode, &cMain, &cBar);
    PCTX c = {dc, PAD, r.right - PAD, 0, cMain, C_TEXT, C_DIM, CreateSolidBrush(cBar), CreateSolidBrush(C_GHOST_BAR)};
    HBRUSH bg = CreateSolidBrush(C_POP_BG), frame = CreateSolidBrush(cMain);
    FillRect(dc, &r, bg); FrameRect(dc, &r, frame);
    SelectObject(dc, DrawFont()); SetBkMode(dc, TRANSPARENT);
    ViewPaint(&c, popMode);
    DeleteObject(bg); DeleteObject(c.bar); DeleteObject(c.ghost); DeleteObject(frame);
    EndPaint(h, &ps);
}

LRESULT CALLBACK PopProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) { Paint(h); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, w, l);
}
