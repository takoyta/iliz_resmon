// Metric history for the popup graphs: one sample per second (averaged over the ticks of that second,
// busiest core: max), kept for HISTN seconds.
#pragma once
#include "common.h"
#define HISTN 300                          // 5 min at 1 s
enum { H_CPU, H_CPUMAX, H_MEM, H_DISK, H_GPU, H_PCT_N };   // percent series, 0..100 (stored as bytes)
enum { H_RX, H_TX, H_NET_N };              // bytes/s series (stored as 16-bit floating codes, ~0.05% error)
void PushHistory();                        // every timer tick
int HistLen();                             // seconds stored, <= HISTN
int HistPct(int m, int i);                 // i = 0 oldest .. HistLen()-1 newest
ULONG HistNet(int m, int i);
