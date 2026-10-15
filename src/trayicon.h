#pragma once
#include "common.h"
void InitTray();
void LoadTraySettings();                   // read iliz_resmon.ini (before the first AddIcons)
int TrayInterval();                        // tick period from the ini, ms
void ReloadTraySettings();                 // reread ini, remove and re-add icons
void AddIcons();                           // also after Explorer restart; only enabled icons
void UpdateIcons();
void RemoveIcons();
