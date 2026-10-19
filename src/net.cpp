// Network adapters via GetIfTable2 (no admin needed).
#include "common.h"
#include "net.h"

NIC nics[MAXN];
int nnic, netRx, netTx;
static LONGLONG pkV[60]; static ULONGLONG pkS[60];       // per-second max of rx/tx for the last minute (icon scale)

void NetTotals(LONGLONG *rx, LONGLONG *tx) {
    *rx = *tx = 0;
    for (int i = 0; i < nnic; i++) { *rx += nics[i].rps; *tx += nics[i].tps; }
}

void SampleNet() {
    PMIB_IF_TABLE2 t;
    if (GetIfTable2(&t) != NO_ERROR) return;
    LONGLONG dt = TickDt();
    static WORD day; SYSTEMTIME st; GetLocalTime(&st);      // adapter totals cover today, like the process tables
    BOOL newDay = day && day != st.wDay; day = st.wDay;
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
        if (newDay || r->InOctets < n->inStart || r->OutOctets < n->outStart) { n->inStart = r->InOctets; n->outStart = r->OutOctets; }
        n->rps = (LONGLONG)(di * 1000 / dt); n->tps = (LONGLONG)(dout * 1000 / dt);
        n->in0 = r->InOctets; n->out0 = r->OutOctets; n->seen = 1;
    }
    FreeMibTable(t);
    for (int i = 0; i < nnic; ) if (!nics[i].seen) nics[i] = nics[--nnic]; else i++;   // adapter went down
    LONGLONG rx, tx; NetTotals(&rx, &tx);
    ULONGLONG sec = tickNow / 1000; int b = (int)(sec % 60);   // time-based: same minute at any interval
    if (pkS[b] != sec) { pkS[b] = sec; pkV[b] = 0; }
    if ((rx > tx ? rx : tx) > pkV[b]) pkV[b] = rx > tx ? rx : tx;
    LONGLONG pk = 128 * 1024;                              // at least 128 KB/s
    for (int i = 0; i < 60; i++) if (sec - pkS[i] < 60 && pkV[i] > pk) pk = pkV[i];
    netRx = (int)(rx * 100 / pk); netTx = (int)(tx * 100 / pk);
}
