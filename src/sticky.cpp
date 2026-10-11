// "Sticky" top lists: processes that drop out stay a few seconds as dimmed ghost rows.
#include "common.h"
#include "sticky.h"
#include "procs.h"

STLIST stTop[TM_N], stNet, stDisk[MAXD];

void StickyUpdate(STLIST *l, const STICKY *live, int nLive, int cap) {
    STICKY a[2 * TOPN]; int n = 0;
    for (int i = 0; i < nLive && i < cap; i++) {
        a[n] = live[i]; a[n].seen = tickNow; a[n].live = 1;
        a[n].peakV = a[n].v; a[n].peakRx = a[n].net.rx; a[n].peakTx = a[n].net.tx; n++;
    }
    for (int i = 0; i < l->n; i++) {                     // surviving ghosts keep their last value
        const STICKY *o = &l->e[i]; int dup = 0;
        for (int k = 0; k < nLive && k < cap; k++) if (a[k].pid == o->pid) {
            dup = 1;                                     // still live: carry its peak forward
            if (o->peakV > a[k].peakV) a[k].peakV = o->peakV;
            if (o->peakRx > a[k].peakRx) a[k].peakRx = o->peakRx;
            if (o->peakTx > a[k].peakTx) a[k].peakTx = o->peakTx;
            break;
        }
        if (dup || tickNow - o->seen >= GHOST_MS) continue;
        a[n] = *o; a[n].live = 0;                        // ghost shows the peak it had while live
        a[n].v = o->peakV; a[n].net.rx = o->peakRx; a[n].net.tx = o->peakTx; n++;
    }
    for (int i = 1; i < n; i++) {                        // insertion sort by value desc
        STICKY t = a[i]; int k = i;
        while (k > 0 && a[k - 1].v < t.v) { a[k] = a[k - 1]; k--; }
        a[k] = t;
    }
    while (n > cap) {                                    // drop lowest ghost first, then lowest live
        int d = n - 1;
        for (int i = n - 1; i >= 0; i--) if (!a[i].live) { d = i; break; }
        for (int i = d; i < n - 1; i++) a[i] = a[i + 1];
        n--;
    }
    CopyMemory(l->e, a, n * sizeof(STICKY)); l->n = n;
}

static void FromTop() {
    for (int m = 0; m < TM_N; m++) {
        STICKY a[TOPN]; const TOP *t = top[m];
        for (int i = 0; i < nTop[m]; i++) { ZeroMemory(&a[i], sizeof(STICKY)); a[i].pid = t[i].pid; lstrcpynW(a[i].name, t[i].name, 40); a[i].v = t[i].v; }
        StickyUpdate(&stTop[m], a, nTop[m], TOPN);
    }
}

static void FromNet() {
    STICKY a[TOPN];
    for (int i = 0; i < nNtop; i++) {
        ZeroMemory(&a[i], sizeof(STICKY)); a[i].pid = ntop[i].pid; a[i].net = ntop[i];
        lstrcpynW(a[i].name, NameOf(ntop[i].pid), 40); a[i].v = ntop[i].rx + ntop[i].tx;
    }
    StickyUpdate(&stNet, a, nNtop, TOPN);
}

static void FromDisk() {
    for (int d = 0; d < ndisk; d++) {
        STICKY a[DTOPN];
        for (int i = 0; i < ndtop[d]; i++) {
            ZeroMemory(&a[i], sizeof(STICKY)); a[i].pid = dtop[d][i].pid; a[i].v = dtop[d][i].v;
            lstrcpynW(a[i].name, NameOf(dtop[d][i].pid), 40);
        }
        StickyUpdate(&stDisk[d], a, ndtop[d], DTOPN);
    }
}

void StickyTick() { FromTop(); FromNet(); FromDisk(); }
