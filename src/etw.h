#pragma once
#include "common.h"
#define DTOPN_MAX 6
#define DTOPN ((topN * 2 + 2) / 5)           // per-disk rows: 4 at the default 10 (1..6)
typedef struct { DWORD pid; LONGLONG v, r, w; } DTOP;    // per-disk top, bytes/s: v = read r + write w
typedef struct {                           // net process row: totals since start + current rates
    DWORD pid; WCHAR name[NAME_LEN];
    LONGLONG rx, tx;                       // bytes/s this tick (0 if idle)
    ULONGLONG totRx, totTx;
    int live;                              // traffic in the last NET_IDLE_MS
} NTOP;
#define NET_IDLE_MS 5000
enum { ETW_OFF, ETW_ON, ETW_DENIED, ETW_DIED };          // etwState
extern int etwOn, etwState, etwTicks;      // etwOn == (etwState == ETW_ON); etwTicks: ticks since session start
extern ULONG etwErr, etwLost;              // last start error; events lost (from session query)
extern DTOP dtop[MAXD][DTOPN_MAX]; extern int ndtop[MAXD];
extern NTOP ntop[TOPN_MAX]; extern int nNtop;   // top by totRx desc (all processes seen since start)
int EtwNote();                             // !etwOn: S_NOTE_ADMIN (not elevated) or S_NOTE_BLOCKED; 0 while tracing
void EtwInit();
void EtwStart();                           // at startup; EtwTick retries every 15 s while down, health check every 10 s
void EtwStop();                            // on exit
void EtwTick();                            // every timer tick: per-disk tops, cumulative net totals + list
void EtwNames();                           // after SampleProcs: remember names of active pids
