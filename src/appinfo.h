#pragma once
#define APP_NAME    L"iliz resmon"
#define APP_ID      L"iliz_resmon"          // window class, mutex, ETW session, registry value
#include "version.h"
#define APP_WIDE_(s) L##s
#define APP_WIDE(s) APP_WIDE_(s)
#define APP_VERSION APP_WIDE(APP_VERSION_A)
#define APP_DESC    L"Загрузка CPU, памяти, дисков и сети в трее"
