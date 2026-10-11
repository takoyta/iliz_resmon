#pragma once
void FmtRate(WCHAR *o, LONGLONG b);        // bytes/s -> "12.3 МБ/с"
void FmtSize(WCHAR *o, ULONGLONG b);       // bytes -> "123 ГБ" / "1.2 ТБ"
void FmtMB(WCHAR *o, ULONG64 b);           // < 1 ГБ as "N МБ", else FmtSize
void FmtBytes(WCHAR *o, ULONGLONG b);     // "512 КБ" / "12 МБ" / "1.2 ГБ"
void FmtMem(WCHAR *o, ULONGLONG b);       // "512 МБ" / "3.4 ГБ"
