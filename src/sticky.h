#pragma once
#include "etw.h"
#include "procs.h"
#define GHOST_MS 5000                      // how long a dropped-out process stays visible
typedef struct { DWORD pid; WCHAR name[40]; LONGLONG v; NTOP net; ULONGLONG seen; int live; LONGLONG peakV, peakRx, peakTx; } STICKY;
typedef struct { STICKY e[TOPN]; int n; } STLIST;  // live + ghost rows sorted by value; ghosts dropped first on overflow
extern STLIST stTop[TM_N], stNet, stDisk[MAXD];   // per-metric process lists, network, per-disk ETW
void StickyUpdate(STLIST *l, const STICKY *live, int nLive, int cap);   // cap <= TOPN
void StickyTick();                         // every tick, after SampleProcs/EtwTick (ghosts survive popup close)
