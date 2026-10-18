// "Sticky" top lists: processes that drop out stay a few seconds as dimmed ghost rows showing their peak.
#include "sticky.h"
#include "etw.h"
#include "disk.h"

STLIST stTop[TM_N], stDisk[MAXD];

static void Update(STLIST *l, const STICKY *live, int nLive, int cap) {   // cap <= TOPN_MAX
    static STICKY a[2 * TOPN_MAX]; int n = 0;                // static: keep the stack frame small
    for (int i = 0; i < nLive && i < cap; i++) {
        a[n] = live[i]; a[n].seen = tickNow; a[n].live = 1;
        a[n].peakV = a[n].v; n++;
    }
    for (int i = 0; i < l->n; i++) {                     // previous rows: carry peaks / become ghosts
        const STICKY *o = &l->e[i]; int dup = 0;
        for (int k = 0; k < nLive && k < cap; k++) if (a[k].pid == o->pid) {
            dup = 1;                                     // still live: carry its peak forward
            if (o->peakV > a[k].peakV) a[k].peakV = o->peakV;
            break;
        }
        if (dup || tickNow - o->seen >= GHOST_MS) continue;
        a[n] = *o; a[n].live = 0;                        // ghost shows the peak it had while live
        a[n].v = o->peakV; n++;
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

static void Row(STICKY *a, DWORD pid, const WCHAR *name, LONGLONG v) {
    ZeroMemory(a, sizeof(*a)); a->pid = pid; lstrcpynW(a->name, name, NAME_LEN); a->v = v;
}

void StickyTick() {
    static STICKY a[TOPN_MAX];
    for (int m = 0; m < TM_N; m++) {
        for (int i = 0; i < nTop[m]; i++) Row(&a[i], top[m][i].pid, top[m][i].name, top[m][i].v);
        Update(&stTop[m], a, nTop[m], TOPN);
    }
    for (int d = 0; d < ndisk; d++) {
        for (int i = 0; i < ndtop[d]; i++) Row(&a[i], dtop[d][i].pid, NameOf(dtop[d][i].pid), dtop[d][i].v);
        Update(&stDisk[d], a, ndtop[d], DTOPN);
    }
}
