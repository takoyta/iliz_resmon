// Process sampling via NtQuerySystemInformation(SystemProcessInformation), every tick:
// top lists by CPU time, I/O bytes and private working set. Buffers sized to the actual process count.
#include "common.h"
#include "procs.h"
#include "cpu.h"

typedef struct {                                         // class 5 (x64 layout)
    ULONG Next, NThreads; LARGE_INTEGER PrivWs, R[2], Create, User, Kernel;
    USHORT Len, Max; PWSTR Name; LONG Prio; HANDLE Pid;
} SPI;
#define SPI_READ_BYTES  232                              // ReadTransferCount offset (x64)
#define SPI_WRITE_BYTES 240                              // WriteTransferCount offset (x64)

typedef struct { DWORD pid; LONGLONG create, t, io; WCHAR name[28]; } PREC;
static PREC *pPrev, *pCur; static int nPrev, cap;        // snapshots, cap entries each
static int *hIdx; static int hMask;                      // pid -> index into pPrev (open addressing, -1 empty)
static BYTE *pbuf; static ULONG pbufSize;
int haveBase, nTop[TM_N];
TOP top[TM_N][TOPN];

static void *Grow(void *p, SIZE_T n) {
    HANDLE h = GetProcessHeap();
    return p ? HeapReAlloc(h, 0, p, n) : HeapAlloc(h, 0, n);
}

static void CopyName(WCHAR *dst, const SPI *s, int max) {  // without ".exe"
    int n = s->Len / 2; if (n > max) n = max;
    for (int c = 0; c < n; c++) dst[c] = s->Name[c];
    dst[n] = 0;
    if (n > 4 && !lstrcmpiW(dst + n - 4, L".exe")) dst[n - 4] = 0;
}

static void TopInsert(int m, const SPI *s, LONGLONG v) {
    if (v <= 0) return;
    TOP *t = top[m]; int k = nTop[m] < TOPN ? nTop[m]++ : TOPN;
    while (k > 0 && t[k - 1].v < v) { if (k < TOPN) t[k] = t[k - 1]; k--; }
    if (k >= TOPN) return;
    CopyName(t[k].name, s, 39); t[k].pid = (DWORD)(ULONG_PTR)s->Pid;
    t[k].v = v;
}

static inline UINT H(DWORD pid) { return (pid >> 2) * 2654435761u; }

static const PREC *FindPrev(DWORD pid) {
    if (!hIdx) return 0;
    for (UINT h = H(pid) & hMask; hIdx[h] >= 0; h = (h + 1) & hMask)
        if (pPrev[hIdx[h]].pid == pid) return &pPrev[hIdx[h]];
    return 0;
}

static void IndexPrev() {                                // hash table at <= 50% load
    int size = 64; while (size < nPrev * 2) size <<= 1;
    if (size - 1 != hMask) { hIdx = (int*)Grow(hIdx, size * sizeof(int)); hMask = size - 1; }
    for (int i = 0; i <= hMask; i++) hIdx[i] = -1;
    for (int i = 0; i < nPrev; i++) {
        UINT h = H(pPrev[i].pid) & hMask;
        while (hIdx[h] >= 0) h = (h + 1) & hMask;
        hIdx[h] = i;
    }
}

void SampleProcs() {
    ULONG need = 0; LONG st;
    for (;;) {
        st = NtQSI(5, pbuf, pbufSize, &need);
        if (st != (LONG)0xC0000004) break;               // STATUS_INFO_LENGTH_MISMATCH
        pbufSize = need + 16384;
        BYTE *nb = (BYTE*)Grow(pbuf, pbufSize); if (!nb) return; pbuf = nb;
    }
    if (st < 0) return;
    int n = 0;
    for (BYTE *p = pbuf;; p += ((SPI*)p)->Next) { n++; if (!((SPI*)p)->Next) break; }
    if (n > cap) {
        cap = n + 64;
        pPrev = (PREC*)Grow(pPrev, cap * sizeof(PREC)); pCur = (PREC*)Grow(pCur, cap * sizeof(PREC));
        if (!pPrev || !pCur) { cap = nPrev = haveBase = 0; return; }
    }
    int nCur = 0; LONGLONG dt = TickDt();
    for (int m = 0; m < TM_N; m++) nTop[m] = 0;
    for (BYTE *p = pbuf;; p += ((SPI*)p)->Next) {
        SPI *s = (SPI*)p;
        if (s->Pid) {
            PREC *r = &pCur[nCur++];
            r->pid = (DWORD)(ULONG_PTR)s->Pid; r->create = s->Create.QuadPart;
            r->t = s->User.QuadPart + s->Kernel.QuadPart;
            r->io = *(LONGLONG*)(p + SPI_READ_BYTES) + *(LONGLONG*)(p + SPI_WRITE_BYTES);
            CopyName(r->name, s, 27);
            TopInsert(TM_MEM, s, s->PrivWs.QuadPart);    // instantaneous, no baseline
            const PREC *o = haveBase ? FindPrev(r->pid) : 0;
            if (o && o->create == r->create) {
                if (sysDelta > 0) TopInsert(TM_CPU, s, (r->t - o->t) * 1000 / sysDelta);
                TopInsert(TM_IO, s, (r->io - o->io) * 1000 / dt);
            }
        }
        if (!s->Next) break;
    }
    PREC *t = pPrev; pPrev = pCur; pCur = t; nPrev = nCur; haveBase = 1;
    IndexPrev();
}

const WCHAR *NameOf(DWORD pid) {
    if (pid == 0) return L"System (ядро)";
    if (pid == 4) return L"System";
    const PREC *r = FindPrev(pid);
    return r ? r->name : L"?";
}
