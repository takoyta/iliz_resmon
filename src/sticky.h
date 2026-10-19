#pragma once
#include "common.h"
#include "procs.h"
#define GHOST_MS 5000                      // how long a dropped-out process stays visible
typedef struct {
    DWORD pid; WCHAR name[NAME_LEN];
    LONGLONG v;                            // sort value (rate / %·10 / bytes)
    LONGLONG r, w;                         // per-disk lists: read / write part of v (bytes/s), else 0
    LONGLONG peakV, peakR, peakW;          // max v while live (with its r/w); shown by ghost rows
    ULONGLONG seen; int live;              // last tick seen live
} STICKY;
typedef struct { STICKY e[TOPN_MAX]; int n; } STLIST;  // live + ghost rows sorted by value; ghosts dropped first on overflow
extern STLIST stTop[TM_N], stDisk[MAXD];   // per-metric process lists, per-disk (trace)
void StickyTick();                         // every tick, after SampleProcs/EtwTick (ghosts survive popup close)
void StickyResetDisks();                   // disks re-enumerated: indexes changed
