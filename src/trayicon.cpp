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
#include "format.h"
#include "settings.h"

static Settings cfg = {1, 1, 1, 1, 1000};              // which icons are enabled (iliz_resmon.ini)

static NOTIFYICONDATAW nidCpu, nidMem, nidDisk, nidNet;
static HICON icCpu, icMem, icDisk, icNet;

static int IconSize() { return GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem()); }

// Canvas helpers: color bitmap + empty mask -> HICON
typedef struct { int S; HDC sdc, dc; HBITMAP col, mask; HGDIOBJ old; } CANVAS;
static void Begin(CANVAS *c, COLORREF bg) {
    c->S = IconSize(); c->sdc = GetDC(0); c->dc = CreateCompatibleDC(c->sdc);
    static BYTE zero[32 * 256];                          // all-zero AND mask = fully opaque (max 256 px, .bss)
    if (c->S > 256) c->S = 256;
    c->col = CreateCompatibleBitmap(c->sdc, c->S, c->S); c->mask = CreateBitmap(c->S, c->S, 1, 1, zero);
    c->old = SelectObject(c->dc, c->col);
    RECT r = {0, 0, c->S, c->S}; HBRUSH b = CreateSolidBrush(bg);
    FillRect(c->dc, &r, b); DeleteObject(b);
}
static HICON End(CANVAS *c) {
    SelectObject(c->dc, c->old);
    ICONINFO ii = {TRUE, 0, 0, c->mask, c->col};
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(c->col); DeleteObject(c->mask); DeleteDC(c->dc); ReleaseDC(0, c->sdc);
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

static BOOL SetIcon(NOTIFYICONDATAW *nid, HICON *slot, HICON ic, DWORD msg) {
    nid->uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; nid->hIcon = ic;
    BOOL ok = Shell_NotifyIconW(msg, nid);
    if (*slot) DestroyIcon(*slot);
    *slot = ic;
    return ok;
}

static BOOL Update(DWORD msg) {                          // FALSE if any icon is missing in the tray
    BOOL ok = TRUE;
    if (cfg.cpu) {
    wsprintfW(nidCpu.szTip, L"CPU: %d%%", AvgLoad());
    ok &= SetIcon(&nidCpu, &icCpu, DrawBars(load, ncpu, C_CPU, C_CPU_DARK), msg);
    }
    if (cfg.mem) {
    wsprintfW(nidMem.szTip, T(S_TIP_MEM), mem.load);
    ok &= SetIcon(&nidMem, &icMem, DrawMem(), msg);
    }
    if (cfg.disk && ndisk) {
        int v[MAXD], mx = 0;
        for (int i = 0; i < ndisk; i++) { v[i] = disks[i].busy; if (v[i] > mx) mx = v[i]; }
        wsprintfW(nidDisk.szTip, T(S_TIP_DISK), mx);
        ok &= SetIcon(&nidDisk, &icDisk, DrawBars(v, ndisk, C_DSK, C_DSK_DARK), msg);
    }
    if (cfg.net) {
    LONGLONG rx, tx; WCHAR a[24], b[24];
    NetTotals(&rx, &tx); FmtRate(a, rx); FmtRate(b, tx);
    wsprintfW(nidNet.szTip, T(S_TIP_NET), a, b);
    ok &= SetIcon(&nidNet, &icNet, DrawNet(), msg);
    }
    return ok;
}

void InitTray() {
    NOTIFYICONDATAW *n[4] = {&nidCpu, &nidMem, &nidDisk, &nidNet};
    UINT ids[4] = {ID_CPU, ID_MEM, ID_DISK, ID_NET};
    for (int i = 0; i < 4; i++) {
        n[i]->cbSize = sizeof(NOTIFYICONDATAW); n[i]->hWnd = hMain; n[i]->uID = ids[i];
        n[i]->uCallbackMessage = WM_TRAY; n[i]->uVersion = NOTIFYICON_VERSION_4;
    }
}

void UpdateIcons() { if (!Update(NIM_MODIFY)) AddIcons(); }   // tray not ready at logon / Explorer gone

void AddIcons() {
    Update(NIM_ADD);
    if (cfg.net) Shell_NotifyIconW(NIM_SETVERSION, &nidNet);
    if (cfg.cpu) Shell_NotifyIconW(NIM_SETVERSION, &nidCpu);
    if (cfg.mem) Shell_NotifyIconW(NIM_SETVERSION, &nidMem);
    if (cfg.disk && ndisk) Shell_NotifyIconW(NIM_SETVERSION, &nidDisk);
}

void RemoveIcons() {                                     // deleting an icon that is not there is harmless
    Shell_NotifyIconW(NIM_DELETE, &nidCpu);
    Shell_NotifyIconW(NIM_DELETE, &nidNet);
    Shell_NotifyIconW(NIM_DELETE, &nidMem);
    Shell_NotifyIconW(NIM_DELETE, &nidDisk);
}

static void Load() {                                     // never end up with no icon at all: the menu lives on them
    LoadSettings(&cfg);
    if (!cfg.cpu && !cfg.mem && !cfg.net && !(cfg.disk && ndisk)) cfg.cpu = 1;
}

void LoadTraySettings() { Load(); }

int TrayInterval() { return cfg.interval; }

void ReloadTraySettings() {                              // configurator changed the ini: re-add only enabled icons
    RemoveIcons();
    Load();
    AddIcons();
}
