#pragma once
#include "common.h"
#define MAXG 24                            // engine types x adapters
#define GPU_ROWS 5                         // engine rows in the popup
typedef struct { WCHAR luid[24], type[24]; int ad, v; } GENG;   // ad: adapter index (order seen), v: %
extern GENG geng[MAXG];
extern int ngeng, nGpuAd, gpuLoad, gpuOk;  // gpuLoad: max over engine types, %; gpuOk: counters readable
void GpuEnable(BOOL on);                   // load pdh.dll and start the query only while the GPU icon is on
void SampleGpu();                          // every tick (no-op while disabled)
