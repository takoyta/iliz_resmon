// Tick pipeline: Sample -> Derive -> Render. Order matters, see comments.
#include "common.h"
#include "app.h"
#include "cpu.h"
#include "mem.h"
#include "disk.h"
#include "net.h"
#include "etw.h"
#include "procs.h"
#include "sticky.h"
#include "history.h"
#include "trayicon.h"
#include "popup.h"

void AppInit() { InitCpu(); InitDisks(); EtwInit(); EtwStart(); }

void AppSample() {
    SampleCpu();                           // before SampleProcs: provides sysDelta for per-process CPU %
    SampleMem(); SampleDisks(); SampleNet();
    EtwTick();                             // per-disk / per-process net from the trace
    SampleProcs();                         // after EtwTick: NameOf() for trace pids uses this snapshot
}

void AppTick() {
    tickPrev = tickNow; tickNow = GetTickCount64();
    AppSample();
    StickyTick();                          // derive: needs procs + ETW tops of this tick
    PushHistory();                         // derive: needs CPU, mem, net
    UpdateIcons();                         // render
    if (popMode) PopupTick();
}
