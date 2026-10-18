// Metric history: per-second ring buffers for 5 minutes (~2.7 KB), see history.h.
#include "common.h"
#include "history.h"
#include "cpu.h"
#include "mem.h"
#include "disk.h"
#include "net.h"
#include "gpu.h"

#define NS (H_PCT_N + H_NET_N)
static BYTE pct[H_PCT_N][HISTN];
static USHORT net[H_NET_N][HISTN];
static int hn, hpos;                       // stored count, next write slot
static LONGLONG acc[NS]; static int cnt;   // ticks of the current second
static ULONGLONG sec;                      // tickNow / 1000 of acc

static USHORT Enc(ULONG v) { int s = 0; while (v >= 2048) { v >>= 1; s++; } return (USHORT)(s << 11 | v); }
static ULONG Dec(USHORT c) { return (ULONG)(c & 2047) << (c >> 11); }

static void Store(const LONGLONG *v) {
    for (int m = 0; m < H_PCT_N; m++) pct[m][hpos] = (BYTE)(v[m] < 0 ? 0 : v[m] > 100 ? 100 : v[m]);
    for (int m = 0; m < H_NET_N; m++) { LONGLONG x = v[H_PCT_N + m]; net[m][hpos] = Enc((ULONG)(x < 0 ? 0 : x > 0xFFFFFFFF ? 0xFFFFFFFF : x)); }
    hpos = (hpos + 1) % HISTN;
    if (hn < HISTN) hn++;
}

void PushHistory() {
    ULONGLONG s = tickNow / 1000;
    if (cnt && s != sec) {                               // second over: store it (repeat for skipped seconds)
        LONGLONG v[NS], zero[NS] = {0};
        for (int i = 0; i < NS; i++) v[i] = i == H_CPUMAX ? acc[i] : acc[i] / cnt;
        ULONGLONG gap = s - sec; if (gap > HISTN) gap = HISTN;
        Store(v);
        for (ULONGLONG k = 1; k < gap; k++) Store(gap <= 5 ? v : zero);   // long gap = sleep: leave it empty
        ZeroMemory(acc, sizeof(acc)); cnt = 0;
    }
    sec = s;
    int mx = 0, dmx = 0;
    for (int i = 0; i < ncpu; i++) if (load[i] > mx) mx = load[i];
    for (int i = 0; i < ndisk; i++) if (disks[i].busy > dmx) dmx = disks[i].busy;
    LONGLONG rx, tx; NetTotals(&rx, &tx);
    acc[H_CPU] += AvgLoad(); if (mx > acc[H_CPUMAX]) acc[H_CPUMAX] = mx;
    acc[H_MEM] += mem.load; acc[H_DISK] += dmx; acc[H_GPU] += gpuLoad;
    acc[H_PCT_N + H_RX] += rx; acc[H_PCT_N + H_TX] += tx;
    cnt++;
}

int HistLen() { return hn; }
int HistPct(int m, int i) { return pct[m][(hpos - hn + i + HISTN) % HISTN]; }
ULONG HistNet(int m, int i) { return Dec(net[m][(hpos - hn + i + HISTN) % HISTN]); }
