// Metric history, one sample per second (averaged over the ticks of that second, busiest core: max),
// kept for an hour. The popups show the last HISTN seconds, the history window the whole hour.
#pragma once
#include "common.h"
#define HLONG 3600                         // 60 min at 1 s
#define HISTN 300                          // popup graph span: 5 min
enum { H_CPU, H_CPUMAX, H_MEM, H_DISK, H_PCT_N };   // percent series, 0..100 (stored as bytes)
enum { H_RX, H_TX, H_NET_N };              // bytes/s series (stored as 16-bit floating codes, ~0.05% error)
void PushHistory();                        // every timer tick
int HistLen();                             // seconds stored, <= HLONG
int HistPct(int m, int i);                 // i = 0 oldest .. HistLen()-1 newest
ULONG HistNet(int m, int i);
