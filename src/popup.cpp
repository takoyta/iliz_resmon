// Hover popup: placement near the tray icon (per-monitor DPI) and painting per mode.
#include "common.h"
#include "popup.h"
#include "cpu.h"
#include "disk.h"
#include "net.h"
#include "mem.h"
#include "history.h"
#include "procs.h"
#include "etw.h"
#include "format.h"
#include "sticky.h"

static HFONT hFont;
static int dpi = 96, fontDpi;
#define SC(v) MulDiv((v), dpi, 96)
#define ROWH SC(20)
#define PAD SC(10)
#define GRAPH_ROWS 3                                      // graph height = 3 rows = SC(60)

static void UpdateFont() {
    if (fontDpi == dpi) return;
    NONCLIENTMETRICSW ncm = {sizeof(ncm)};
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi);
    ncm.lfMessageFont.lfQuality = CLEARTYPE_QUALITY;
    if (hFont) DeleteObject(hFont);
    hFont = CreateFontIndirectW(&ncm.lfMessageFont);
    fontDpi = dpi;
}

// ---- layout ----
static int PopW() { return popMode == ID_NET ? SC(520) : popMode == ID_DISK ? SC(330) : popMode == ID_MEM ? SC(300) : SC(240); }
static int PopRows() {
    if (popMode == ID_CPU) return 1 + GRAPH_ROWS + TOPN;
    if (popMode == ID_MEM) return 5 + GRAPH_ROWS + (mem.cache ? 1 : 0) + TOPN;
    if (popMode == ID_NET) return 2 * (nnic ? nnic : 1) + GRAPH_ROWS + (etwOn ? 2 + TOPN : 0);
    int r = 0;
    for (int i = 0; i < ndisk; i++) r += 2 + lstrlenW(disks[i].letters) + (etwOn ? DTOPN : 0);
    return etwOn ? r : r + 1 + TOPN;
}

static void Place() {
    NOTIFYICONIDENTIFIER ni = {sizeof(ni)}; ni.hWnd = hMain; ni.uID = popMode;
    RECT ir;
    if (Shell_NotifyIconGetRect(&ni, &ir) != S_OK) { POINT c; GetCursorPos(&c); SetRect(&ir, c.x, c.y, c.x + 1, c.y + 1); }
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&ir, MONITOR_DEFAULTTONEAREST), &mi);
    RECT w = mi.rcWork, m = mi.rcMonitor;
    SetWindowPos(hPop, 0, w.left, w.top, 1, 1, SWP_NOACTIVATE | SWP_NOZORDER);   // pick up that monitor's DPI
    dpi = GetDpiForWindow(hPop); if (!dpi) dpi = 96;
    UpdateFont();
    int PW = PopW(), PH = ROWH * PopRows() + SC(10), gap = SC(6);
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
    static int rows; int r = PopRows();                   // trace may come up later -> re-place
    if (r != rows) { rows = r; Place(); }
    InvalidateRect(hPop, 0, TRUE);
}

// ---- painting ----
typedef struct { HDC dc; int L, R, row; COLORREF cMain, cText, cDim; HBRUSH bar, ghost; } PCTX;
#define C_GHOST RGB(110, 120, 115)

static void Text(PCTX *c, int l, int r, const WCHAR *s, COLORREF col, UINT al) {
    RECT t = {l, SC(5) + ROWH * c->row, r, SC(5) + ROWH * (c->row + 1)};
    SetTextColor(c->dc, col);
    DrawTextW(c->dc, s, -1, &t, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | al);
}

static void BarB(PCTX *c, int x0, int w, int inset, HBRUSH br) {   // horizontal bar in current row
    int y = SC(5) + ROWH * c->row;
    RECT b = {x0, y + SC(inset), x0 + w, y + ROWH - SC(inset) + SC(2)};
    FillRect(c->dc, &b, br);
}
static void Bar(PCTX *c, int x0, int w, int inset) { BarB(c, x0, w, inset, c->bar); }

static void RxTx(PCTX *c, LONGLONG rx, LONGLONG tx, COLORREF cRx, COLORREF cTx) {   // "↓ rx   ↑ tx" right-aligned
    WCHAR a[24], s[32]; SIZE sz; int R = c->R;
    FmtRate(a, tx); wsprintfW(s, L"↑ %s", a);
    GetTextExtentPoint32W(c->dc, s, lstrlenW(s), &sz);
    int tw = sz.cx > SC(85) ? sz.cx : SC(85);
    Text(c, R - tw, R, s, cTx, DT_LEFT);
    FmtRate(a, rx); wsprintfW(s, L"↓ %s", a);
    Text(c, R - tw - SC(95), R - tw - SC(8), s, cRx, DT_LEFT);
}

// Graph over GRAPH_ROWS rows. Series are 0..1000, n samples, newest right.
// fill: dark area + main-color line; l2: extra line in c2, drawn on top if l2Top.
static void Curve(const int *v, int n, POINT *pt, int x0, int W, int y1, int H) {
    for (int i = 0; i < n; i++) {
        pt[i].x = x0 + (HISTN - n + i) * (W - 1) / (HISTN - 1);
        pt[i].y = y1 - 1 - v[i] * (H - 2) / 1000;
    }
}

static void PaintGraph(PCTX *c, const int *fill, const int *l2, int n, COLORREF c2, int l2Top, const WCHAR *topLabel) {
    int y0 = SC(5) + ROWH * c->row + SC(2), H = ROWH * GRAPH_ROWS - SC(4), y1 = y0 + H;
    int x0 = c->L, W = c->R - c->L;
    RECT r = {x0, y0, c->R, y1};
    HBRUSH bg = CreateSolidBrush(RGB(26, 36, 34)); FillRect(c->dc, &r, bg); DeleteObject(bg);
    HGDIOBJ oldPen = SelectObject(c->dc, GetStockObject(DC_PEN)), oldBr = SelectObject(c->dc, c->bar);
    SetDCPenColor(c->dc, RGB(40, 54, 50));
    for (int q = 1; q < 4; q++) { int y = y1 - 1 - q * (H - 2) / 4; MoveToEx(c->dc, x0, y, 0); LineTo(c->dc, c->R, y); }
    static POINT pt[HISTN + 2];
    if (n > 1) {
        Curve(fill, n, pt, x0, W, y1, H);
        pt[n].x = pt[n - 1].x; pt[n].y = y1;
        pt[n + 1].x = pt[0].x; pt[n + 1].y = y1;
        SelectObject(c->dc, GetStockObject(NULL_PEN));
        Polygon(c->dc, pt, n + 2);
        SelectObject(c->dc, GetStockObject(DC_PEN));
        for (int pass = 0; pass < 2; pass++) {
            if (pass == (l2Top ? 0 : 1)) { Curve(fill, n, pt, x0, W, y1, H); SetDCPenColor(c->dc, c->cMain); }
            else if (l2) { Curve(l2, n, pt, x0, W, y1, H); SetDCPenColor(c->dc, c2); }
            else continue;
            Polyline(c->dc, pt, n);
        }
    }
    SelectObject(c->dc, oldPen); SelectObject(c->dc, oldBr);
    RECT t = {x0 + SC(3), y0, c->R - SC(3), y1};
    SetTextColor(c->dc, c->cDim);
    DrawTextW(c->dc, topLabel, -1, &t, DT_SINGLELINE | DT_TOP | DT_RIGHT);
    DrawTextW(c->dc, L"5 мин", -1, &t, DT_SINGLELINE | DT_BOTTOM | DT_LEFT);
    c->row += GRAPH_ROWS;
}

static int g1[HISTN], g2[HISTN];                         // graph scratch
static int Pct(const HIST *h, int *o) { for (int i = 0; i < h->n; i++) o[i] = HistGet(h, i) * 10; return h->n; }

static void PaintTop(PCTX *c, int cpu) {                 // generic NtQSI top list (CPU, total I/O or private WS)
    WCHAR s[32];
    const STLIST *l = &stTop[popMode == ID_CPU ? TM_CPU : popMode == ID_MEM ? TM_MEM : TM_IO];
    if (!l->n) Text(c, c->L, c->R, haveBase ? L"Всё спокойно" : L"Измеряю…"   /* only before the first tick */, c->cDim, DT_LEFT);
    LONGLONG full = cpu ? 1000 : l->n ? l->e[0].v : 1; if (!full) full = 1;
    for (int i = 0; i < l->n; i++, c->row++) {
        const STICKY *e = &l->e[i]; COLORREF ct = e->live ? c->cText : C_GHOST;
        int bw = (int)(e->v * (c->R - c->L) / full); if (bw < 1) bw = 1; if (bw > c->R - c->L) bw = c->R - c->L;
        BarB(c, c->L, bw, 3, e->live ? c->bar : c->ghost);
        Text(c, c->L + SC(4), c->R - SC(70), e->name, ct, DT_LEFT);
        if (cpu) wsprintfW(s, L"%d.%d%%", (int)(e->v / 10), (int)(e->v % 10));
        else if (popMode == ID_MEM) FmtMem(s, e->v);
        else FmtRate(s, e->v);
        Text(c, c->L, c->R - SC(4), s, ct, DT_RIGHT);
    }
}

static void Line(PCTX *c, const WCHAR *label, const WCHAR *val) {
    Text(c, c->L + SC(8), c->R, label, c->cDim, DT_LEFT);
    Text(c, c->L, c->R, val, c->cText, DT_RIGHT); c->row++;
}

static void PaintMem(PCTX *c) {
    WCHAR s[96], a[32], b[32];
    FmtMem(a, mem.total - mem.avail); FmtMem(b, mem.total);
    wsprintfW(s, L"Память %d%%  •  %s из %s", mem.load, a, b);
    Text(c, c->L, c->R, s, c->cMain, DT_LEFT); c->row++;
    PaintGraph(c, g1, 0, Pct(&hMem, g1), 0, 0, L"100%");
    int y = SC(5) + ROWH * c->row, W = c->R - c->L;     // usage bar
    RECT o = {c->L, y + SC(5), c->R, y + ROWH - SC(5)};
    FrameRect(c->dc, &o, c->bar);
    Bar(c, c->L, W * mem.load / 100, 5); c->row++;
    FmtMem(a, mem.avail); Line(c, L"Доступно", a);
    if (mem.cache) { FmtMem(a, mem.cache); Line(c, L"Кэш", a); }
    FmtMem(a, mem.commit); FmtMem(b, mem.commitLimit);
    wsprintfW(s, L"%s из %s", a, b); Line(c, L"Выделено (commit)", s);
    Text(c, c->L, c->R, L"Процессы", c->cMain, DT_LEFT); c->row++;
    PaintTop(c, 0);
}

static void PaintCpu(PCTX *c) {
    WCHAR s[64];
    wsprintfW(s, L"CPU %d%%  •  %d ядер", AvgLoad(), ncpu);
    Text(c, c->L, c->R, s, c->cMain, DT_LEFT); c->row++;
    Pct(&hCpuMax, g2);                                   // peak core: thin light line
    PaintGraph(c, g1, g2, Pct(&hCpuAvg, g1), RGB(215, 255, 160), 0, L"100%");
    PaintTop(c, 1);
}

static void PaintVolume(PCTX *c, WCHAR letter) {
    WCHAR root[4] = {letter, ':', '\\', 0}, s[96], a[32], b[32]; ULARGE_INTEGER fr, tot;
    BOOL ok = GetDiskFreeSpaceExW(root, &fr, &tot, 0);
    if (ok) { FmtSize(a, fr.QuadPart); FmtSize(b, tot.QuadPart); wsprintfW(s, L"%c:  свободно %s из %s", letter, a, b); }
    else wsprintfW(s, L"%c:", letter);
    Text(c, c->L + SC(8), c->R, s, c->cDim, DT_LEFT);
    if (ok && tot.QuadPart) {                            // usage bar
        int bw = SC(60), used = (int)((tot.QuadPart - fr.QuadPart) * bw / tot.QuadPart);
        int y = SC(5) + ROWH * c->row;
        RECT o = {c->R - bw, y + SC(6), c->R, y + ROWH - SC(6)}, u = {c->R - bw, y + SC(6), c->R - bw + used, y + ROWH - SC(6)};
        FrameRect(c->dc, &o, c->bar); FillRect(c->dc, &u, c->bar);
    }
    c->row++;
}

static void PaintDiskProcs(PCTX *c, int i) {             // ETW per-disk top
    WCHAR s[32]; int x0 = c->L + SC(8);
    for (int k = 0; k < DTOPN; k++, c->row++) {
        const STLIST *l = &stDisk[i];
        if (k >= l->n) { if (!k) Text(c, x0, c->R, L"процессы: тишина", c->cDim, DT_LEFT); continue; }
        const STICKY *e = &l->e[k]; COLORREF ct = e->live ? c->cText : C_GHOST;
        LONGLONG full = l->e[0].v ? l->e[0].v : 1;
        int bw = (int)(e->v * (c->R - x0) / full); if (bw < 1) bw = 1; if (bw > c->R - x0) bw = c->R - x0;
        BarB(c, x0, bw, 4, e->live ? c->bar : c->ghost);
        Text(c, x0 + SC(4), c->R - SC(70), e->name, ct, DT_LEFT);
        FmtRate(s, e->v);
        Text(c, c->L, c->R - SC(4), s, ct, DT_RIGHT);
    }
}

static void PaintDisk(PCTX *c) {
    WCHAR s[160], a[32], b[32];
    for (int i = 0; i < ndisk; i++) {
        DISK *d = &disks[i];
        lstrcpynW(s, d->model[0] ? d->model : L"?", 100);
        if (d->letters[0]) {                                   // "  [C: D:]"
            int n = lstrlenW(s);
            s[n++] = ' '; s[n++] = ' '; s[n++] = '[';
            for (const WCHAR *lt = d->letters; *lt; lt++) {
                if (lt != d->letters) s[n++] = ' ';
                s[n++] = *lt; s[n++] = ':';
            }
            s[n++] = ']'; s[n] = 0;
        }
        Text(c, c->L, c->R - SC(45), s, c->cMain, DT_LEFT);
        wsprintfW(s, L"%d%%", d->busy);
        Text(c, c->L, c->R, s, c->cMain, DT_RIGHT); c->row++;
        FmtRate(a, d->rps); FmtRate(b, d->wps);
        wsprintfW(s, L"чтение %s   запись %s   очередь %lu", a, b, d->queue);
        Text(c, c->L + SC(8), c->R, s, c->cText, DT_LEFT); c->row++;
        for (WCHAR *lt = d->letters; *lt; lt++) PaintVolume(c, *lt);
        if (etwOn) PaintDiskProcs(c, i);
    }
    if (etwOn) return;
    Text(c, c->L, c->R, L"Процессы", c->cMain, DT_LEFT); c->row++;
    PaintTop(c, 0);
}

static void PaintNetGraph(PCTX *c) {                    // rx area + tx line, auto-scaled to window max
    int n = hNetRx.n; ULONG mx = 1024;
    for (int i = 0; i < n; i++) { ULONG a = HistGetL(&hNetRx, i), b = HistGetL(&hNetTx, i); if (a > mx) mx = a; if (b > mx) mx = b; }
    for (int i = 0; i < n; i++) { g1[i] = (int)((ULONGLONG)HistGetL(&hNetRx, i) * 1000 / mx); g2[i] = (int)((ULONGLONG)HistGetL(&hNetTx, i) * 1000 / mx); }
    WCHAR s[24]; FmtRate(s, mx);
    PaintGraph(c, g1, g2, n, C_TX, 1, s);
}

#define C_RX_TOT RGB(70, 130, 180)
#define C_TX_TOT RGB(180, 110, 40)

// Net process columns, right to left: Σ↑ | Σ↓ | ↑ rate | ↓ rate; name takes the rest.
static void NetCols(PCTX *c, int *x) {                   // x[0..4]: left edges of ↓rate, ↑rate, Σ↓, Σ↑, right end
    int R = c->R - SC(4), w = SC(85);
    x[4] = R; x[3] = R - w; x[2] = R - 2 * w; x[1] = R - 3 * w - SC(10); x[0] = R - 4 * w - SC(10);
}

static void NetRow(PCTX *c, const STICKY *e, LONGLONG full) {
    int x[5]; NetCols(c, x); WCHAR a[24], s[32];
    COLORREF ct = e->live ? c->cText : C_GHOST;
    int bw = (int)(e->v * (c->R - c->L) / full); if (bw < 1) bw = 1; if (bw > c->R - c->L) bw = c->R - c->L;
    BarB(c, c->L, bw, 3, e->live ? c->bar : c->ghost);
    Text(c, c->L + SC(4), x[0] - SC(6), e->name, ct, DT_LEFT);
    FmtRate(a, e->net.rx);   wsprintfW(s, L"↓ %s", a); Text(c, x[0], x[1] - SC(6), s, ct, DT_RIGHT);
    FmtRate(a, e->net.tx);   wsprintfW(s, L"↑ %s", a); Text(c, x[1], x[2] - SC(10), s, e->live ? C_TX : C_GHOST, DT_RIGHT);
    FmtBytes(a, e->net.totRx); wsprintfW(s, L"↓ %s", a); Text(c, x[2], x[3] - SC(6), s, e->live ? C_RX_TOT : C_GHOST, DT_RIGHT);
    FmtBytes(a, e->net.totTx); wsprintfW(s, L"↑ %s", a); Text(c, x[3], x[4], s, e->live ? C_TX_TOT : C_GHOST, DT_RIGHT);
    c->row++;
}

static void PaintNet(PCTX *c) {
    WCHAR s[160], a[32], b[32];
    if (!nnic) { Text(c, c->L, c->R, L"Нет активных адаптеров", c->cDim, DT_LEFT); c->row += 2; }
    for (int i = 0; i < nnic; i++) {
        NIC *n = &nics[i];
        Text(c, c->L, c->R - SC(170), n->name, c->cMain, DT_LEFT);
        RxTx(c, n->rps, n->tps, c->cMain, C_TX); c->row++;
        FmtMB(a, n->in0 - n->inStart); FmtMB(b, n->out0 - n->outStart);
        wsprintfW(s, L"с запуска: принято %s, отправлено %s", a, b);
        Text(c, c->L + SC(8), c->R, s, c->cDim, DT_LEFT); c->row++;
    }
    PaintNetGraph(c);
    if (!etwOn) return;                                  // no trace -> no process section
    int x[5]; NetCols(c, x);
    Text(c, c->L, x[0], L"Процессы", c->cMain, DT_LEFT);
    Text(c, x[0], x[2] - SC(10), L"скорость", c->cDim, DT_CENTER);
    Text(c, x[2], x[4], L"всего", c->cDim, DT_CENTER); c->row++;
    const STLIST *l = &stNet;
    if (!l->n) Text(c, c->L + SC(8), c->R, etwTicks ? L"тишина" : L"Измеряю…", c->cDim, DT_LEFT);
    LONGLONG full = l->n ? l->e[0].v : 1; if (!full) full = 1;
    for (int i = 0; i < l->n; i++) NetRow(c, &l->e[i], full);
}

static void Paint(HWND h) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
    RECT r; GetClientRect(h, &r);
    int cpu = popMode == ID_CPU, net = popMode == ID_NET, mm = popMode == ID_MEM;
    COLORREF cBar = cpu ? C_CPU_DARK : net ? C_RX_DARK : mm ? C_MEM_DARK : C_DSK_DARK;
    PCTX c = {dc, PAD, r.right - PAD, 0, cpu ? C_CPU : net ? C_RX : mm ? C_MEM : C_DSK, RGB(220, 255, 230), RGB(120, 160, 140), CreateSolidBrush(cBar), CreateSolidBrush(RGB(48, 56, 54))};
    HBRUSH bg = CreateSolidBrush(RGB(18, 26, 24)), frame = CreateSolidBrush(c.cMain);
    FillRect(dc, &r, bg); FrameRect(dc, &r, frame);
    SelectObject(dc, hFont); SetBkMode(dc, TRANSPARENT);
    if (cpu) PaintCpu(&c); else if (mm) PaintMem(&c); else if (net) PaintNet(&c); else PaintDisk(&c);
    DeleteObject(bg); DeleteObject(c.bar); DeleteObject(c.ghost); DeleteObject(frame);
    EndPaint(h, &ps);
}

LRESULT CALLBACK PopProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) { Paint(h); return 0; }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, w, l);
}
