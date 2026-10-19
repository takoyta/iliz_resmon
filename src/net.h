#pragma once
#include "common.h"
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#define MAXN 16
typedef struct { NET_LUID luid; WCHAR name[40]; ULONG64 in0, out0, inStart, outStart;   /* *Start: counters at local 00:00 (or when first seen) */ LONGLONG rps, tps; int seen; } NIC;
extern NIC nics[MAXN];
extern int nnic;
extern int netRx, netTx;                   // icon bars, % of last-minute peak
void SampleNet();
void NetTotals(LONGLONG *rx, LONGLONG *tx);
