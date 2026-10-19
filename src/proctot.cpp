// Per-process daily totals, see proctot.h. Main thread only.
#include "common.h"
#include "proctot.h"
#include "procs.h"
#include "disk.h"

struct PTENT_ {
    DWORD key;                             // pid + 1, 0 = empty slot
    LONGLONG create;                       // process creation time (0 = unknown): detects pid reuse
    ULONGLONG ta, tb;                      // today
    LONGLONG ka, kb;                       // this tick (valid while last == tickNow)
    ULONGLONG last;                        // tickNow of the last traffic, ms
    WCHAR name[NAME_LEN];                  // last known name (survives process exit)
};

PTAB ptNet, ptIo, *ptDisk[PT_DISKS];

static inline UINT Hash(DWORD k) { return (k * 2654435761u) >> 8; }

static void Alloc(PTAB *t, int cap, int sumSort) {
    t->e = (PTENT*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cap * sizeof(PTENT));
    t->cap = t->e ? cap : 0; t->sumSort = sumSort;
}

void PtInit() { Alloc(&ptNet, 1024, 0); Alloc(&ptIo, 512, 1); }

static DWORD ModelTag(const DISK *d) { DWORD h = 2166136261u; for (const WCHAR *p = d->model; *p; p++) h = (h ^ *p) * 16777619u; return h; }

static const DISK *Listed(int num) { for (int i = 0; i < ndisk; i++) if (disks[i].num == num) return &disks[i]; return 0; }

PTAB *PtDisk(int num) {                                  // 256 entries (~30 KB) per drive with traffic
    if (num < 0 || num >= PT_DISKS) return 0;
    if (!ptDisk[num]) {
        const DISK *d = Listed(num); if (!d) return 0;
        PTAB *t = (PTAB*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PTAB)); if (!t) return 0;
        Alloc(t, 256, 1);
        if (!t->cap) { HeapFree(GetProcessHeap(), 0, t); return 0; }
        t->tag = ModelTag(d); ptDisk[num] = t;
    }
    return ptDisk[num];
}

void PtDisksSync() {                                     // same number + same model: keep today's totals
    for (int n = 0; n < PT_DISKS; n++) if (ptDisk[n]) {
        const DISK *d = Listed(n);
        if (d && ModelTag(d) == ptDisk[n]->tag) continue;
        HeapFree(GetProcessHeap(), 0, ptDisk[n]->e); HeapFree(GetProcessHeap(), 0, ptDisk[n]); ptDisk[n] = 0;
    }
}

int PtDiskRows() {
    int r = ndisk > 1 ? topN / ndisk : topN, m = (topN * 2 + 2) / 5;
    return r < m ? m : r;
}

void PtDisksFinish(int rows) { for (int n = 0; n < PT_DISKS; n++) if (ptDisk[n]) PtFinish(ptDisk[n], rows); }

static LONGLONG Created(DWORD pid) {                     // snapshot first (free), else ask the process
    LONGLONG c = CreateOf(pid); if (c) return c;
    FILETIME ct, x1, x2, x3;
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (p) { if (GetProcessTimes(p, &ct, &x1, &x2, &x3)) c = ((LONGLONG)ct.dwHighDateTime << 32) | ct.dwLowDateTime; CloseHandle(p); }
    return c;
}

static void DayCheck(PTAB *t) {                          // totals cover the current local day
    SYSTEMTIME st; GetLocalTime(&st);
    if (t->day && t->day != st.wDay) { ZeroMemory(t->e, t->cap * sizeof(PTENT)); t->n = 0; t->nrow = 0; }
    t->day = st.wDay;
}

static PTENT *Find(PTAB *t, DWORD key) {                 // slot of key, or the empty slot where it belongs
    UINT m = t->cap - 1, h = Hash(key) & m;
    while (t->e[h].key && t->e[h].key != key) h = (h + 1) & m;
    return &t->e[h];
}

static void Del(PTAB *t, int i) {                        // linear-probing delete with backward shift
    UINT m = t->cap - 1;
    for (int j = i;;) {
        j = (j + 1) & m;
        if (!t->e[j].key) break;
        int h = Hash(t->e[j].key) & m;
        if (i <= j ? (i < h && h <= j) : (i < h || h <= j)) continue;   // home slot in (i, j]: stays
        t->e[i] = t->e[j]; i = j;
    }
    t->e[i].key = 0; t->n--;
}

static void Evict(PTAB *t) {                             // drop the least recently active quarter, exited first
    static WORD idx[1024]; int n = 0;
    for (int i = 0; i < t->cap && n < 1024; i++) if (t->e[i].key) {
        PTENT *e = &t->e[i];
        if (CreateOf(e->key - 1) != e->create) e->last = 0;   // exited or reused: oldest
        int k = n++;                                     // insertion sort by last asc
        while (k > 0 && t->e[idx[k - 1]].last > e->last) { idx[k] = idx[k - 1]; k--; }
        idx[k] = (WORD)i;
    }
    int drop = n / 4; if (!drop) return;
    ULONGLONG T = t->e[idx[drop - 1]].last;              // evict entries with last <= T (at most drop)
    for (int i = 0; i < t->cap && drop; i++)
        while (t->e[i].key && t->e[i].last <= T && drop) { Del(t, i); drop--; }
}

void PtAdd(PTAB *t, DWORD pid, LONGLONG a, LONGLONG b) {
    if (!t->cap || (a <= 0 && b <= 0)) return;
    DayCheck(t);
    DWORD key = pid + 1;
    PTENT *e = Find(t, key);
    if (e->key) {                                        // pid reused by a new process: start over
        LONGLONG ct = CreateOf(pid);
        if (ct && e->create && ct != e->create) { e->ta = e->tb = 0; e->name[0] = 0; e->create = ct; }
        else if (!e->create) e->create = ct;
    } else {
        if (t->n >= t->cap * 3 / 4) { Evict(t); e = Find(t, key); }
        ZeroMemory(e, sizeof(*e)); e->key = key; e->create = Created(pid); t->n++;
    }
    if (e->last != tickNow) e->ka = e->kb = 0;
    if (a > 0) { e->ka += a; e->ta += a; }
    if (b > 0) { e->kb += b; e->tb += b; }
    e->last = tickNow;
}

static BOOL Before(const PTAB *t, const PTROW *r, const PTENT *e) {   // r sorts after e
    if (t->sumSort) return r->ta + r->tb < e->ta + e->tb;
    return r->ta < e->ta || (r->ta == e->ta && r->tb < e->tb);
}

void PtFinish(PTAB *t, int rows) {
    if (!t->cap) return;
    DayCheck(t);
    LONGLONG dt = TickDt();
    int n = 0;
    for (int i = 0; i < t->cap; i++) if (t->e[i].key) {
        PTENT *e = &t->e[i];
        BOOL now = e->last == tickNow;
        if (now) { const WCHAR *nm = NameOf(e->key - 1); if (nm[0] != '?') lstrcpynW(e->name, nm, NAME_LEN); }
        if (!e->ta && !e->tb) continue;
        int k = TopPush(t->row, &n, rows, [&](const PTROW &r) { return Before(t, &r, e); });
        if (k < 0) continue;
        PTROW *r = &t->row[k];
        r->pid = e->key - 1; r->ta = e->ta; r->tb = e->tb;
        r->ra = now ? e->ka * 1000 / dt : 0; r->rb = now ? e->kb * 1000 / dt : 0;
        r->live = tickNow - e->last < PT_IDLE_MS;
        lstrcpynW(r->name, e->name[0] ? e->name : NameOf(r->pid), NAME_LEN);
    }
    t->nrow = n;
}
