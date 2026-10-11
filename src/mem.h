#pragma once
typedef struct {
    int load;                                  // used, %
    ULONGLONG total, avail, cache;             // bytes; cache = 0 if unknown
    ULONGLONG commit, commitLimit;             // bytes
} MEMINFO;
extern MEMINFO mem;
void SampleMem();
