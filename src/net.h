#pragma once
#include <ws2ipdef.h>
#include <iphlpapi.h>
#define MAXN 8
typedef struct { NET_LUID luid; WCHAR name[40]; ULONG64 in0, out0, inStart, outStart; LONGLONG rps, tps; int seen; } NIC;
extern NIC nics[MAXN];
extern int nnic;
extern int netRx, netTx;                   // icon bars, % of last-minute peak
void SampleNet();
void NetTotals(LONGLONG *rx, LONGLONG *tx);
