#pragma once
#include "common.h"
#define HISTN 300                          // 5 min at 1 s
typedef struct { BYTE v[HISTN]; int n, pos; } HIST;  // ring buffer, 0..100
typedef struct { ULONG v[HISTN]; int n, pos; } HISTL; // ring buffer of bytes/s
extern HIST hCpuAvg, hCpuMax, hMem;
extern HISTL hNetRx, hNetTx;
void PushHistory();                        // every timer tick
int HistGet(const HIST *h, int i);         // i = 0 oldest .. n-1 newest
ULONG HistGetL(const HISTL *h, int i);
