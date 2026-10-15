// GPU load via PDH "\GPU Engine(*)\Utilization Percentage" (no admin needed). Instances look like
// pid_1234_luid_0x0000_0x0000D2F1_phys_0_eng_0_engtype_3D: summed per (adapter, engine type), load = max type.
// pdh.dll is loaded on demand (the GPU icon is off by default), so it costs nothing while disabled.
#include "common.h"
#include "gpu.h"
#include <pdh.h>
#include <pdhmsg.h>

typedef PDH_STATUS (WINAPI *OpenQ_t)(LPCWSTR, DWORD_PTR, PDH_HQUERY*);
typedef PDH_STATUS (WINAPI *AddC_t)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER*);
typedef PDH_STATUS (WINAPI *Collect_t)(PDH_HQUERY);
typedef PDH_STATUS (WINAPI *GetArr_t)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD, PPDH_FMT_COUNTERVALUE_ITEM_W);
typedef PDH_STATUS (WINAPI *CloseQ_t)(PDH_HQUERY);
static HMODULE pdh; static OpenQ_t pOpen; static AddC_t pAdd; static Collect_t pCollect; static GetArr_t pGetArr; static CloseQ_t pClose;
static PDH_HQUERY q; static PDH_HCOUNTER ctr;
static BYTE *buf; static DWORD bufSize;

GENG geng[MAXG];
int ngeng, nGpuAd, gpuLoad, gpuOk;

static void Off() {
    if (q) pClose(q);
    if (pdh) FreeLibrary(pdh);
    q = 0; pdh = 0; ngeng = nGpuAd = gpuLoad = gpuOk = 0;
    if (buf) HeapFree(GetProcessHeap(), 0, buf);
    buf = 0; bufSize = 0;
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
    if (pAdd(q, L"\\GPU Engine(*)\\Utilization Percentage", 0, &ctr) != ERROR_SUCCESS) { Off(); return; }
    pCollect(q);                                         // baseline: rates need two samples
}

static const WCHAR *Find(const WCHAR *s, const WCHAR *sub) {   // pointer after sub in s, or 0
    for (; *s; s++) {
        int k = 0; while (sub[k] && s[k] == sub[k]) k++;
        if (!sub[k]) return s + k;
    }
    return 0;
}

static void Part(WCHAR *d, const WCHAR *s, WCHAR stop, int max) {   // copy up to stop / end
    int k = 0; while (s && s[k] && s[k] != stop && k < max - 1) { d[k] = s[k]; k++; }
    d[k] = 0;
}

void SampleGpu() {
    if (!q) return;
    for (int i = 0; i < ngeng; i++) geng[i].v = 0;
    gpuLoad = 0;
    pCollect(q);
    DWORD sz = bufSize, n = 0;
    PDH_STATUS st = pGetArr(ctr, PDH_FMT_LONG, &sz, &n, (PPDH_FMT_COUNTERVALUE_ITEM_W)buf);
    if (st == (PDH_STATUS)PDH_MORE_DATA) {
        BYTE *nb = (BYTE*)(buf ? HeapReAlloc(GetProcessHeap(), 0, buf, sz + 4096) : HeapAlloc(GetProcessHeap(), 0, sz + 4096));
        if (!nb) return;
        buf = nb; bufSize = sz + 4096; sz = bufSize;
        st = pGetArr(ctr, PDH_FMT_LONG, &sz, &n, (PPDH_FMT_COUNTERVALUE_ITEM_W)buf);
    }
    gpuOk = st == ERROR_SUCCESS;
    if (!gpuOk) return;
    PPDH_FMT_COUNTERVALUE_ITEM_W it = (PPDH_FMT_COUNTERVALUE_ITEM_W)buf;
    for (DWORD i = 0; i < n; i++) {
        if (it[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && it[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
        WCHAR luid[24], type[24];
        const WCHAR *ls = Find(it[i].szName, L"luid_"), *le = ls ? Find(ls, L"_phys") : 0;   // "0x00000000_0x0000D2F1"
        int k = 0;
        if (ls) for (const WCHAR *e = le ? le - 5 : ls + lstrlenW(ls); ls + k < e && k < 23; k++) luid[k] = ls[k];
        luid[k] = 0;
        Part(type, Find(it[i].szName, L"engtype_"), 0, 24);
        if (!type[0]) lstrcpyW(type, L"?");
        int g = 0;
        for (; g < ngeng; g++) if (!lstrcmpW(geng[g].luid, luid) && !lstrcmpW(geng[g].type, type)) break;
        if (g == ngeng) {
            if (ngeng >= MAXG) continue;
            GENG *e = &geng[ngeng++]; lstrcpyW(e->luid, luid); lstrcpyW(e->type, type); e->v = 0;
            e->ad = -1;
            for (int k = 0; k < g; k++) if (!lstrcmpW(geng[k].luid, luid)) { e->ad = geng[k].ad; break; }
            if (e->ad < 0) e->ad = nGpuAd++;
        }
        geng[g].v += it[i].FmtValue.longValue;
    }
    for (int g = 0; g < ngeng; g++) {
        if (geng[g].v > 100) geng[g].v = 100;
        if (geng[g].v > gpuLoad) gpuLoad = geng[g].v;
    }
}
