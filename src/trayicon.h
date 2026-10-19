#pragma once
#include "common.h"
void InitTray();
void LoadTraySettings();                   // read iliz_resmon.ini (before the first AddIcons)
int TrayInterval();                        // tick period from the ini, ms
void ReloadTraySettings();                 // reread ini, add / remove only the icons that changed
void TraySync();                           // after disks appear / vanish: add or remove the disk icon
void TrayDpiChanged();                     // WM_DISPLAYCHANGE / WM_SETTINGCHANGE / Explorer restart: reread the taskbar DPI
void AddIcons();                           // also after Explorer restart; only enabled icons
void UpdateIcons();
void RemoveIcons();
