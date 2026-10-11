#pragma once
#include "common.h"
#define MAXC 256
extern int ncpu, load[MAXC];               // per-core load, %
extern LONGLONG sysDelta;                  // total CPU time of last interval (100ns)
void InitCpu();
void SampleCpu();
int AvgLoad();
