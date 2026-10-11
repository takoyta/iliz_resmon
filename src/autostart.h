#pragma once
#include "common.h"
BOOL IsElevated();
BOOL AutostartEnabled();                   // HKCU\...\Run value "iliz_resmon" exists
void AutostartSet(BOOL on);                // write quoted exe path / delete the value (no admin needed)
void RelaunchElevated();                   // "runas" self; the new instance takes over
