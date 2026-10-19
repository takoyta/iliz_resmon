#pragma once
#include "common.h"
enum { TM_CPU, TM_MEM, TM_N };      // top-list metrics, all computed every tick
typedef struct { DWORD pid; WCHAR name[NAME_LEN]; LONGLONG v; } TOP;   // CPU: percent*10; MEM: bytes
extern TOP top[TM_N][TOPN_MAX];
extern int nTop[TM_N], haveBase;
void SampleProcs();                        // every tick: snapshot + top lists, I/O deltas into ptIo
const WCHAR *NameOf(DWORD pid);
LONGLONG CreateOf(DWORD pid);              // creation time from the last snapshot, 0 if not there
