#pragma once
#include "common.h"
void FmtRate(WCHAR *o, LONGLONG b);        // bytes/s -> "0" / "512 B/s" / "12 KB/s" / "1.2 MB/s" (localized units)
void FmtBytes(WCHAR *o, ULONGLONG b);      // "512 KB" / "12 MB" / "1.2 GB" / "1.1 TB"
void FmtSize(WCHAR *o, ULONGLONG b);       // disk volumes, whole GB: "123 GB" / "1.2 TB"
