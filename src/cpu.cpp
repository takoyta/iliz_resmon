// Per-core CPU load via NtQuerySystemInformation(SystemProcessorPerformanceInformation).
#include "common.h"
#include "cpu.h"

typedef struct { LARGE_INTEGER Idle, Kernel, User, R1[2]; ULONG R2; } SPPI;
static SPPI cur[MAXC], prev[MAXC];
int ncpu, load[MAXC];
LONGLONG sysDelta;

void InitCpu() {
    SYSTEM_INFO si; GetSystemInfo(&si);
    ncpu = si.dwNumberOfProcessors > MAXC ? MAXC : si.dwNumberOfProcessors;
}

void SampleCpu() {
    ULONG len; LONGLONG tot = 0;
    NtQSI(8, cur, sizeof(SPPI) * ncpu, &len);
    for (int i = 0; i < ncpu; i++) {
        LONGLONG idle = cur[i].Idle.QuadPart - prev[i].Idle.QuadPart;
        LONGLONG t = (cur[i].Kernel.QuadPart - prev[i].Kernel.QuadPart) +
                     (cur[i].User.QuadPart - prev[i].User.QuadPart);   // kernel includes idle
        load[i] = t > 0 ? (int)((t - idle) * 100 / t) : 0;
        if (load[i] < 0) load[i] = 0;
        tot += t; prev[i] = cur[i];
    }
    sysDelta = tot;
}

int AvgLoad() { int s = 0; for (int i = 0; i < ncpu; i++) s += load[i]; return s / ncpu; }
