#pragma once
#include "common.h"
#include "procs.h"
#define GHOST_MS 5000                      // how long a dropped-out process stays visible
typedef struct {
    DWORD pid; WCHAR name[NAME_LEN];
    LONGLONG v;                            // sort value (rate / %·10 / bytes)
    LONGLONG peakV;                        // max v while live; shown by ghost rows
    ULONGLONG seen; int live;              // last tick seen live
} STICKY;
typedef struct { STICKY e[TOPN_MAX]; int n; } STLIST;  // live + ghost rows sorted by value; ghosts dropped first on overflow
extern STLIST stTop[TM_N];                 // per-metric process lists
void StickyTick();                         // every tick, after SampleProcs (ghosts survive popup close)
