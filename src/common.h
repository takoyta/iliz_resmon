// Shared constants and app-wide state (window handles, popup mode, tick clock).
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define WM_TRAY (WM_APP + 1)
#define TOPN 10                            // process list length
#define MAXD 16                            // physical disks
#define NAME_LEN 32                        // process name buffer (incl. 0)
enum { ID_CPU = 1, ID_DISK, ID_NET, ID_MEM };   // tray icon ids = popup modes

typedef LONG (WINAPI *NtQSI_t)(int, PVOID, ULONG, PULONG);
extern NtQSI_t NtQSI;
extern HWND hMain, hPop;
extern int popMode;                        // 0 hidden, else ID_* of the open popup
extern ULONGLONG tickPrev, tickNow;

inline LONGLONG TickDt() { ULONGLONG d = tickNow - tickPrev; return d ? (LONGLONG)d : 1000; }   // ms
