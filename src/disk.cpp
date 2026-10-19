// Physical disks: IOCTL_DISK_PERFORMANCE counters, model names, drive letter mapping.
#include "common.h"
#include "disk.h"

DISK disks[MAXD];
int ndisk;

static void ReadModel(DISK *d) {
    STORAGE_PROPERTY_QUERY q = {StorageDeviceProperty, PropertyStandardQuery};
    BYTE buf[1024]; DWORD br;
    if (!DeviceIoControl(d->h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf, sizeof(buf), &br, 0)) return;
    STORAGE_DEVICE_DESCRIPTOR *sd = (STORAGE_DEVICE_DESCRIPTOR*)buf;
    if (!sd->ProductIdOffset || sd->ProductIdOffset >= br) return;
    char *s = (char*)buf + sd->ProductIdOffset; int k = 0;
    while (*s == ' ') s++;
    while (s[k] && k < 47) { d->model[k] = (BYTE)s[k]; k++; }
    while (k > 0 && d->model[k - 1] == ' ') k--;
    d->model[k] = 0;
}

static void MapLetters() {
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++) if (mask & (1u << i)) {
        WCHAR root[4] = {(WCHAR)('A' + i), ':', '\\', 0}, vol[8] = L"\\\\.\\A:";
        UINT t = GetDriveTypeW(root);
        if (t != DRIVE_FIXED && t != DRIVE_REMOVABLE) continue;
        vol[4] = (WCHAR)('A' + i);
        HANDLE h = CreateFileW(vol, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        if (h == INVALID_HANDLE_VALUE) continue;
        BYTE buf[256]; DWORD br;
        if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, buf, sizeof(buf), &br, 0)) {
            VOLUME_DISK_EXTENTS *e = (VOLUME_DISK_EXTENTS*)buf;
            for (int k = 0; k < ndisk; k++) if (disks[k].num == (int)e->Extents[0].DiskNumber) {
                int L = lstrlenW(disks[k].letters); disks[k].letters[L] = (WCHAR)('A' + i); disks[k].letters[L + 1] = 0;
            }
        }
        CloseHandle(h);
    }
}

void InitDisks() {
    for (int n = 0; n < 32 && ndisk < MAXD; n++) {
        WCHAR path[32]; wsprintfW(path, L"\\\\.\\PhysicalDrive%d", n);
        HANDLE h = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        if (h == INVALID_HANDLE_VALUE) continue;
        DISK_PERFORMANCE dp; DWORD br;
        if (!DeviceIoControl(h, IOCTL_DISK_PERFORMANCE, 0, 0, &dp, sizeof(dp), &br, 0)) { CloseHandle(h); continue; }
        DISK *d = &disks[ndisk++];
        d->num = n; d->h = h; d->p0 = dp;
        ReadModel(d);
    }
    MapLetters();
}

void ReinitDisks() {                                     // device or volume arrived / left: rebuild the list
    for (int i = 0; i < ndisk; i++) CloseHandle(disks[i].h);
    ZeroMemory(disks, sizeof(disks)); ndisk = 0;
    InitDisks();
}

static LONGLONG PerSec(LONGLONG d, LONGLONG qt) {        // d * 10^7 / qt (qt in 100 ns) without overflow
    if (d <= 0 || qt <= 0) return 0;
    LONGLONG q = d / qt, r = d % qt;
    while (qt > 1000000000) { qt >>= 1; r >>= 1; }       // r < qt: r * 10^7 stays < 10^16
    return q * 10000000 + r * 10000000 / qt;
}

void SampleDisks() {
    for (int i = 0; i < ndisk; i++) {
        DISK *d = &disks[i]; DISK_PERFORMANCE p; DWORD br;
        if (!DeviceIoControl(d->h, IOCTL_DISK_PERFORMANCE, 0, 0, &p, sizeof(p), &br, 0)) { d->busy = 0; continue; }
        LONGLONG qt = p.QueryTime.QuadPart - d->p0.QueryTime.QuadPart;       // 100ns units
        LONGLONG it = p.IdleTime.QuadPart - d->p0.IdleTime.QuadPart;
        int b = qt > 0 ? (int)(100 - it * 100 / qt) : 0;
        d->busy = b < 0 ? 0 : b > 100 ? 100 : b;
        d->rps = PerSec(p.BytesRead.QuadPart - d->p0.BytesRead.QuadPart, qt);
        d->wps = PerSec(p.BytesWritten.QuadPart - d->p0.BytesWritten.QuadPart, qt);
        d->queue = p.QueueDepth;
        d->p0 = p;
    }
}
