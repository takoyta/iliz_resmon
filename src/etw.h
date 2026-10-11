#pragma once
#include "disk.h"
#define DTOPN 4
typedef struct { DWORD pid; LONGLONG v; } DTOP;          // per-disk top, bytes/s
typedef struct { DWORD pid; LONGLONG rx, tx; ULONGLONG totRx, totTx; } NTOP;   // rates bytes/s, totals since start
extern int etwOn, etwTicks;                // etwTicks: ticks folded since the session started
extern ULONG etwErr;
extern DTOP dtop[MAXD][DTOPN]; extern int ndtop[MAXD];
extern NTOP ntop[TOPN]; extern int nNtop;
void EtwInit();
void EtwStart();                           // at startup; EtwTick retries every 15 s while down, health check every 10 s
void EtwStop();                            // on exit
void EtwTick();                            // every timer tick: per-disk tops, net tops + cumulative totals
