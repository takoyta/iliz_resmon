#pragma once
BOOL IsElevated();
BOOL AutostartEnabled();                   // scheduled task "CpuTray" exists
void AutostartSet(BOOL on);                // create (XML, highest rights) / delete; UAC prompt if not elevated
void RelaunchElevated();                   // "runas" self; the new instance takes over
