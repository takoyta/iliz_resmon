// Shared headers, constants, colors and app-wide state.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>

#define WM_TRAY (WM_APP + 1)
#define TOPN 10
#define ID_CPU  1
#define ID_DISK 2
#define ID_NET  3
#define ID_MEM  4

#define C_CPU      RGB(150, 230, 60)
#define C_CPU_DARK RGB(70, 125, 35)
#define C_DSK      RGB(70, 215, 200)
#define C_DSK_DARK RGB(30, 110, 105)
#define C_RX       RGB(100, 190, 255)
#define C_RX_DARK  RGB(40, 80, 150)
#define C_MEM      RGB(190, 120, 255)
#define C_MEM_DARK RGB(90, 50, 140)
#define C_TX       RGB(255, 150, 40)

typedef LONG (WINAPI *NtQSI_t)(int, PVOID, ULONG, PULONG);
extern NtQSI_t NtQSI;
extern HWND hMain, hPop;
extern int popMode;                        // 0 hidden, else ID_* of the open popup
extern ULONGLONG tickPrev, tickNow;

inline LONGLONG TickDt() { ULONGLONG d = tickNow - tickPrev; return d ? (LONGLONG)d : 1000; }   // ms
