#pragma once
#include "common.h"
BOOL IsElevated();
void RelaunchElevated();                   // "runas" self; the new instance takes over
