#pragma once
#include "common.h"
void FmtRate(WCHAR *o, LONGLONG b);        // bytes/s -> "0" / "512 Б/с" / "12 КБ/с" / "1.2 МБ/с"
void FmtBytes(WCHAR *o, ULONGLONG b);      // "512 КБ" / "12 МБ" / "1.2 ГБ" / "1.1 ТБ"
void FmtSize(WCHAR *o, ULONGLONG b);       // disk volumes, whole GB: "123 ГБ" / "1.2 ТБ"
