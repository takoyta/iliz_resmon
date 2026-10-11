// 5-minute history: CPU avg / max-core, RAM used %, network rx/tx bytes/s.
#include "common.h"
#include "history.h"
#include "cpu.h"
#include "mem.h"
#include "net.h"

HIST hCpuAvg, hCpuMax, hMem;
HISTL hNetRx, hNetTx;

static void Push(HIST *h, int v) {
    h->v[h->pos] = (BYTE)(v < 0 ? 0 : v > 100 ? 100 : v);
    h->pos = (h->pos + 1) % HISTN;
    if (h->n < HISTN) h->n++;
}

static void PushL(HISTL *h, LONGLONG v) {
    h->v[h->pos] = (ULONG)(v < 0 ? 0 : v > 0xFFFFFFFF ? 0xFFFFFFFF : v);
    h->pos = (h->pos + 1) % HISTN;
    if (h->n < HISTN) h->n++;
}

ULONG HistGetL(const HISTL *h, int i) { return h->v[(h->pos - h->n + i + HISTN) % HISTN]; }
ULONGLONG HistSumL(const HISTL *h) { ULONGLONG s = 0; for (int i = 0; i < h->n; i++) s += h->v[i]; return s; }

int HistGet(const HIST *h, int i) { return h->v[(h->pos - h->n + i + HISTN) % HISTN]; }

void PushHistory() {
    int mx = 0;
    for (int i = 0; i < ncpu; i++) if (load[i] > mx) mx = load[i];
    Push(&hCpuAvg, AvgLoad()); Push(&hCpuMax, mx); Push(&hMem, mem.load);
    LONGLONG rx, tx; NetTotals(&rx, &tx); PushL(&hNetRx, rx); PushL(&hNetTx, tx);
}
