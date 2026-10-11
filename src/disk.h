#pragma once
#include <winioctl.h>
#define MAXD 16
typedef struct {
    int num; HANDLE h; WCHAR model[48]; WCHAR letters[27];
    DISK_PERFORMANCE p0;
    int busy; LONGLONG rps, wps; ULONG queue;              // busy %, bytes/s
} DISK;
extern DISK disks[MAXD];
extern int ndisk;
void InitDisks();
void SampleDisks();
