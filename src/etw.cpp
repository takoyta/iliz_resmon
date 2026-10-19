// ETW kernel trace (disk I/O + TCP/UDP), always on: per-process disk and network traffic. Needs admin.
#include "common.h"
#define INITGUID
#include <evntrace.h>
#include <evntcons.h>
#include "etw.h"
#include "disk.h"
#include "procs.h"
#include "proctot.h"
#include "lang.h"
#include "autostart.h"

static const GUID DiskIoGuid = {0x3d6fa8d4, 0xfe05, 0x11d0, {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}};
static const GUID TcpGuid    = {0x9a280ac0, 0xc8e0, 0x11d1, {0x84, 0xe2, 0x00, 0xc0, 0x4f, 0xb9, 0x98, 0xa2}};
static const GUID UdpGuid    = {0xbf3a50c5, 0xa9c9, 0x4988, {0xa0, 0x05, 0x2d, 0xf0, 0xb7, 0xc8, 0x0f, 0x80}};
static const GUID SessGuid   = {0x6c1a7f3e, 0x41b2, 0x4d8e, {0x9a, 0x51, 0x2e, 0x7c, 0x10, 0x5b, 0x33, 0xa4}};
#define SESS_NAME APP_ID
// Kernel MOF event layouts (x64). DiskIo_TypeGroup1: Read=10, Write=11; DiskNumber@0, TransferSize@8,
// IssuingThreadId@48. TcpIp/UdpIp_TypeGroup1: Send=10, Recv=11, SendIPV6=26, RecvIPV6=27; PID@0, size@4.
#define OP_SEND    10
#define OP_RECV    11
#define OP_SEND6   26
#define OP_RECV6   27
#define DISK_EV_MIN 52
#define DISK_EV_TID 48
#define AGGN 1024                                        // per-tick hash tables (power of 2)
#define NAGGN 512
#define THN 4096

// Per-tick tables, filled by the trace thread under cs, swapped out each tick. key 0 = empty slot.
typedef struct { DWORD key; int disk; LONGLONG rd, wr; } AGG;     // key = pid + 1; bytes read / written on that disk
typedef struct { DWORD key; LONGLONG rx, tx; } NAGG;
// Double-buffered: the trace thread writes *agg / *nagg; each tick the pointers swap under cs and the
// drained table is read and cleared outside the lock.
static AGG aggT[2][AGGN], *agg = aggT[0], *snap = aggT[1]; static int nAgg;
static NAGG naggT[2][NAGGN], *nagg = naggT[0], *nsnap = naggT[1]; static int nNagg;
static CRITICAL_SECTION cs;
static DWORD thTid[THN], thPid[THN], thAt[THN];          // thread -> process cache + lookup time in s (trace thread only)
#define TH_TTL 30                                        // thread ids get reused: re-check after 30 s
static TRACEHANDLE etwSess, etwTrace = INVALID_PROCESSTRACE_HANDLE;
static HANDLE etwThread;
static BYTE etwProps[sizeof(EVENT_TRACE_PROPERTIES) + 2 * sizeof(SESS_NAME) + 16];
static ULONGLONG etwRetryAt, etwCheckAt;
#define ETW_RETRY_MS 15000
#define ETW_CHECK_MS 10000

int etwOn, etwState, etwTicks; ULONG etwErr, etwLost;
DTOP dtop[MAXD][DTOPN_MAX]; int ndtop[MAXD];

static inline UINT Hash(DWORD k) { return (k * 2654435761u) >> 8; }

static int elevated;
void EtwInit() { InitializeCriticalSection(&cs); elevated = IsElevated(); PtInit(); }

int EtwNote() { return etwOn ? 0 : elevated ? S_NOTE_BLOCKED : S_NOTE_ADMIN; }

static DWORD PidOfThread(DWORD tid) {
    DWORD now = (DWORD)(GetTickCount64() / 1000);
    int h = (tid >> 2) & (THN - 1);
    for (int k = 0; k < 8; k++, h = (h + 1) & (THN - 1)) {
        if (thTid[h] == tid) { if (now - thAt[h] < TH_TTL) return thPid[h]; break; }
        if (!thTid[h]) break;
    }
    DWORD pid = 0;
    HANDLE t = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    if (t) { pid = GetProcessIdOfThread(t); CloseHandle(t); }
    thTid[h] = tid; thPid[h] = pid; thAt[h] = now;
    return pid;
}

static void OnNet(PEVENT_RECORD e) {                     // TcpIp/UdpIp Send/Recv, IPv4 10/11, IPv6 26/27
    int op = e->EventHeader.EventDescriptor.Opcode;
    if ((op != OP_SEND && op != OP_RECV && op != OP_SEND6 && op != OP_RECV6) || e->UserDataLength < 8) return;
    BYTE *d = (BYTE*)e->UserData;
    DWORD key = *(ULONG*)d + 1; LONGLONG size = *(ULONG*)(d + 4);
    int rx = op == OP_RECV || op == OP_RECV6;
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
    if ((op != OP_SEND && op != OP_RECV) || e->UserDataLength < DISK_EV_MIN) return;   // same opcodes: Read/Write
    BYTE *d = (BYTE*)e->UserData;
    int disk = *(ULONG*)d; LONGLONG size = *(ULONG*)(d + 8); DWORD tid = *(ULONG*)(d + DISK_EV_TID);
    DWORD pid = tid ? PidOfThread(tid) : e->EventHeader.ProcessId;
    if (pid == 0xFFFFFFFF) pid = 0;
    DWORD key = pid + 1;
    EnterCriticalSection(&cs);
    for (UINT h = Hash(key * 31 + disk) & (AGGN - 1), k = 0; k < AGGN; k++, h = (h + 1) & (AGGN - 1)) {
        AGG *g = &agg[h];
        if (!g->key) { if (nAgg >= AGGN * 3 / 4) break; g->key = key; g->disk = disk; nAgg++; }
        if (g->key == key && g->disk == disk) { if (op == OP_SEND) g->rd += size; else g->wr += size; break; }   // 10 Read, 11 Write
    }
    LeaveCriticalSection(&cs);
}

static void WINAPI OnEvent(PEVENT_RECORD e) {
    const GUID &g = e->EventHeader.ProviderId;
    if (InlineIsEqualGUID(g, TcpGuid) || InlineIsEqualGUID(g, UdpGuid)) OnNet(e);   // Inline*: no memcmp (no CRT)
    else if (InlineIsEqualGUID(g, DiskIoGuid)) OnDisk(e);
}

static DWORD WINAPI TraceThread(LPVOID) {
    ZeroMemory(thTid, sizeof(thTid));                    // cache is owned by this thread
    ProcessTrace(&etwTrace, 1, 0, 0);
    return 0;
}

static EVENT_TRACE_PROPERTIES *Props() {
    ZeroMemory(etwProps, sizeof(etwProps));
    EVENT_TRACE_PROPERTIES *p = (EVENT_TRACE_PROPERTIES*)etwProps;
    p->Wnode.BufferSize = sizeof(etwProps);
    p->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->Wnode.ClientContext = 1;                          // QPC timestamps
    p->Wnode.Guid = SessGuid;
    p->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
    p->EnableFlags = EVENT_TRACE_FLAG_DISK_IO | EVENT_TRACE_FLAG_NETWORK_TCPIP;
    p->BufferSize = 16; p->MinimumBuffers = 4; p->MaximumBuffers = 32; p->FlushTimer = 1;   // KB; up to 512 KB pool
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

static BOOL ThreadDone() {                               // reap the trace thread if it has exited
    if (!etwThread) return TRUE;
    if (WaitForSingleObject(etwThread, 0) != WAIT_OBJECT_0) return FALSE;
    CloseHandle(etwThread); etwThread = 0;
    return TRUE;
}

static void Cleanup(int state) {                         // stop session (if alive), close handles
    ControlTraceW(0, SESS_NAME, Props(), EVENT_TRACE_CONTROL_STOP);
    if (etwTrace != INVALID_PROCESSTRACE_HANDLE) CloseTrace(etwTrace);
    if (etwThread) WaitForSingleObject(etwThread, 2000);
    ThreadDone();                                        // still running -> keep handle, EtwStart waits for it
    etwTrace = INVALID_PROCESSTRACE_HANDLE; etwOn = 0; etwState = state;
    etwRetryAt = GetTickCount64() + ETW_RETRY_MS;
}

void EtwResume() { etwCheckAt = etwRetryAt = 0; }

void EtwStop() { if (etwOn) Cleanup(ETW_OFF); }

static BOOL Alive() {                                    // trace thread running and session still exists
    if (etwThread && WaitForSingleObject(etwThread, 0) == WAIT_OBJECT_0) return FALSE;
    EVENT_TRACE_PROPERTIES *p = Props();
    if (ControlTraceW(0, SESS_NAME, p, EVENT_TRACE_CONTROL_QUERY) != ERROR_SUCCESS) return FALSE;
    etwLost = p->EventsLost + p->RealTimeBuffersLost;
    return TRUE;
}

// Stop leftover sessions from killed instances: our session GUID must be unique, so an orphan
// left by a killed instance makes StartTrace fail with a name collision.
static void StopOrphans() {
    enum { N = 64, PSZ = sizeof(EVENT_TRACE_PROPERTIES) + 2 * 256 * sizeof(WCHAR) };
    BYTE *buf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, N * PSZ);   // ~73 KB, only for this call
    if (!buf) return;
    EVENT_TRACE_PROPERTIES *pp[N];
    for (int i = 0; i < N; i++) {
        pp[i] = (EVENT_TRACE_PROPERTIES*)(buf + i * PSZ);
        pp[i]->Wnode.BufferSize = PSZ;
        pp[i]->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        pp[i]->LogFileNameOffset = sizeof(EVENT_TRACE_PROPERTIES) + 256 * sizeof(WCHAR);
    }
    ULONG n = 0;
    QueryAllTracesW(pp, N, &n);                       // ERROR_MORE_DATA still fills N entries
    if (n > N) n = N;                                    // n is the total count, may exceed the array
    for (ULONG i = 0; i < n; i++) {
        const WCHAR *name = (const WCHAR*)((BYTE*)pp[i] + pp[i]->LoggerNameOffset);
        if (InlineIsEqualGUID(pp[i]->Wnode.Guid, SessGuid))
            ControlTraceW(0, name, Props(), EVENT_TRACE_CONTROL_STOP);
    }
    HeapFree(GetProcessHeap(), 0, buf);
}

void EtwStart() {
    if (etwOn) return;
    etwRetryAt = GetTickCount64() + ETW_RETRY_MS;
    if (!ThreadDone()) return;                           // previous trace thread still draining
    StopOrphans();
    EnableProfilePrivilege();
    ULONG st = StartTraceW(&etwSess, SESS_NAME, Props());
    if (st == ERROR_ALREADY_EXISTS) {                    // orphan from a killed instance
        ULONG s2 = ControlTraceW(0, SESS_NAME, Props(), EVENT_TRACE_CONTROL_STOP);
        st = s2 == ERROR_SUCCESS ? StartTraceW(&etwSess, SESS_NAME, Props()) : s2;   // access denied -> retry later
    }
    if (st != ERROR_SUCCESS) { etwErr = st; etwState = ETW_DENIED; return; }
    EVENT_TRACE_LOGFILEW lf = {0};
    lf.LoggerName = (LPWSTR)SESS_NAME;
    lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    lf.EventRecordCallback = OnEvent;
    etwTrace = OpenTraceW(&lf);
    if (etwTrace == INVALID_PROCESSTRACE_HANDLE) { ControlTraceW(0, SESS_NAME, Props(), EVENT_TRACE_CONTROL_STOP); etwErr = GetLastError(); etwState = ETW_DENIED; return; }
    ZeroMemory(aggT, sizeof(aggT)); ZeroMemory(naggT, sizeof(naggT)); nAgg = nNagg = 0;   // no thread running now
    etwThread = CreateThread(0, 0, TraceThread, 0, 0, 0);
    if (!etwThread) { etwErr = GetLastError(); Cleanup(ETW_DENIED); return; }
    etwOn = 1; etwState = ETW_ON; etwErr = 0; etwLost = 0; etwTicks = 0;
    etwCheckAt = GetTickCount64() + ETW_CHECK_MS;
}

static void DiskTick() {                                 // daily per-process totals (all disks) + per-disk top lists
    LONGLONG dt = TickDt();
    for (int i = 0; i < ndisk; i++) ndtop[i] = 0;
    for (int a = 0; a < AGGN; a++) if (snap[a].key) {
        const AGG *g = &snap[a];
        PtAdd(&ptDisk, g->key - 1, g->rd, g->wr);        // several disks of one pid add up within the tick
        int di = -1;
        for (int i = 0; i < ndisk; i++) if (disks[i].num == g->disk) di = i;
        if (di < 0) continue;
        LONGLONG r = g->rd * 1000 / dt, w = g->wr * 1000 / dt, v = r + w;   // sorted by the total
        int k = TopPush(dtop[di], &ndtop[di], DTOPN, [&](const DTOP &x) { return x.v < v; });
        if (k >= 0) { DTOP *t = &dtop[di][k]; t->pid = g->key - 1; t->v = v; t->r = r; t->w = w; }
    }
}

static void NetTick() {                                  // fold this tick into the daily per-process totals
    for (int i = 0; i < NAGGN; i++) if (nsnap[i].key) PtAdd(&ptNet, nsnap[i].key - 1, nsnap[i].rx, nsnap[i].tx);
}

void EtwTick() {
    ULONGLONG now = GetTickCount64();
    if (etwOn && now >= etwCheckAt) {                    // session killed (e.g. by anti-cheat) -> restart later
        etwCheckAt = now + ETW_CHECK_MS;
        if (!Alive()) { etwErr = 0; Cleanup(ETW_DIED); }
    }
    if (!etwOn) { for (int i = 0; i < ndisk; i++) ndtop[i] = 0; if (elevated && now >= etwRetryAt) EtwStart(); return; }   // not elevated: one try at startup only; daily totals kept
    EnterCriticalSection(&cs);
    AGG *a = agg; agg = snap; snap = a; nAgg = 0;
    NAGG *g = nagg; nagg = nsnap; nsnap = g; nNagg = 0;
    LeaveCriticalSection(&cs);
    DiskTick(); NetTick();
    ZeroMemory(snap, sizeof(aggT[0])); ZeroMemory(nsnap, sizeof(naggT[0]));   // ready for the next swap
    etwTicks++;
}
