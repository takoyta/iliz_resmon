// RAM: GlobalMemoryStatusEx + K32GetPerformanceInfo (kernel32, no psapi.lib).
#include "common.h"
#include "mem.h"
#include <psapi.h>

MEMINFO mem;

void SampleMem() {
    MEMORYSTATUSEX ms = {sizeof(ms)};
    if (GlobalMemoryStatusEx(&ms)) {
        mem.load = ms.dwMemoryLoad; mem.total = ms.ullTotalPhys; mem.avail = ms.ullAvailPhys;
        mem.commitLimit = ms.ullTotalPageFile; mem.commit = ms.ullTotalPageFile - ms.ullAvailPageFile;
    }
    PERFORMANCE_INFORMATION pi = {sizeof(pi)};
    if (K32GetPerformanceInfo(&pi, sizeof(pi))) {
        ULONGLONG pg = pi.PageSize;
        mem.cache = (ULONGLONG)pi.SystemCache * pg;
        mem.commit = (ULONGLONG)pi.CommitTotal * pg; mem.commitLimit = (ULONGLONG)pi.CommitLimit * pg;
    }
}
