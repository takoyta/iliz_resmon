#pragma once
enum { TM_CPU, TM_IO, TM_MEM, TM_N };      // top-list metrics, all computed every tick
typedef struct { DWORD pid; WCHAR name[40]; LONGLONG v; } TOP;       // CPU: percent*10; IO: bytes/s; MEM: bytes
extern TOP top[TM_N][TOPN];
extern int nTop[TM_N], haveBase;
void SampleProcs();                        // every tick: snapshot + all top lists
const WCHAR *NameOf(DWORD pid);
