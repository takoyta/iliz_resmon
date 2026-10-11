// ETW kernel trace (disk I/O + TCP/UDP), always on: per-process disk and network traffic. Needs admin.
#include "common.h"
#define INITGUID
#include <evntrace.h>
#include <evntcons.h>
#include "etw.h"

static const GUID DiskIoGuid = {0x3d6fa8d4, 0xfe05, 0x11d0, {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}};
static const GUID TcpGuid    = {0x9a280ac0, 0xc8e0, 0x11d1, {0x84, 0xe2, 0x00, 0xc0, 0x4f, 0xb9, 0x98, 0xa2}};
static const GUID UdpGuid    = {0xbf3a50c5, 0xa9c9, 0x4988, {0xa0, 0x05, 0x2d, 0xf0, 0xb7, 0xc8, 0x0f, 0x80}};
static const GUID SessGuid   = {0x6c1a7f3e, 0x41b2, 0x4d8e, {0x9a, 0x51, 0x2e, 0x7c, 0x10, 0x5b, 0x33, 0xa4}};
#define SESS_NAME L"CpuTrayDiskIo"
#define AGGN 1024                                        // per-tick hash tables (power of 2)
#define NAGGN 1024
#define CUMN 2048                                        // cumulative per-process net totals
#define THN 4096

// Per-tick tables, filled by the trace thread under cs, swapped out each tick. key 0 = empty slot.
typedef struct { DWORD key; int disk; LONGLONG bytes; } AGG;       // key = pid + 1
typedef struct { DWORD key; LONGLONG rx, tx; } NAGG;
static AGG agg[AGGN], snap[AGGN]; static int nAgg;
static NAGG nagg[NAGGN], nsnap[NAGGN]; static int nNagg;
typedef struct { DWORD key; LONGLONG create; ULONGLONG rx, tx, last; } CUM;   // main thread only
static CUM cum[CUMN]; static int nCum;
static CRITICAL_SECTION cs;
static DWORD thTid[THN], thPid[THN];                     // thread -> process cache (trace thread only)
static TRACEHANDLE etwSess, etwTrace = INVALID_PROCESSTRACE_HANDLE;
static HANDLE etwThread;
static BYTE etwProps[sizeof(EVENT_TRACE_PROPERTIES) + 2 * sizeof(SESS_NAME) + 16];
static ULONGLONG etwRetryAt, etwCheckAt;
#define ETW_RETRY_MS 15000
#define ETW_CHECK_MS 10000

int etwOn, etwTicks; ULONG etwErr;
DTOP dtop[MAXD][DTOPN]; int ndtop[MAXD];
NTOP ntop[TOPN]; int nNtop;

static inline UINT Hash(DWORD k) { return (k * 2654435761u) >> 8; }

void EtwInit() { InitializeCriticalSection(&cs); }

static DWORD PidOfThread(DWORD tid) {
    int h = (tid >> 2) & (THN - 1);
    for (int k = 0; k < 8; k++, h = (h + 1) & (THN - 1)) {
        if (thTid[h] == tid) return thPid[h];
        if (!thTid[h]) break;
    }
    DWORD pid = 0;
    HANDLE t = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    if (t) { pid = GetProcessIdOfThread(t); CloseHandle(t); }
    thTid[h] = tid; thPid[h] = pid;
    return pid;
}

static void OnNet(PEVENT_RECORD e) {                     // TcpIp/UdpIp Send/Recv, IPv4 10/11, IPv6 26/27
    int op = e->EventHeader.EventDescriptor.Opcode;
    if ((op != 10 && op != 11 && op != 26 && op != 27) || e->UserDataLength < 8) return;
    BYTE *d = (BYTE*)e->UserData;
    DWORD key = *(ULONG*)d + 1; LONGLONG size = *(ULONG*)(d + 4);
    int rx = op == 11 || op == 27;
    EnterCriticalSection(&cs);
    for (UINT h = Hash(key) & (NAGGN - 1), k = 0; k < NAGGN; k++, h = (h + 1) & (NAGGN - 1)) {
        NAGG *g = &nagg[h];
        if (!g->key) { if (nNagg >= NAGGN * 3 / 4) break; g->key = key; nNagg++; }
        if (g->key == key) { if (rx) g->rx += size; else g->tx += size; break; }
    }
    LeaveCriticalSection(&cs);
}

static void OnDisk(PEVENT_RECORD e) {                    // DiskIo Read/Write
    int op = e->EventHeader.EventDescriptor.Opcode;
    if ((op != 10 && op != 11) || e->UserDataLength < 52) return;
    BYTE *d = (BYTE*)e->UserData;
    int disk = *(ULONG*)d; LONGLONG size = *(ULONG*)(d + 8); DWORD tid = *(ULONG*)(d + 48);
    DWORD pid = tid ? PidOfThread(tid) : e->EventHeader.ProcessId;
    if (pid == 0xFFFFFFFF) pid = 0;
    DWORD key = pid + 1;
    EnterCriticalSection(&cs);
    for (UINT h = Hash(key * 31 + disk) & (AGGN - 1), k = 0; k < AGGN; k++, h = (h + 1) & (AGGN - 1)) {
        AGG *g = &agg[h];
        if (!g->key) { if (nAgg >= AGGN * 3 / 4) break; g->key = key; g->disk = disk; nAgg++; }
        if (g->key == key && g->disk == disk) { g->bytes += size; break; }
    }
    LeaveCriticalSection(&cs);
}

static void WINAPI OnEvent(PEVENT_RECORD e) {
    const GUID &g = e->EventHeader.ProviderId;
    if (IsEqualGUID(g, TcpGuid) || IsEqualGUID(g, UdpGuid)) OnNet(e);
    else if (IsEqualGUID(g, DiskIoGuid)) OnDisk(e);
}

static DWORD WINAPI TraceThread(LPVOID) { ProcessTrace(&etwTrace, 1, 0, 0); return 0; }

static EVENT_TRACE_PROPERTIES *Props() {
    ZeroMemory(etwProps, sizeof(etwProps));
    EVENT_TRACE_PROPERTIES *p = (EVENT_TRACE_PROPERTIES*)etwProps;
    p->Wnode.BufferSize = sizeof(etwProps);
    p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->Wnode.ClientContext = 1;                          // QPC timestamps
    p->Wnode.Guid = SessGuid;
    p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
    p->EnableFlags = EVENT_TRACE_FLAG_DISK_IO | EVENT_TRACE_FLAG_NETWORK_TCPIP;
    p->BufferSize = 16; p->MinimumBuffers = 2; p->MaximumBuffers = 4; p->FlushTimer = 1;
    p->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    return p;
}

static void EnableProfilePrivilege() {
    HANDLE tk;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tk)) return;
    TOKEN_PRIVILEGES tp = {1};
    LookupPrivilegeValueW(0, L"SeSystemProfilePrivilege", &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(tk, FALSE, &tp, 0, 0, 0);
    CloseHandle(tk);
}

static void Cleanup() {                                  // stop session (if ours/alive), close handles
    ControlTraceW(0, SESS_NAME, Props(), EVENT_TRACE_CONTROL_STOP);
    if (etwTrace != INVALID_PROCESSTRACE_HANDLE) CloseTrace(etwTrace);
    if (etwThread) { WaitForSingleObject(etwThread, 2000); CloseHandle(etwThread); }
    etwTrace = INVALID_PROCESSTRACE_HANDLE; etwThread = 0; etwOn = 0;
    etwRetryAt = GetTickCount64() + ETW_RETRY_MS;
}

void EtwStop() { if (etwOn) Cleanup(); }

static BOOL Alive() {                                    // trace thread running and session still exists
    if (etwThread && WaitForSingleObject(etwThread, 0) == WAIT_OBJECT_0) return FALSE;
    return ControlTraceW(0, SESS_NAME, Props(), EVENT_TRACE_CONTROL_QUERY) == ERROR_SUCCESS;
}

void EtwStart() {
    if (etwOn) return;
    etwRetryAt = GetTickCount64() + ETW_RETRY_MS;
    EnableProfilePrivilege();
    ULONG st = StartTraceW(&etwSess, SESS_NAME, Props());
    if (st == ERROR_ALREADY_EXISTS) {                    // orphan from a killed instance
        ULONG s2 = ControlTraceW(0, SESS_NAME, Props(), EVENT_TRACE_CONTROL_STOP);
        st = s2 == ERROR_SUCCESS ? StartTraceW(&etwSess, SESS_NAME, Props()) : s2;   // access denied -> retry later
    }
    if (st != ERROR_SUCCESS) { etwErr = st; return; }
    EVENT_TRACE_LOGFILEW lf = {0};
    lf.LoggerName = (LPWSTR)SESS_NAME;
    lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    lf.EventRecordCallback = OnEvent;
    etwTrace = OpenTraceW(&lf);
    if (etwTrace == INVALID_PROCESSTRACE_HANDLE) { ControlTraceW(0, SESS_NAME, Props(), EVENT_TRACE_CONTROL_STOP); etwErr = GetLastError(); return; }
    ZeroMemory(agg, sizeof(agg)); ZeroMemory(nagg, sizeof(nagg)); nAgg = nNagg = 0;
    ZeroMemory(thTid, sizeof(thTid));
    etwThread = CreateThread(0, 0, TraceThread, 0, 0, 0);
    etwOn = 1; etwErr = 0; etwTicks = 0;
    etwCheckAt = GetTickCount64() + ETW_CHECK_MS;
}

static void DiskTick(LONGLONG dt) {                      // per-disk top list for the last interval
    for (int i = 0; i < ndisk; i++) ndtop[i] = 0;
    for (int a = 0; a < AGGN; a++) if (snap[a].key) {
        int di = -1;
        for (int i = 0; i < ndisk; i++) if (disks[i].num == snap[a].disk) di = i;
        if (di < 0) continue;
        LONGLONG v = snap[a].bytes * 1000 / dt;
        DTOP *t = dtop[di]; int k = ndtop[di] < DTOPN ? ndtop[di]++ : DTOPN;
        while (k > 0 && t[k - 1].v < v) { if (k < DTOPN) t[k] = t[k - 1]; k--; }
        if (k < DTOPN) { t[k].pid = snap[a].key - 1; t[k].v = v; }
    }
}

static LONGLONG CreateTime(DWORD pid) {                  // 0 if unknown (system / access denied / dead)
    LONGLONG c = 0; FILETIME ct, x1, x2, x3;
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (p) { if (GetProcessTimes(p, &ct, &x1, &x2, &x3)) c = ((LONGLONG)ct.dwHighDateTime << 32) | ct.dwLowDateTime; CloseHandle(p); }
    return c;
}

static CUM *CumFind(DWORD key) {                         // open addressing; returns slot (maybe empty)
    UINT h = Hash(key) & (CUMN - 1);
    while (cum[h].key && cum[h].key != key) h = (h + 1) & (CUMN - 1);
    return &cum[h];
}

static void CumEvict() {                                 // drop the oldest-active quarter (prefer dead), rehash
    static CUM all[CUMN]; int n = 0;
    for (int i = 0; i < CUMN; i++) if (cum[i].key) {
        all[n] = cum[i];
        if (CreateTime(all[n].key - 1) != all[n].create) all[n].last = 0;   // dead or reused: evict first
        n++;
    }
    for (int i = 1; i < n; i++) { CUM t = all[i]; int k = i; while (k > 0 && all[k - 1].last < t.last) { all[k] = all[k - 1]; k--; } all[k] = t; }
    n = n * 3 / 4;
    ZeroMemory(cum, sizeof(cum)); nCum = 0;
    for (int i = 0; i < n; i++) { *CumFind(all[i].key) = all[i]; nCum++; }
}

static CUM *CumAdd(DWORD key, LONGLONG rx, LONGLONG tx) {
    CUM *c = CumFind(key);
    if (c->key && tickNow - c->last > 10000) {          // idle a while: check for pid reuse
        LONGLONG ct = CreateTime(key - 1);
        if (ct != c->create) { c->rx = c->tx = 0; c->create = ct; }
    }
    if (!c->key) {
        if (nCum >= CUMN * 3 / 4) { CumEvict(); c = CumFind(key); }
        c->key = key; c->create = CreateTime(key - 1); c->rx = c->tx = 0; nCum++;
    }
    c->rx += rx; c->tx += tx; c->last = tickNow;
    return c;
}

static void NetTick(LONGLONG dt) {                       // fold into totals, top by current rate
    nNtop = 0;
    for (int i = 0; i < NAGGN; i++) if (nsnap[i].key) {
        NAGG *g = &nsnap[i];
        CUM *c = CumAdd(g->key, g->rx, g->tx);
        NTOP x = {g->key - 1, g->rx * 1000 / dt, g->tx * 1000 / dt, c->rx, c->tx};
        LONGLONG v = x.rx + x.tx; if (v <= 0) continue;
        int k = nNtop < TOPN ? nNtop++ : TOPN;
        while (k > 0 && ntop[k - 1].rx + ntop[k - 1].tx < v) { if (k < TOPN) ntop[k] = ntop[k - 1]; k--; }
        if (k < TOPN) ntop[k] = x;
    }
}

void EtwTick() {
    ULONGLONG now = GetTickCount64();
    if (etwOn && now >= etwCheckAt) {                    // session killed (e.g. by anti-cheat) -> restart later
        etwCheckAt = now + ETW_CHECK_MS;
        if (!Alive()) { etwErr = ERROR_INVALID_HANDLE; Cleanup(); }
    }
    if (!etwOn) { nNtop = 0; for (int i = 0; i < ndisk; i++) ndtop[i] = 0; if (now >= etwRetryAt) EtwStart(); return; }
    EnterCriticalSection(&cs);
    CopyMemory(snap, agg, sizeof(agg)); ZeroMemory(agg, sizeof(agg)); nAgg = 0;
    CopyMemory(nsnap, nagg, sizeof(nagg)); ZeroMemory(nagg, sizeof(nagg)); nNagg = 0;
    LeaveCriticalSection(&cs);
    LONGLONG dt = TickDt();
    DiskTick(dt); NetTick(dt);
    etwTicks++;
}
