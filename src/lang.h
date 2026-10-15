// UI strings, English and Russian. Language = Windows UI language (Russian -> ru, anything else -> en).
// Shared by iliz_resmon.exe and iliz_resmon_config.exe (src\lang.cpp is linked into both).
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

enum StrId {
    S_DESC,
    S_KB,
    S_MB,
    S_GB,
    S_TB,
    S_BPS,
    S_KBPS,
    S_MBPS,
    S_5MIN,
    S_UPD_FAIL,
    S_ETW_OFF,
    S_ETW_ON,
    S_ETW_FAIL,
    S_ETW_RESTART,
    S_ABOUT,
    S_ABOUT_ERR,
    S_ABOUT_LOST,
    S_M_ADMIN,
    S_M_CONFIG,
    S_M_UPDATE_TO,
    S_M_CHECK,
    S_M_EXIT,
    S_CRASH,
    S_KERNEL,
    S_TIP_MEM,
    S_TIP_DISK,
    S_TIP_NET,
    S_QUIET,
    S_MEASURING,
    S_CPU_HDR,
    S_MEM_HDR,
    S_AVAIL,
    S_CACHE,
    S_OF,
    S_COMMIT,
    S_PROCS,
    S_VOL,
    S_DPROC_QUIET,
    S_DISK_LINE,
    S_NO_NIC,
    S_NIC_TOT,
    S_SPEED,
    S_TOTAL,
    S_SILENCE,
    S_CFG_TITLE,
    S_CFG_GROUP,
    S_CFG_MEM,
    S_CFG_DISK,
    S_CFG_NET,
    S_CFG_AUTO,
    S_CFG_CLOSE,
    S_CFG_AUTO_FAIL,
    S_CFG_SAVE_FAIL,
    S_COUNT
};

const WCHAR *T(int id);                    // picks the table on first use
