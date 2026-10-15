// History window: the last 60 minutes of one metric, opened by a left click on its tray icon.
#pragma once
#include "common.h"
void HistWndShow(int id);                  // ID_*; reuses the open window (switches metric)
void HistWndTick();                        // every timer tick: repaint if open
