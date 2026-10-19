// Tray icons: bar drawing and Shell_NotifyIcon (NOTIFYICON_VERSION_4).
#include "common.h"
#include "lang.h"
#include <shellapi.h>
#include "theme.h"
#include "trayicon.h"
#include "cpu.h"
#include "disk.h"
#include "net.h"
#include "mem.h"
#include "gpu.h"
#include "etw.h"
#include "format.h"
#include "settings.h"

static Settings cfg;                                     // which icons are enabled etc. (iliz_resmon.ini, LoadSettings)

static int trayDpi;                                      // DPI of the taskbar (TrayDpiChanged)
static int IconSize() { if (!trayDpi) TrayDpiChanged(); return GetSystemMetricsForDpi(SM_CXSMICON, trayDpi); }

void TrayDpiChanged() {                                  // taskbar monitor / scale changed: icons follow on the next draw
    HWND t = FindWindowW(L"Shell_TrayWnd", 0);
    UINT d = t ? GetDpiForWindow(t) : 0;
    trayDpi = d ? d : GetDpiForSystem();
}

// Canvas helpers: color bitmap + empty mask -> HICON. One memory DC and mask for the life of the process.
typedef struct { int S; HDC dc; HBITMAP col; HGDIOBJ old; } CANVAS;
static HDC memDC; static HBITMAP maskBmp; static int maskS;
static void Begin(CANVAS *c, COLORREF bg) {
    c->S = IconSize(); if (c->S > 256) c->S = 256;
    if (!memDC) memDC = CreateCompatibleDC(0);
    if (maskS != c->S) {                                 // all-zero AND mask = fully opaque (max 256 px, .bss)
        static BYTE zero[32 * 256];
        if (maskBmp) DeleteObject(maskBmp);
        maskBmp = CreateBitmap(c->S, c->S, 1, 1, zero); maskS = c->S;
    }
    HDC sdc = GetDC(0); c->col = CreateCompatibleBitmap(sdc, c->S, c->S); ReleaseDC(0, sdc);
    c->dc = memDC; c->old = SelectObject(c->dc, c->col);
    RECT r = {0, 0, c->S, c->S}; HBRUSH b = CreateSolidBrush(bg);
    FillRect(c->dc, &r, b); DeleteObject(b);
}
static HICON End(CANVAS *c) {
    SelectObject(c->dc, c->old);
    ICONINFO ii = {TRUE, 0, 0, maskBmp, c->col};
    HICON ic = CreateIconIndirect(&ii);                  // copies both bitmaps
    DeleteObject(c->col);
    return ic;
}

// Bars; when n exceeds what fits, neighbors are grouped: dark = group max, light = group avg.
static HICON DrawBars(const int *vals, int n, COLORREF cAvg, COLORREF cMax) {
    CANVAS c; Begin(&c, C_ICON_BG); int S = c.S;
    if (n < 1) n = 1;
    int maxBars = S / 2; if (maxBars < 1) maxBars = 1;   // bars >= 1px + 1px gap
    int per = (n + maxBars - 1) / maxBars, nb = (n + per - 1) / per;
    HBRUSH bMax = CreateSolidBrush(cMax), bAvg = CreateSolidBrush(cAvg);
    for (int b = 0; b < nb; b++) {
        int mx = 0, sum = 0, cnt = 0;
        for (int i = b * per; i < n && i < (b + 1) * per; i++) { if (vals[i] > mx) mx = vals[i]; sum += vals[i]; cnt++; }
        int avg = cnt ? sum / cnt : 0;
        int x0 = b * S / nb, x1 = (b + 1) * S / nb;
        if (nb > 1 && x1 - x0 > 1) x1--;
        int hm = (mx * S + 50) / 100, ha = (avg * S + 50) / 100;
        if (mx > 0 && hm < 1) hm = 1;
        if (avg > 0 && ha < 1) ha = 1;
        RECT rm = {x0, S - hm, x1, S}, ra = {x0, S - ha, x1, S};
        if (per > 1) FillRect(c.dc, &rm, bMax);
        FillRect(c.dc, per > 1 ? &ra : &rm, bAvg);
    }
    DeleteObject(bMax); DeleteObject(bAvg);
    return End(&c);
}

static HICON DrawNet() {                                 // two bars: rx blue, tx orange
    CANVAS c; Begin(&c, C_ICON_BG_NET); int S = c.S;
    int v[2] = {netRx > 100 ? 100 : netRx, netTx > 100 ? 100 : netTx};
    HBRUSH br[2] = {CreateSolidBrush(C_RX), CreateSolidBrush(C_TX)};
    for (int b = 0; b < 2; b++) {
        int hh = (v[b] * S + 50) / 100; if (v[b] > 0 && hh < 1) hh = 1;
        RECT r = {b * S / 2, S - hh, (b + 1) * S / 2 - 1, S};
        FillRect(c.dc, &r, br[b]);
    }
    DeleteObject(br[0]); DeleteObject(br[1]);
    return End(&c);
}

static HICON DrawMem() {                                 // one wide bar: used %
    CANVAS c; Begin(&c, C_ICON_BG); int S = c.S;
    int hh = (mem.load * S + 50) / 100; if (mem.load > 0 && hh < 1) hh = 1;
    RECT r = {S / 8, S - hh, S - S / 8, S};
    HBRUSH b = CreateSolidBrush(C_MEM); FillRect(c.dc, &r, b); DeleteObject(b);
    return End(&c);
}

// Scrolling graph, one column per tick, newest right: GPU load columns + video memory line on top.
static BYTE h[256], vm[256]; static int pos;             // rings of recent load / VRAM % (icon is <= 256 px)
static void GpuPush() {                                  // once per tick (not per draw: re-adds would scroll it)
    h[pos] = (BYTE)gpuLoad; vm[pos] = (BYTE)(gpuAd >= 0 ? vramPct : 255); pos = (pos + 1) & 255;   // 255: no VRAM data
}
static HICON DrawGpu() {
    CANVAS c; Begin(&c, C_ICON_BG); int S = c.S;
    int t = S / 12 < 1 ? 1 : S / 12;                     // line thickness: 1 px at 16, 2 px at 24..32
    HBRUSH b = CreateSolidBrush(C_GPU), bv = CreateSolidBrush(C_VRAM);
    int py = -1;
    for (int x = 0; x < S; x++) {
        int i = (pos - S + x + 512) & 255;
        int v = h[i], hh = (v * S + 50) / 100; if (v > 0 && hh < 1) hh = 1;
        RECT r = {x, S - hh, x + 1, S}; FillRect(c.dc, &r, b);
        if (vm[i] > 100) { py = -1; continue; }
        int y = S - t - (vm[i] * (S - t) + 50) / 100;   // top of the line, stays inside the icon
        int y0 = py < 0 || py > y ? y : py, y1 = (py < 0 || py < y ? y : py) + t;   // join steps vertically
        RECT l = {x, y0, x + 1, y1}; FillRect(c.dc, &l, bv);
        py = y;
    }
    DeleteObject(b); DeleteObject(bv);
    return End(&c);
}

static HICON DrawCpu() { return DrawBars(load, ncpu, C_CPU, C_CPU_DARK); }
static HICON DrawDisk() {
    int v[MAXD]; for (int i = 0; i < ndisk; i++) v[i] = disks[i].busy;
    return DrawBars(v, ndisk, C_DSK, C_DSK_DARK);
}

static void TipCpu(WCHAR *t) { wsprintfW(t, L"CPU: %d%%", AvgLoad()); }
static void TipMem(WCHAR *t) { wsprintfW(t, T(S_TIP_MEM), mem.load); }
static void TipDisk(WCHAR *t) {
    int mx = 0; for (int i = 0; i < ndisk; i++) if (disks[i].busy > mx) mx = disks[i].busy;
    wsprintfW(t, T(S_TIP_DISK), mx);
}
static void TipNet(WCHAR *t) {
    LONGLONG rx, tx; WCHAR a[24], b[24];
    NetTotals(&rx, &tx); FmtRate(a, rx); FmtRate(b, tx);
    wsprintfW(t, T(S_TIP_NET), a, b);
    if (EtwNote()) lstrcatW(t, T(EtwNote() == S_NOTE_ADMIN ? S_TIP_ADMIN : S_TIP_BLOCKED));
}
static void TipGpu(WCHAR *t) {
    wsprintfW(t, T(S_TIP_GPU), gpuLoad);
    if (gpuAd >= 0 && gad[gpuAd].total) wsprintfW(t + lstrlenW(t), T(S_TIP_VRAM), vramPct);
}

typedef struct { int id; int *flag; HICON (*draw)(); void (*tip)(WCHAR*); } TDEF;
static const TDEF defs[] = {                             // tray order on first add
    {ID_CPU,  &cfg.cpu,  DrawCpu,  TipCpu},
    {ID_MEM,  &cfg.mem,  DrawMem,  TipMem},
    {ID_DISK, &cfg.disk, DrawDisk, TipDisk},
    {ID_NET,  &cfg.net,  DrawNet,  TipNet},
    {ID_GPU,  &cfg.gpu,  DrawGpu,  TipGpu},
};
#define NICONS ((int)(sizeof(defs) / sizeof(defs[0])))
typedef struct { const TDEF *d; NOTIFYICONDATAW nid; HICON ic; int shown; } TICON;
static TICON icons[NICONS];                              // runtime state (.bss), d set in InitTray

static BOOL On(const TICON *t) { return *t->d->flag && (t->d->id != ID_DISK || ndisk); }

static BOOL Show(TICON *t, DWORD msg) {                  // draw + NIM_ADD / NIM_MODIFY
    t->d->tip(t->nid.szTip);
    HICON ic = t->d->draw();
    t->nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; t->nid.hIcon = ic;
    BOOL ok = Shell_NotifyIconW(msg, &t->nid);
    if (t->ic) DestroyIcon(t->ic);
    t->ic = ic;
    if (msg == NIM_ADD && ok) { Shell_NotifyIconW(NIM_SETVERSION, &t->nid); t->shown = 1; }
    return ok;
}

static void Hide(TICON *t) { Shell_NotifyIconW(NIM_DELETE, &t->nid); t->shown = 0; }

void InitTray() {
    for (int i = 0; i < NICONS; i++) {
        NOTIFYICONDATAW *n = &icons[i].nid; icons[i].d = &defs[i];
        n->cbSize = sizeof(NOTIFYICONDATAW); n->hWnd = hMain; n->uID = defs[i].id;
        n->uCallbackMessage = WM_TRAY; n->uVersion = NOTIFYICON_VERSION_4;
    }
}

void UpdateIcons() {                                     // a failed MODIFY: icon missing (tray not ready / Explorer gone)
    if (cfg.gpu) GpuPush();
    BOOL ok = TRUE;
    for (int i = 0; i < NICONS; i++) if (On(&icons[i])) ok &= Show(&icons[i], NIM_MODIFY);
    if (!ok) AddIcons();
}

void AddIcons() { for (int i = 0; i < NICONS; i++) if (On(&icons[i])) Show(&icons[i], NIM_ADD); }

void RemoveIcons() { for (int i = 0; i < NICONS; i++) Hide(&icons[i]); }   // deleting an absent icon is harmless

void TraySync() {                                        // add / remove only the icons whose state changed
    for (int i = 0; i < NICONS; i++) {
        TICON *t = &icons[i];
        if (t->shown && !On(t)) Hide(t);
        else if (!t->shown && On(t)) Show(t, NIM_ADD);
    }
}

static void Load() {                                     // never end up with no icon at all: the menu lives on them
    LoadSettings(&cfg);
    if (!cfg.cpu && !cfg.mem && !cfg.net && !cfg.gpu && !(cfg.disk && ndisk)) cfg.cpu = 1;
    GpuEnable(cfg.gpu);                                  // pdh.dll only while the GPU icon is shown
    topN = cfg.topn;                                     // popup heights follow (ViewRows)
}

void LoadTraySettings() { Load(); }

int TrayInterval() { return cfg.interval; }

void ReloadTraySettings() { Load(); TraySync(); }       // configurator changed the ini
