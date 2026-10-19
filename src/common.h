// Shared constants and app-wide state (window handles, popup mode, tick clock).
#pragma once
#define WIN32_LEAN_AND_MEAN
#include "appinfo.h"
#include <windows.h>

#define WM_TRAY (WM_APP + 1)
#define TOPN_MIN 3                         // process list length: [tray] top_n, 3..15, default 10
#define TOPN_MAX 15
#define TOPN topN                          // rows of the main process lists (CPU, memory, I/O, network)
extern int topN;
#define MAXD 16                            // physical disks
#define NAME_LEN 32                        // process name buffer (incl. 0)
enum { ID_CPU = 1, ID_DISK, ID_NET, ID_MEM, ID_GPU };   // tray icon ids = popup modes

typedef LONG (WINAPI *NtQSI_t)(int, PVOID, ULONG, PULONG);
extern NtQSI_t NtQSI;
extern HWND hMain, hPop;
extern int popMode;                        // 0 hidden, else ID_* of the open popup
extern ULONGLONG tickPrev, tickNow;

// Sorted top-N insert into a[0..*n) (best first, at most cap): after(x) is true when x ranks below the new item.
// Lower rows shift down; returns the slot for the new item, or -1 when it does not make the list.
template <class T, class F> inline int TopPush(T *a, int *n, int cap, F after) {
    int k = *n < cap ? (*n)++ : cap;
    while (k > 0 && after(a[k - 1])) { if (k < cap) a[k] = a[k - 1]; k--; }
    return k < cap ? k : -1;
}

inline LONGLONG TickDt() { ULONGLONG d = tickNow - tickPrev; return d ? (LONGLONG)d : 1000; }   // ms
