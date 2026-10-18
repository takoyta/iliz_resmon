#pragma once
#include "common.h"
#define MAXG 24                            // engine types x adapters
#define MAXA 4                             // adapters
#define GPU_ROWS 5                         // engine rows in the popup
#define VPROC_ROWS 5                       // process rows by video memory in the popup
typedef struct { WCHAR luid[24], type[24]; int ad, v; } GENG;   // ad: index into gad[], v: %
typedef struct {
    WCHAR luid[24];                        // "0x%08X_0x%08X" (high_low), as in the PDH instance names
    ULONGLONG total, used;                 // bytes of the pool we show (dedicated, or shared for iGPUs); total 0 = unknown
    int shared;                            // 1: integrated GPU (< 512 MB dedicated): shared system memory is shown
} GADAPTER;
typedef struct { DWORD pid; WCHAR name[NAME_LEN]; ULONGLONG bytes; } GPROC;
extern GENG geng[MAXG]; extern GADAPTER gad[MAXA]; extern GPROC gproc[VPROC_ROWS];
extern int ngeng, nGpuAd, ngproc, gpuLoad, gpuOk;   // gpuLoad: max over engine types, %; gpuOk: counters readable
extern int gpuAd, vramPct;                 // adapter shown for video memory (-1 none), its use in %
void GpuEnable(BOOL on);                   // load pdh.dll and start the query only while the GPU icon is on
void SampleGpu();                          // every tick (no-op while disabled)
