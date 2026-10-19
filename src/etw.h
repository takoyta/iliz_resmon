#pragma once
#include "common.h"
#define DTOPN_MAX 6
#define DTOPN ((topN * 2 + 2) / 5)           // per-disk rows: 4 at the default 10 (1..6)
typedef struct { DWORD pid; LONGLONG v, r, w; } DTOP;    // per-disk top, bytes/s: v = read r + write w
enum { ETW_OFF, ETW_ON, ETW_DENIED, ETW_DIED };          // etwState
extern int etwOn, etwState, etwTicks;      // etwOn == (etwState == ETW_ON); etwTicks: ticks since session start
extern ULONG etwErr, etwLost;              // last start error; events lost (from session query)
extern DTOP dtop[MAXD][DTOPN_MAX]; extern int ndtop[MAXD];
int EtwNote();                             // !etwOn: S_NOTE_ADMIN (not elevated) or S_NOTE_BLOCKED; 0 while tracing
void EtwInit();
void EtwStart();                           // at startup; EtwTick retries every 15 s while down, health check every 10 s
void EtwStop();                            // on exit
void EtwTick();                            // every timer tick: per-disk tops, net traffic into ptNet
