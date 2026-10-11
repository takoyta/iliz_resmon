// Text formatting helpers (rates, sizes).
#include "common.h"
#include "format.h"

void FmtRate(WCHAR *o, LONGLONG b) {
    if (b >= 1048576) wsprintfW(o, L"%d.%d МБ/с", (int)(b / 1048576), (int)(b % 1048576 * 10 / 1048576));
    else if (b >= 1024) wsprintfW(o, L"%d КБ/с", (int)(b / 1024));
    else if (b > 0) wsprintfW(o, L"%d Б/с", (int)b);
    else lstrcpyW(o, L"0");
}

void FmtSize(WCHAR *o, ULONGLONG b) {
    ULONGLONG g = b >> 30;
    if (g >= 1024) wsprintfW(o, L"%d.%d ТБ", (int)(g / 1024), (int)(g % 1024 * 10 / 1024));
    else wsprintfW(o, L"%d ГБ", (int)g);
}

void FmtMB(WCHAR *o, ULONG64 b) {
    if (b >> 30) FmtSize(o, b); else wsprintfW(o, L"%d МБ", (int)(b >> 20));
}

void FmtBytes(WCHAR *o, ULONGLONG b) {
    if (b < (1ull << 20)) wsprintfW(o, L"%d КБ", (int)(b >> 10)); else FmtMem(o, b);
}

void FmtMem(WCHAR *o, ULONGLONG b) {
    if (b < (1ull << 30)) wsprintfW(o, L"%d МБ", (int)(b >> 20));
    else wsprintfW(o, L"%d.%d ГБ", (int)(b >> 30), (int)((b & ((1ull << 30) - 1)) * 10 >> 30));
}
