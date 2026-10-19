// Per-process cumulative counters for today (two byte counters a/b, e.g. rx/tx or read/write):
// totals reset at local 00:00, kept for exited processes (with their last known name), current rates,
// and a top list for the popups. One PTAB per source (network trace, ...).
#pragma once
#include "common.h"
#define PT_IDLE_MS 5000                    // a row is "live" with traffic in the last 5 s (any interval)
typedef struct {
    DWORD pid; WCHAR name[NAME_LEN];
    LONGLONG ra, rb;                       // bytes/s this tick (0 if idle)
    ULONGLONG ta, tb;                      // today
    int live;
} PTROW;
typedef struct PTENT_ PTENT;
typedef struct {
    PTENT *e; int cap, n;                  // open-addressing table, cap = power of 2 (heap, PtInit)
    int sumSort;                           // top list order: 0 = by ta then tb, 1 = by ta + tb
    WORD day;                              // local day of the totals
    PTROW row[TOPN_MAX]; int nrow;         // top list (PtFinish)
} PTAB;
extern PTAB ptNet;                         // ETW TCP/UDP: a = received, b = sent
void PtInit();                             // allocate the tables
void PtAdd(PTAB *t, DWORD pid, LONGLONG a, LONGLONG b);   // bytes moved this tick by pid
void PtFinish(PTAB *t);                    // after SampleProcs: names, rates, top TOPN rows
