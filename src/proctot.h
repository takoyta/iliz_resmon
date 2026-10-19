// Per-process cumulative counters for today (two byte counters a/b, e.g. rx/tx or read/write):
// totals reset at local 00:00, kept for exited processes (with their last known name), current rates,
// and a top list for the popups. One PTAB per source (network trace, each physical disk, ...).
#pragma once
#include "common.h"
#define PT_IDLE_MS 5000                    // a row is "live" with traffic in the last 5 s (any interval)
typedef struct {
    DWORD pid; WCHAR name[NAME_LEN];
    LONGLONG ra, rb;                       // bytes/s this tick (0 if idle)
    ULONGLONG ta, tb;                      // today
    int live;
} PTROW;
typedef struct PTENT_ PTENT;
typedef struct {
    PTENT *e; int cap, n;                  // open-addressing table, cap = power of 2 (heap)
    int sumSort;                           // top list order: 0 = by ta then tb, 1 = by ta + tb
    WORD day;                              // local day of the totals
    DWORD tag;                             // per-disk tables: hash of the drive model (rescan check)
    PTROW row[TOPN_MAX]; int nrow;         // top list (PtFinish)
} PTAB;
extern PTAB ptNet;                         // ETW TCP/UDP: a = received, b = sent
#define PT_DISKS 32                        // per-disk tables by PhysicalDrive number (InitDisks scans 0..31)
extern PTAB *ptDisk[PT_DISKS];             // ETW disk I/O per drive: a = read, b = written; created on first traffic
extern PTAB ptIo;                          // process I/O counters: a = read, b = written (all I/O: files, devices, network)
void PtInit();                             // allocate the tables
void PtAdd(PTAB *t, DWORD pid, LONGLONG a, LONGLONG b);   // bytes moved this tick by pid
void PtFinish(PTAB *t, int rows);          // after SampleProcs: names, rates, top rows (<= TOPN_MAX)
PTAB *PtDisk(int num);                     // table of PhysicalDrive<num> (created if that drive is listed), or 0
void PtDisksFinish(int rows);              // PtFinish for every per-disk table
void PtDisksSync();                        // after ReinitDisks: drop tables of drives that left or changed
int PtDiskRows();                          // rows per disk table: top_n split over the drives, at least 2/5 of it
