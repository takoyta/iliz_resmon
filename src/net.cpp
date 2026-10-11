// Network adapters via GetIfTable2 (no admin needed).
#include "common.h"
#include "net.h"

NIC nics[MAXN];
int nnic, netRx, netTx;
static LONGLONG netPeak[60]; static int netPeakPos;

void NetTotals(LONGLONG *rx, LONGLONG *tx) {
    *rx = *tx = 0;
    for (int i = 0; i < nnic; i++) { *rx += nics[i].rps; *tx += nics[i].tps; }
}

void SampleNet() {
    PMIB_IF_TABLE2 t;
    if (GetIfTable2(&t) != NO_ERROR) return;
    LONGLONG dt = TickDt();
    for (int i = 0; i < nnic; i++) nics[i].seen = 0;
    for (ULONG k = 0; k < t->NumEntries; k++) {
        MIB_IF_ROW2 *r = &t->Table[k];
        if (r->Type == IF_TYPE_SOFTWARE_LOOPBACK || r->OperStatus != IfOperStatusUp) continue;
        if (!r->InterfaceAndOperStatusFlags.HardwareInterface) continue;
        int i = 0;
        for (; i < nnic; i++) if (nics[i].luid.Value == r->InterfaceLuid.Value) break;
        if (i == nnic) {
            if (nnic >= MAXN) continue;
            NIC *n = &nics[nnic++]; ZeroMemory(n, sizeof(*n));
            n->luid = r->InterfaceLuid; lstrcpynW(n->name, r->Alias, 40);
            n->in0 = n->inStart = r->InOctets; n->out0 = n->outStart = r->OutOctets;
        }
        NIC *n = &nics[i];
        ULONG64 di = r->InOctets >= n->in0 ? r->InOctets - n->in0 : 0;   // counter reset -> 0
        ULONG64 dout = r->OutOctets >= n->out0 ? r->OutOctets - n->out0 : 0;
        if (r->InOctets < n->inStart || r->OutOctets < n->outStart) { n->inStart = r->InOctets; n->outStart = r->OutOctets; }
        n->rps = (LONGLONG)(di * 1000 / dt); n->tps = (LONGLONG)(dout * 1000 / dt);
        n->in0 = r->InOctets; n->out0 = r->OutOctets; n->seen = 1;
    }
    FreeMibTable(t);
    for (int i = 0; i < nnic; ) if (!nics[i].seen) nics[i] = nics[--nnic]; else i++;   // adapter went down
    LONGLONG rx, tx; NetTotals(&rx, &tx);
    netPeak[netPeakPos++ % 60] = rx > tx ? rx : tx;
    LONGLONG pk = 128 * 1024;                              // at least 128 KB/s
    for (int i = 0; i < 60; i++) if (netPeak[i] > pk) pk = netPeak[i];
    netRx = (int)(rx * 100 / pk); netTx = (int)(tx * 100 / pk);
}
