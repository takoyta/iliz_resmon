// Popup views: CPU, memory, disks, network.
#include "views.h"
#include "lang.h"
#include "cpu.h"
#include "mem.h"
#include "disk.h"
#include "net.h"
#include "gpu.h"
#include "etw.h"
#include "procs.h"
#include "sticky.h"
#include "history.h"
#include "format.h"

#define COL_VAL SC(70)                     // value column of process rows
enum { F_PCT10, F_BYTES, F_RATE };         // value formats

static int g1[HISTN], g2[HISTN];           // graph scratch
static int Last() { return HistLen(); }   // popup graphs: last HISTN seconds
static int Pct(int m, int *o) { int n = Last(), b = HistLen() - n; for (int i = 0; i < n; i++) o[i] = HistPct(m, b + i) * 10; return n; }

int ViewWidth(int mode) { return mode == ID_GPU ? SC(320) : mode == ID_NET ? SC(520) : mode == ID_DISK ? SC(330) : mode == ID_MEM ? SC(300) : SC(240); }

int ViewRows(int mode) {
    if (mode == ID_CPU) return 1 + GRAPH_ROWS + TOPN;
    if (mode == ID_GPU) return 2 + GRAPH_ROWS + GPU_ROWS + 1 + VPROC_ROWS;
    if (mode == ID_MEM) return 5 + GRAPH_ROWS + (mem.cache ? 1 : 0) + TOPN;
    if (mode == ID_NET) return 2 * (nnic ? nnic : 1) + GRAPH_ROWS + (etwOn ? 2 + TOPN : 1);
    int r = 0;
    for (int i = 0; i < ndisk; i++) r += 2 + lstrlenW(disks[i].letters) + (etwOn ? DTOPN : 0);
    return etwOn ? r : r + 1 + TOPN;
}

void ViewColors(int mode, COLORREF *cMain, COLORREF *cBar) {
    if (mode == ID_GPU) { *cMain = C_GPU; *cBar = C_GPU_DARK; return; }
    *cMain = mode == ID_CPU ? C_CPU : mode == ID_NET ? C_RX : mode == ID_MEM ? C_MEM : C_DSK;
    *cBar  = mode == ID_CPU ? C_CPU_DARK : mode == ID_NET ? C_RX_DARK : mode == ID_MEM ? C_MEM_DARK : C_DSK_DARK;
}

// Generic process list (snapshot-based: CPU, private WS, total I/O).
static void PaintTop(PCTX *c, const STLIST *l, int fmt) {
    WCHAR s[32];
    if (!l->n) Text(c, c->L, c->R, haveBase ? T(S_QUIET) : T(S_MEASURING), c->cDim, DT_LEFT);   // "Измеряю" only before the first tick
    LONGLONG full = fmt == F_PCT10 ? 1000 : l->n ? l->e[0].v : 1;
    for (int i = 0; i < l->n && i < TOPN; i++, c->row++) {   // n may exceed TOPN until the tick after a reload
        const STICKY *e = &l->e[i];
        COLORREF ct = ListRow(c, c->L, c->R - COL_VAL, e->name, e->v, full, 3, e->live);
        if (fmt == F_PCT10) wsprintfW(s, L"%d.%d%%", (int)(e->v / 10), (int)(e->v % 10));
        else if (fmt == F_BYTES) FmtBytes(s, e->v);
        else FmtRate(s, e->v);
        Text(c, c->L, c->R - SC(4), s, ct, DT_RIGHT);
    }
}

static void Line(PCTX *c, const WCHAR *label, const WCHAR *val) {
    Text(c, c->L + SC(8), c->R, label, c->cDim, DT_LEFT);
    Text(c, c->L, c->R, val, c->cText, DT_RIGHT); c->row++;
}

static void PaintCpu(PCTX *c) {
    WCHAR s[64];
    wsprintfW(s, T(S_CPU_HDR), AvgLoad(), ncpu);
    Text(c, c->L, c->R, s, c->cMain, DT_LEFT); c->row++;
    Pct(H_CPUMAX, g2);
    PaintGraph(c, g1, g2, Pct(H_CPU, g1), C_CPU_PEAK, 0, L"100%");
    PaintTop(c, &stTop[TM_CPU], F_PCT10);
}

static void PaintMem(PCTX *c) {
    WCHAR s[96], a[32], b[32];
    FmtBytes(a, mem.total - mem.avail); FmtBytes(b, mem.total);
    wsprintfW(s, T(S_MEM_HDR), mem.load, a, b);
    Text(c, c->L, c->R, s, c->cMain, DT_LEFT); c->row++;
    PaintGraph(c, g1, 0, Pct(H_MEM, g1), 0, 0, L"100%");
    int y = RowY(c);                                     // usage bar
    RECT o = {c->L, y + SC(5), c->R, y + ROWH - SC(5)};
    FrameRect(c->dc, &o, c->bar);
    Bar(c, c->L, (c->R - c->L) * mem.load / 100, 5, c->bar); c->row++;
    FmtBytes(a, mem.avail); Line(c, T(S_AVAIL), a);
    if (mem.cache) { FmtBytes(a, mem.cache); Line(c, T(S_CACHE), a); }
    FmtBytes(a, mem.commit); FmtBytes(b, mem.commitLimit);
    wsprintfW(s, T(S_OF), a, b); Line(c, T(S_COMMIT), s);
    Text(c, c->L, c->R, T(S_PROCS), c->cMain, DT_LEFT); c->row++;
    PaintTop(c, &stTop[TM_MEM], F_BYTES);
}

static void PaintVolume(PCTX *c, WCHAR letter) {
    WCHAR root[4] = {letter, ':', '\\', 0}, s[96], a[32], b[32]; ULARGE_INTEGER fr, tot;
    BOOL ok = GetDiskFreeSpaceExW(root, &fr, &tot, 0);
    if (ok) { FmtSize(a, fr.QuadPart); FmtSize(b, tot.QuadPart); wsprintfW(s, T(S_VOL), letter, a, b); }
    else wsprintfW(s, L"%c:", letter);
    Text(c, c->L + SC(8), c->R, s, c->cDim, DT_LEFT);
    if (ok && tot.QuadPart) {                            // usage bar
        int bw = SC(60), used = (int)((tot.QuadPart - fr.QuadPart) * bw / tot.QuadPart), y = RowY(c);
        RECT o = {c->R - bw, y + SC(6), c->R, y + ROWH - SC(6)}, u = {c->R - bw, y + SC(6), c->R - bw + used, y + ROWH - SC(6)};
        FrameRect(c->dc, &o, c->bar); FillRect(c->dc, &u, c->bar);
    }
    c->row++;
}

static void PaintDiskProcs(PCTX *c, const STLIST *l) {  // ETW per-disk list, DTOPN rows reserved
    WCHAR s[32]; int x0 = c->L + SC(8);
    for (int k = 0; k < DTOPN; k++, c->row++) {
        if (k >= l->n) { if (!k) Text(c, x0, c->R, T(S_DPROC_QUIET), c->cDim, DT_LEFT); continue; }
        const STICKY *e = &l->e[k];
        COLORREF ct = ListRow(c, x0, c->R - COL_VAL, e->name, e->v, l->e[0].v, 4, e->live);
        FmtRate(s, e->v);
        Text(c, c->L, c->R - SC(4), s, ct, DT_RIGHT);
    }
}

static void PaintDisk(PCTX *c) {
    WCHAR s[160], a[32], b[32];
    for (int i = 0; i < ndisk; i++) {
        DISK *d = &disks[i];
        lstrcpynW(s, d->model[0] ? d->model : L"?", 100);
        if (d->letters[0]) {                             // "  [C: D:]"
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
        wsprintfW(s, T(S_DISK_LINE), a, b, d->queue);
        Text(c, c->L + SC(8), c->R, s, c->cText, DT_LEFT); c->row++;
        for (WCHAR *lt = d->letters; *lt; lt++) PaintVolume(c, *lt);
        if (etwOn) PaintDiskProcs(c, &stDisk[i]);
    }
    if (etwOn) return;
    // Fallback while the trace is down (e.g. blocked by an anti-cheat): all process I/O incl. network.
    Text(c, c->L, c->R, T(S_PROCS), c->cMain, DT_LEFT); c->row++;
    PaintTop(c, &stTop[TM_IO], F_RATE);
}

static void PaintNetGraph(PCTX *c) {                    // rx area + tx line, auto-scaled to window max
    int n = Last(), b = HistLen() - n; ULONG mx = 1024;
    for (int i = 0; i < n; i++) { ULONG x = HistNet(H_RX, b + i), y = HistNet(H_TX, b + i); if (x > mx) mx = x; if (y > mx) mx = y; }
    for (int i = 0; i < n; i++) { g1[i] = (int)((ULONGLONG)HistNet(H_RX, b + i) * 1000 / mx); g2[i] = (int)((ULONGLONG)HistNet(H_TX, b + i) * 1000 / mx); }
    WCHAR s[24]; FmtRate(s, mx);
    PaintGraph(c, g1, g2, n, C_TX, 1, s);
}

// Net process columns, right to left: Σ↑ | Σ↓ | ↑ rate | ↓ rate; name takes the rest.
static void NetCols(PCTX *c, int *x) {                   // x[0..4]: left edges of ↓rate, ↑rate, Σ↓, Σ↑, right end
    int R = c->R - SC(4), w = SC(85);
    x[4] = R; x[3] = R - w; x[2] = R - 2 * w; x[1] = R - 3 * w - SC(10); x[0] = R - 4 * w - SC(10);
}

static void NetRow(PCTX *c, const NTOP *e, LONGLONG full) {   // bar = total rx relative to the top row
    int x[5]; NetCols(c, x); WCHAR a[24], s[32];
    COLORREF ct = ListRow(c, c->L, x[0] - SC(6), e->name, (LONGLONG)e->totRx, full, 3, e->live);
    FmtRate(a, e->rx);    wsprintfW(s, L"↓ %s", a); Text(c, x[0], x[1] - SC(6), s, ct, DT_RIGHT);
    FmtRate(a, e->tx);    wsprintfW(s, L"↑ %s", a); Text(c, x[1], x[2] - SC(10), s, e->live ? C_TX : C_GHOST, DT_RIGHT);
    FmtBytes(a, e->totRx); wsprintfW(s, L"↓ %s", a); Text(c, x[2], x[3] - SC(6), s, e->live ? C_RX_TOT : C_GHOST, DT_RIGHT);
    FmtBytes(a, e->totTx); wsprintfW(s, L"↑ %s", a); Text(c, x[3], x[4], s, e->live ? C_TX_TOT : C_GHOST, DT_RIGHT);
    c->row++;
}

static void PaintNet(PCTX *c) {
    WCHAR s[160], a[32], b[32];
    if (!nnic) { Text(c, c->L, c->R, T(S_NO_NIC), c->cDim, DT_LEFT); c->row += 2; }
    for (int i = 0; i < nnic; i++) {
        NIC *n = &nics[i];
        Text(c, c->L, c->R - SC(170), n->name, c->cMain, DT_LEFT);
        RxTx(c, n->rps, n->tps, c->cMain, C_TX); c->row++;
        FmtBytes(a, n->in0 - n->inStart); FmtBytes(b, n->out0 - n->outStart);
        wsprintfW(s, T(S_NIC_TOT), a, b);
        Text(c, c->L + SC(8), c->R, s, c->cDim, DT_LEFT); c->row++;
    }
    PaintNetGraph(c);
    if (!etwOn) {                                        // no trace (not elevated / blocked): adapter totals only
        Text(c, c->L, c->R, T(EtwNote()), c->cDim, DT_LEFT); c->row++;
        return;
    }
    int x[5]; NetCols(c, x);
    Text(c, c->L, x[0], T(S_PROCS), c->cMain, DT_LEFT);
    Text(c, x[0], x[2] - SC(10), T(S_SPEED), c->cDim, DT_CENTER);
    Text(c, x[2], x[4], T(S_TOTAL), c->cDim, DT_CENTER); c->row++;
    if (!nNtop) Text(c, c->L + SC(8), c->R, etwTicks ? T(S_SILENCE) : T(S_MEASURING), c->cDim, DT_LEFT);
    LONGLONG full = nNtop ? (LONGLONG)ntop[0].totRx : 1;
    for (int i = 0; i < nNtop; i++) NetRow(c, &ntop[i], full);
}

static void PaintGpu(PCTX *c) {                         // load, VRAM, graph (load + VRAM line), engine types, VRAM processes
    WCHAR s[96], a[24], b[24];
    wsprintfW(s, T(S_GPU_HDR), gpuLoad);
    Text(c, c->L, c->R, s, c->cMain, DT_LEFT); c->row++;
    const GADAPTER *ad = gpuAd >= 0 && gad[gpuAd].total ? &gad[gpuAd] : 0;
    if (ad) { FmtBytes(a, ad->used); FmtBytes(b, ad->total); wsprintfW(s, T(ad->shared ? S_VRAM_LINE_SH : S_VRAM_LINE), a, b, vramPct); }
    else lstrcpyW(s, T(S_VRAM_NA));
    Text(c, c->L, c->R, s, ad ? C_VRAM : c->cDim, DT_LEFT); c->row++;
    int g0 = c->row;
    Pct(H_VRAM, g2);
    PaintGraph(c, g1, g2, Pct(H_GPU, g1), C_VRAM, 1, L"100%");
    int g9 = c->row; c->row = g0;                        // legend in the graph's top-left corner
    SIZE sz; GetTextExtentPoint32W(c->dc, T(S_LEG_LOAD), lstrlenW(T(S_LEG_LOAD)), &sz);
    Text(c, c->L + SC(4), c->R, T(S_LEG_LOAD), c->cMain, DT_LEFT);
    Text(c, c->L + SC(14) + sz.cx, c->R, T(S_LEG_VRAM), C_VRAM, DT_LEFT);
    c->row = g9;
    if (!gpuOk || !ngeng) { Text(c, c->L, c->R, gpuOk ? T(S_MEASURING) : T(S_GPU_NONE), c->cDim, DT_LEFT); return; }
    int idx[MAXG], n = 0;
    for (int i = 0; i < ngeng; i++) {                    // sort by load desc (stable: first seen wins ties)
        int k = n++;
        while (k > 0 && geng[idx[k - 1]].v < geng[i].v) { idx[k] = idx[k - 1]; k--; }
        idx[k] = i;
    }
    for (int r = 0; r < n && r < GPU_ROWS; r++, c->row++) {
        const GENG *e = &geng[idx[r]];
        if (nGpuAd > 1) wsprintfW(s, L"GPU%d %s", e->ad, e->type); else lstrcpyW(s, e->type);
        ListRow(c, c->L, c->R - COL_VAL, s, e->v, 100, 3, 1);
        wsprintfW(s, L"%d%%", e->v);
        Text(c, c->L, c->R - SC(4), s, c->cText, DT_RIGHT);
    }
    c->row = g9 + GPU_ROWS;                              // fixed slots: the list below doesn't move
    Text(c, c->L, c->R, T(S_VRAM_PROCS), C_VRAM, DT_LEFT); c->row++;
    if (!ngproc) Text(c, c->L + SC(8), c->R, ad ? T(S_MEASURING) : T(S_VRAM_NA), c->cDim, DT_LEFT);
    for (int r = 0; r < ngproc && r < VPROC_ROWS; r++, c->row++) {
        ListRow(c, c->L, c->R - COL_VAL, gproc[r].name, (LONGLONG)gproc[r].bytes, (LONGLONG)gproc[0].bytes, 3, 1);
        FmtBytes(s, gproc[r].bytes);
        Text(c, c->L, c->R - SC(4), s, c->cText, DT_RIGHT);
    }
}

void ViewPaint(PCTX *c, int mode) {
    if (mode == ID_GPU) PaintGpu(c);
    else if (mode == ID_CPU) PaintCpu(c);
    else if (mode == ID_MEM) PaintMem(c);
    else if (mode == ID_NET) PaintNet(c);
    else PaintDisk(c);
}
