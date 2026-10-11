// Text formatting helpers (rates, sizes).
#include "format.h"

void FmtRate(WCHAR *o, LONGLONG b) {
    if (b >= 1048576) wsprintfW(o, L"%d.%d МБ/с", (int)(b / 1048576), (int)(b % 1048576 * 10 / 1048576));
    else if (b >= 1024) wsprintfW(o, L"%d КБ/с", (int)(b / 1024));
    else if (b > 0) wsprintfW(o, L"%d Б/с", (int)b);
    else lstrcpyW(o, L"0");
}

static void Dec(WCHAR *o, ULONGLONG b, int sh, const WCHAR *unit) {   // "X.Y unit" for b / 2^sh
    wsprintfW(o, L"%d.%d %s", (int)(b >> sh), (int)((b & ((1ull << sh) - 1)) * 10 >> sh), unit);
}

void FmtBytes(WCHAR *o, ULONGLONG b) {
    if (b < (1ull << 20)) wsprintfW(o, L"%d КБ", (int)(b >> 10));
    else if (b < (1ull << 30)) wsprintfW(o, L"%d МБ", (int)(b >> 20));
    else if (b < (1ull << 40)) Dec(o, b, 30, L"ГБ");
    else Dec(o, b, 40, L"ТБ");
}

void FmtSize(WCHAR *o, ULONGLONG b) {
    if (b < (1ull << 40)) wsprintfW(o, L"%d ГБ", (int)(b >> 30)); else Dec(o, b, 40, L"ТБ");
}
