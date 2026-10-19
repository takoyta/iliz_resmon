// Text formatting helpers (rates, sizes).
#include "format.h"
#include "lang.h"

void FmtRate(WCHAR *o, LONGLONG b) {
    if (b >= 1048576) wsprintfW(o, L"%d%s%d %s", (int)(b / 1048576), T(S_DEC), (int)(b % 1048576 * 10 / 1048576), T(S_MBPS));
    else if (b >= 1024) wsprintfW(o, L"%d %s", (int)(b / 1024), T(S_KBPS));
    else if (b > 0) wsprintfW(o, L"%d %s", (int)b, T(S_BPS));
    else lstrcpyW(o, L"0");
}

static void Dec(WCHAR *o, ULONGLONG b, int sh, const WCHAR *unit) {   // "X.Y unit" for b / 2^sh
    wsprintfW(o, L"%d%s%d %s", (int)(b >> sh), T(S_DEC), (int)((b & ((1ull << sh) - 1)) * 10 >> sh), unit);
}

void FmtBytes(WCHAR *o, ULONGLONG b) {
    if (b < (1ull << 20)) wsprintfW(o, L"%d %s", (int)(b >> 10), T(S_KB));
    else if (b < (1ull << 30)) wsprintfW(o, L"%d %s", (int)(b >> 20), T(S_MB));
    else if (b < (1ull << 40)) Dec(o, b, 30, T(S_GB));
    else Dec(o, b, 40, T(S_TB));
}

void FmtSize(WCHAR *o, ULONGLONG b) {
    if (b < (1ull << 40)) wsprintfW(o, L"%d %s", (int)(b >> 30), T(S_GB)); else Dec(o, b, 40, T(S_TB));
}
