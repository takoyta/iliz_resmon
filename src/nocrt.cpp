// No C runtime (MSVC build): entry point + the few functions the compiler emits calls to.
// Built with /NODEFAULTLIB /ENTRY:WinMainEntry /GS- and no /GL, so these are not turned into recursive calls.
#ifdef _MSC_VER
#include "common.h"
#include <intrin.h>

extern "C" {
#pragma function(memset, memcpy, memcmp)
void *__cdecl memset(void *d, int c, size_t n) { __stosb((unsigned char*)d, (unsigned char)c, n); return d; }
void *__cdecl memcpy(void *d, const void *s, size_t n) { __movsb((unsigned char*)d, (const unsigned char*)s, n); return d; }
int __cdecl memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p = (const unsigned char*)a, *q = (const unsigned char*)b;
    for (; n; n--, p++, q++) if (*p != *q) return *p - *q;
    return 0;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int);

extern "C" void WinMainEntry() {                       // no CRT init needed: no static constructors, no floats
    ExitProcess(wWinMain(GetModuleHandleW(0), 0, GetCommandLineW(), SW_SHOWDEFAULT));
}
#endif
