// Popup contents per mode: size, colors, painting.
#pragma once
#include "draw.h"
int ViewWidth(int mode);
int ViewRows(int mode);                    // fixed per state, so the popup doesn't jump
void ViewColors(int mode, COLORREF *cMain, COLORREF *cBar);
const INCOLORS *ViewIn(int mode);       // read / received colours of a two-counter view
void ViewPaint(PCTX *c, int mode);
