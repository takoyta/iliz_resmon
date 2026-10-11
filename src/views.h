// Popup contents per mode: size, colors, painting.
#pragma once
#include "draw.h"
int ViewWidth(int mode);
int ViewRows(int mode);                    // fixed per state, so the popup doesn't jump
void ViewColors(int mode, COLORREF *cMain, COLORREF *cBar);
void ViewPaint(PCTX *c, int mode);
