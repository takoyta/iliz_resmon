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
    if (!IsWindowVisible(hPop))                          // pick up that monitor's DPI (visible: already there, no jump)
        SetWindowPos(hPop, 0, w.left, w.top, 1, 1, SWP_NOACTIVATE | SWP_NOZORDER);
    dpi = GetDpiForWindow(hPop); if (!dpi) dpi = 96;
    rows = ViewRows(popMode);
    int PW = ViewWidth(popMode), PH = ROWH * rows + SC(10), gap = SC(6);
    if (PH > w.bottom - w.top - 2 * gap) PH = w.bottom - w.top - 2 * gap;   // very tall (many disks, big top_n): clip the bottom rows
    int px = (ir.left + ir.right) / 2 - PW / 2, py;
    POINT ic = {(ir.left + ir.right) / 2, (ir.top + ir.bottom) / 2};
    if (PtInRect(&w, ic)) {                              // icon sits in the hidden-icons flyout, not on the taskbar
        RECT fr = ir;                                    // avoid covering the flyout window itself
        HWND fw = GetAncestor(WindowFromPoint(ic), GA_ROOT);
        if (fw) GetWindowRect(fw, &fr);
        if (fr.top - PH - gap >= w.top)          py = fr.top - PH - gap;          // above the flyout
        else if (fr.left - PW - gap >= w.left) { px = fr.left - PW - gap; py = fr.bottom - PH; }  // left of it
        else if (fr.right + PW + gap <= w.right) { px = fr.right + gap; py = fr.bottom - PH; }    // right of it
        else                                     py = fr.bottom + gap;            // below
    }
    else if (w.bottom < m.bottom)      py = w.bottom - PH - gap;   // taskbar at bottom
    else if (w.top > m.top)       py = w.top + gap;           // top
    else if (w.left > m.left)   { px = w.left + gap; py = ir.top; }
    else if (w.right < m.right) { px = w.right - PW - gap; py = ir.top; }
    else                          py = ir.top - PH - gap;     // auto-hide
    if (px + PW > w.right) px = w.right - PW - gap;
    if (px < w.left) px = w.left + gap;
    if (py + PH > w.bottom) py = w.bottom - PH - gap;
    if (py < w.top) py = w.top + gap;
    SetWindowPos(hPop, HWND_TOPMOST, px, py, PW, PH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hPop, 0, FALSE);
}

void ShowPopup(int id) {
    if (id == ID_DISK && !ndisk) return;
    popMode = id;                                        // all lists are kept up to date every tick
    Place();
}

void HidePopup() { ShowWindow(hPop, SW_HIDE); popMode = 0; }

void PopupTick() {
    if (ViewRows(popMode) != rows) Place();              // trace may come up/down -> resize
    InvalidateRect(hPop, 0, FALSE);
}

static void Paint(HWND h) {
    PAINTSTRUCT ps; HDC wdc = BeginPaint(h, &ps);
    RECT r; GetClientRect(h, &r);
    HDC dc = CreateCompatibleDC(wdc);                    // draw off-screen, blit once: no flicker
    HBITMAP bmp = CreateCompatibleBitmap(wdc, r.right, r.bottom); HGDIOBJ oldBmp = SelectObject(dc, bmp);
    COLORREF cMain, cBar; ViewColors(popMode, &cMain, &cBar);
    PCTX c = {dc, PAD, r.right - PAD, 0, cMain, C_TEXT, C_DIM, CreateSolidBrush(cBar), CreateSolidBrush(C_GHOST_BAR), CreateSolidBrush(C_IN_BAR),
              CreateSolidBrush(C_OUT_BAR), CreateSolidBrush(C_GHOST_BAR2)};
    HBRUSH bg = CreateSolidBrush(C_POP_BG), frame = CreateSolidBrush(cMain);
    FillRect(dc, &r, bg); FrameRect(dc, &r, frame);
    SelectObject(dc, DrawFont()); SetBkMode(dc, TRANSPARENT);
    ViewPaint(&c, popMode);
    DeleteObject(bg); DeleteObject(c.bar); DeleteObject(c.ghost); DeleteObject(c.barIn); DeleteObject(c.barOut); DeleteObject(c.ghost2); DeleteObject(frame);
    BitBlt(wdc, 0, 0, r.right, r.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp); DeleteObject(bmp); DeleteDC(dc);
    EndPaint(h, &ps);
}

LRESULT CALLBACK PopProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) { Paint(h); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, w, l);
}
