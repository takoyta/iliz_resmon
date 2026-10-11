#pragma once
#include "common.h"
#include "procs.h"
#define GHOST_MS 5000                      // how long a dropped-out process stays visible
typedef struct {
    DWORD pid; WCHAR name[NAME_LEN];
    LONGLONG v, rx, tx;                    // sort value (rate / %·10 / bytes); net rates
    LONGLONG peakV, peakRx, peakTx;        // max while live; shown by ghost rows
    ULONGLONG totRx, totTx;                // net totals since start
    ULONGLONG seen; int live;              // last tick seen live
} STICKY;
typedef struct { STICKY e[TOPN]; int n; } STLIST;  // live + ghost rows sorted by value; ghosts dropped first on overflow
extern STLIST stTop[TM_N], stNet, stDisk[MAXD];   // per-metric process lists, network, per-disk ETW
void StickyTick();                         // every tick, after SampleProcs/EtwTick (ghosts survive popup close)
