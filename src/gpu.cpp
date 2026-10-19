// GPU load and video memory via PDH (no admin needed):
//   \GPU Engine(*)\Utilization Percentage   pid_1234_luid_0x00000000_0x0000D2F1_phys_0_eng_0_engtype_3D
//     summed over processes per engine (adapter + phys_N + eng_N), max per (adapter, engine type);
//     load = busiest type (as Task Manager).
//   \GPU Adapter Memory(*)\Dedicated|Shared Usage   luid_..._phys_0   bytes in use per adapter
//   \GPU Process Memory(*)\Dedicated|Shared Usage   pid_..._luid_..._phys_0   per process (top list; own query,
//     open only while the GPU popup is shown: thousands of instances)
// Memory totals come from DXGI (DedicatedVideoMemory / SharedSystemMemory), matched by LUID; reread (and the tables
// rebuilt) when an unknown LUID shows up, e.g. after a driver reset, at most every 30 s.
// pdh.dll and dxgi.dll are loaded on demand (the GPU icon is off by default), so they cost nothing while disabled.
#include "common.h"
#include "gpu.h"
#include "procs.h"
#include <pdh.h>
#include <pdhmsg.h>
#include <dxgi.h>

typedef PDH_STATUS (WINAPI *OpenQ_t)(LPCWSTR, DWORD_PTR, PDH_HQUERY*);
typedef PDH_STATUS (WINAPI *AddC_t)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER*);
typedef PDH_STATUS (WINAPI *Collect_t)(PDH_HQUERY);
typedef PDH_STATUS (WINAPI *GetArr_t)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD, PPDH_FMT_COUNTERVALUE_ITEM_W);
typedef PDH_STATUS (WINAPI *CloseQ_t)(PDH_HQUERY);
typedef HRESULT (WINAPI *CreateFactory_t)(REFIID, void**);
static HMODULE pdh; static OpenQ_t pOpen; static AddC_t pAdd; static Collect_t pCollect; static GetArr_t pGetArr; static CloseQ_t pClose;
static PDH_HQUERY q, qp;                                  // qp: process memory, only while the popup is open
enum { C_ENG, C_AD_DED, C_AD_SH, C_PR_DED, C_PR_SH, C_N };
static PDH_HCOUNTER ctr[C_N];
static const WCHAR *const ctrPath[C_N] = {
    L"\\GPU Engine(*)\\Utilization Percentage",
    L"\\GPU Adapter Memory(*)\\Dedicated Usage", L"\\GPU Adapter Memory(*)\\Shared Usage",
    L"\\GPU Process Memory(*)\\Dedicated Usage", L"\\GPU Process Memory(*)\\Shared Usage",
};
static BYTE *buf; static DWORD bufSize;
#define IGPU_DEDICATED (512ull << 20)                    // less dedicated memory than this: integrated, show shared
#define REBUILD_MS 30000
#define MAXE 64                                          // engines (adapter x phys x eng_N)
typedef struct { WCHAR key[48]; int g; LONGLONG sum; } RENG;   // key "luid_..._phys_0_eng_3", g: index in geng, sum: % x 1000
static RENG reng[MAXE]; static int nreng;
static ULONGLONG rebuildAt;

GENG geng[MAXG]; GADAPTER gad[MAXA]; GPROC gproc[VPROC_MAX];
int ngeng, nGpuAd, ngproc, gpuLoad, gpuOk, gpuAd = -1, vramPct;

static void ProcQuery(BOOL on) {                         // separate query for \GPU Process Memory
    if (on && !qp && pOpen(0, 0, &qp) == ERROR_SUCCESS) {
        for (int i = C_PR_DED; i <= C_PR_SH; i++) if (pAdd(qp, ctrPath[i], 0, &ctr[i]) != ERROR_SUCCESS) ctr[i] = 0;
    } else if (!on && qp) { pClose(qp); qp = 0; ctr[C_PR_DED] = ctr[C_PR_SH] = 0; }
    if (!on) ngproc = 0;
}

static void Off() {
    if (q) { ProcQuery(FALSE); pClose(q); }
    if (pdh) FreeLibrary(pdh);
    q = 0; pdh = 0; ngeng = nreng = nGpuAd = ngproc = gpuLoad = gpuOk = vramPct = 0; gpuAd = -1;
    if (buf) HeapFree(GetProcessHeap(), 0, buf);
    buf = 0; bufSize = 0;
}

static int AdFind(const WCHAR *luid, BOOL add) {        // adapter index by LUID string, -1 if absent / table full
    for (int i = 0; i < nGpuAd; i++) if (!lstrcmpiW(gad[i].luid, luid)) return i;
    if (!add || nGpuAd >= MAXA) return -1;
    GADAPTER *a = &gad[nGpuAd]; ZeroMemory(a, sizeof(*a)); lstrcpynW(a->luid, luid, 24);
    return nGpuAd++;
}

static void ReadDxgi() {                                 // memory sizes per hardware adapter
    HMODULE dx = LoadLibraryW(L"dxgi.dll"); if (!dx) return;
    CreateFactory_t create = (CreateFactory_t)GetProcAddress(dx, "CreateDXGIFactory1");
    IDXGIFactory1 *f = 0;
    if (create && SUCCEEDED(create(__uuidof(IDXGIFactory1), (void**)&f)) && f) {
        IDXGIAdapter1 *a;
        for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_ADAPTER_DESC1 d;
            if (SUCCEEDED(a->GetDesc1(&d)) && !(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                WCHAR l[24]; wsprintfW(l, L"0x%08X_0x%08X", (UINT)d.AdapterLuid.HighPart, (UINT)d.AdapterLuid.LowPart);
                int k = AdFind(l, TRUE);
                if (k >= 0) {
                    gad[k].shared = d.DedicatedVideoMemory < IGPU_DEDICATED;
                    gad[k].total = gad[k].shared ? d.SharedSystemMemory : d.DedicatedVideoMemory;
                }
            }
            a->Release();
        }
        f->Release();
    }
    FreeLibrary(dx);
}

static void Rebuild() {                                  // fresh adapter + engine tables from DXGI
    nreng = ngeng = nGpuAd = 0;
    ReadDxgi();
    gpuAd = -1;                                          // start with the adapter with the most memory
    for (int i = 0; i < nGpuAd; i++) if (gpuAd < 0 || gad[i].total > gad[gpuAd].total) gpuAd = i;
    rebuildAt = GetTickCount64() + REBUILD_MS;
}

void GpuEnable(BOOL on) {
    if (!on) { Off(); return; }
    if (q) return;
    pdh = LoadLibraryW(L"pdh.dll"); if (!pdh) return;
    pOpen = (OpenQ_t)GetProcAddress(pdh, "PdhOpenQueryW");
    pAdd = (AddC_t)GetProcAddress(pdh, "PdhAddEnglishCounterW");
    pCollect = (Collect_t)GetProcAddress(pdh, "PdhCollectQueryData");
    pGetArr = (GetArr_t)GetProcAddress(pdh, "PdhGetFormattedCounterArrayW");
    pClose = (CloseQ_t)GetProcAddress(pdh, "PdhCloseQuery");
    if (!pOpen || !pAdd || !pCollect || !pGetArr || !pClose || pOpen(0, 0, &q) != ERROR_SUCCESS) { q = 0; Off(); return; }
    if (pAdd(q, ctrPath[C_ENG], 0, &ctr[C_ENG]) != ERROR_SUCCESS) { Off(); return; }
    for (int i = C_AD_DED; i <= C_AD_SH; i++) if (pAdd(q, ctrPath[i], 0, &ctr[i]) != ERROR_SUCCESS) ctr[i] = 0;   // memory: optional
    Rebuild();
    pCollect(q);                                         // baseline: rates need two samples
}

static const WCHAR *Find(const WCHAR *s, const WCHAR *sub) {   // pointer after sub in s, or 0
    for (; *s; s++) {
        int k = 0; while (sub[k] && s[k] == sub[k]) k++;
        if (!sub[k]) return s + k;
    }
    return 0;
}

static void Luid(WCHAR *d, const WCHAR *name) {          // "0x00000000_0x0000D2F1" from ..._luid_<this>_phys_...
    const WCHAR *ls = Find(name, L"luid_"), *le = ls ? Find(ls, L"_phys") : 0;
    int k = 0;
    if (ls) for (const WCHAR *e = le ? le - 5 : ls + lstrlenW(ls); ls + k < e && k < 23; k++) d[k] = ls[k];
    d[k] = 0;
}

static PPDH_FMT_COUNTERVALUE_ITEM_W Arr(int c, DWORD fmt, DWORD *n) {   // items in buf (valid until the next call)
    *n = 0;
    if (!ctr[c]) return 0;
    DWORD sz = bufSize;
    PDH_STATUS st = pGetArr(ctr[c], fmt, &sz, n, (PPDH_FMT_COUNTERVALUE_ITEM_W)buf);
    if (st == (PDH_STATUS)PDH_MORE_DATA) {
        BYTE *nb = (BYTE*)(buf ? HeapReAlloc(GetProcessHeap(), 0, buf, sz + 4096) : HeapAlloc(GetProcessHeap(), 0, sz + 4096));
        if (!nb) { *n = 0; return 0; }
        buf = nb; bufSize = sz + 4096; sz = bufSize;
        st = pGetArr(ctr[c], fmt, &sz, n, (PPDH_FMT_COUNTERVALUE_ITEM_W)buf);
    }
    if (st != ERROR_SUCCESS) { *n = 0; return 0; }
    return (PPDH_FMT_COUNTERVALUE_ITEM_W)buf;
}

static BOOL Valid(const PDH_FMT_COUNTERVALUE_ITEM_W *it) {
    return it->FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || it->FmtValue.CStatus == PDH_CSTATUS_NEW_DATA;
}

static void Engines() {
    DWORD n; PPDH_FMT_COUNTERVALUE_ITEM_W it = Arr(C_ENG, PDH_FMT_LARGE | PDH_FMT_1000, &n);   // x1000: keep < 1 % per process
    gpuOk = it != 0;
    BOOL stale = FALSE;
    for (int r = 0; r < nreng; r++) reng[r].sum = 0;
    for (DWORD i = 0; i < n; i++) {
        if (!Valid(&it[i])) continue;
        const WCHAR *ls = Find(it[i].szName, L"luid_"), *te = ls ? Find(ls, L"_engtype_") : 0;
        if (!te) continue;
        WCHAR key[48]; int kl = (int)(te - 9 - (ls - 5)); if (kl > 47) kl = 47;
        lstrcpynW(key, ls - 5, kl + 1);
        int r = 0;
        for (; r < nreng; r++) if (!lstrcmpW(reng[r].key, key)) break;
        if (r == nreng) {                                // new engine: find its adapter and type row
            if (nreng >= MAXE) continue;
            WCHAR luid[24], type[24]; Luid(luid, it[i].szName);
            lstrcpynW(type, *te ? te : L"?", 24);
            int ad = AdFind(luid, FALSE);
            if (ad < 0) {                                // unknown LUID: driver reset / new adapter -> reread DXGI
                if (GetTickCount64() >= rebuildAt) { stale = TRUE; continue; }
                ad = AdFind(luid, TRUE); if (ad < 0) continue;   // not in DXGI: keep it without a memory size
            }
            int g = 0;
            for (; g < ngeng; g++) if (geng[g].ad == ad && !lstrcmpW(geng[g].type, type)) break;
            if (g == ngeng) {
                if (ngeng >= MAXG) continue;
                GENG *e = &geng[ngeng++]; lstrcpyW(e->luid, luid); lstrcpyW(e->type, type); e->v = 0; e->ad = ad;
            }
            RENG *e = &reng[nreng++]; lstrcpyW(e->key, key); e->g = g; e->sum = 0;
        }
        reng[r].sum += it[i].FmtValue.largeValue;
    }
    if (stale) { Rebuild(); return; }                    // load 0 for this tick, tables filled on the next one
    for (int r = 0; r < nreng; r++) {                    // a type is as busy as its busiest engine
        int v = (int)((reng[r].sum + 500) / 1000);
        if (v > 100) v = 100;
        if (v > geng[reng[r].g].v) geng[reng[r].g].v = v;
    }
    int busy = -1;
    for (int g = 0; g < ngeng; g++) if (geng[g].v > gpuLoad) { gpuLoad = geng[g].v; busy = geng[g].ad; }
    if (busy >= 0 && gad[busy].total) gpuAd = busy;      // memory follows the adapter that carries the load; idle: keep
}

static void AdapterMem() {
    for (int s = 0; s < 2; s++) {
        DWORD n; PPDH_FMT_COUNTERVALUE_ITEM_W it = Arr(s ? C_AD_SH : C_AD_DED, PDH_FMT_LARGE, &n);
        for (int a = 0; a < nGpuAd; a++) if (gad[a].shared == s) gad[a].used = 0;
        for (DWORD i = 0; i < n; i++) {
            if (!Valid(&it[i])) continue;
            WCHAR luid[24]; Luid(luid, it[i].szName);
            int a = AdFind(luid, FALSE);
            if (a >= 0 && gad[a].shared == s) gad[a].used += (ULONGLONG)it[i].FmtValue.largeValue;   // all phys_N of the adapter
        }
    }
    vramPct = 0;
    if (gpuAd >= 0 && gad[gpuAd].total) {
        vramPct = (int)(gad[gpuAd].used * 100 / gad[gpuAd].total);
        if (vramPct > 100) vramPct = 100;
    }
}

static void ProcMem() {                                  // top processes on the shown adapter, its memory pool
    ngproc = 0;
    if (gpuAd < 0) return;
    DWORD n; PPDH_FMT_COUNTERVALUE_ITEM_W it = Arr(gad[gpuAd].shared ? C_PR_SH : C_PR_DED, PDH_FMT_LARGE, &n);
    for (DWORD i = 0; i < n; i++) {
        if (!Valid(&it[i]) || it[i].FmtValue.largeValue <= 0) continue;
        WCHAR luid[24]; Luid(luid, it[i].szName);
        if (lstrcmpiW(luid, gad[gpuAd].luid)) continue;
        const WCHAR *p = Find(it[i].szName, L"pid_"); if (!p) continue;
        DWORD pid = 0; while (*p >= '0' && *p <= '9') pid = pid * 10 + (*p++ - '0');
        ULONGLONG b = (ULONGLONG)it[i].FmtValue.largeValue;
        int k = 0;
        for (; k < ngproc; k++) if (gproc[k].pid == pid) break;   // same pid on several phys_N: sum
        if (k < ngproc) b += gproc[k].bytes;
        else if (ngproc < VPROC_ROWS) k = ngproc++;
        else if (b > gproc[VPROC_ROWS - 1].bytes) k = VPROC_ROWS - 1;
        else continue;
        gproc[k].pid = pid; gproc[k].bytes = b;
        while (k > 0 && gproc[k - 1].bytes < gproc[k].bytes) { GPROC t = gproc[k]; gproc[k] = gproc[k - 1]; gproc[k - 1] = t; k--; }
    }
    for (int k = 0; k < ngproc; k++) lstrcpynW(gproc[k].name, NameOf(gproc[k].pid), NAME_LEN);
}

void SampleGpu() {
    if (!q) return;
    for (int i = 0; i < ngeng; i++) geng[i].v = 0;
    gpuLoad = 0;
    pCollect(q);
    Engines();
    AdapterMem();
    ProcQuery(popMode == ID_GPU);                        // the list is only shown in the popup
    if (qp) { pCollect(qp); ProcMem(); }
}
