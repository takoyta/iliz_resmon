#pragma once
#include "disk.h"
#define DTOPN 4
typedef struct { DWORD pid; LONGLONG v; } DTOP;          // per-disk top, bytes/s
typedef struct { DWORD pid; LONGLONG rx, tx; ULONGLONG totRx, totTx; } NTOP;   // rates bytes/s, totals since start
enum { ETW_OFF, ETW_ON, ETW_DENIED, ETW_DIED };          // etwState
extern int etwOn, etwState, etwTicks;      // etwOn == (etwState == ETW_ON); etwTicks: ticks since session start
extern ULONG etwErr, etwLost;              // last start error; events lost (from session query)
extern DTOP dtop[MAXD][DTOPN]; extern int ndtop[MAXD];
extern NTOP ntop[TOPN]; extern int nNtop;
void EtwInit();
void EtwStart();                           // at startup; EtwTick retries every 15 s while down, health check every 10 s
void EtwStop();                            // on exit
void EtwTick();                            // every timer tick: per-disk tops, net tops + cumulative totals
