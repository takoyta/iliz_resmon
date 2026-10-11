// Popup drawing primitives (DPI-scaled rows, bars, graph, list rows).
#pragma once
#include "common.h"
#include "theme.h"

extern int dpi;                            // DPI of the popup's monitor
#define SC(v) MulDiv((v), dpi, 96)
#define ROWH SC(20)
#define PAD SC(10)
#define ROW_TOP SC(5)                      // top margin of row 0
#define GRAPH_ROWS 3                       // graph height = 3 rows

typedef struct { HDC dc; int L, R, row; COLORREF cMain, cText, cDim; HBRUSH bar, ghost; } PCTX;

HFONT DrawFont();                          // font for current dpi (recreated on change)
int RowY(const PCTX *c);                   // top y of the current row
void Text(PCTX *c, int l, int r, const WCHAR *s, COLORREF col, UINT al);
void Bar(PCTX *c, int x0, int w, int inset, HBRUSH br);
void RxTx(PCTX *c, LONGLONG rx, LONGLONG tx, COLORREF cRx, COLORREF cTx);   // "↓ rx  ↑ tx" right-aligned
// Graph over GRAPH_ROWS rows. Series 0..1000, n samples, newest right; l2 drawn on top if l2Top.
void PaintGraph(PCTX *c, const int *fill, const int *l2, int n, COLORREF c2, int l2Top, const WCHAR *topLabel);
// Process row: bar (v relative to full) from x0 + name up to nameR. Returns the text color (ghost-aware).
COLORREF ListRow(PCTX *c, int x0, int nameR, const WCHAR *name, LONGLONG v, LONGLONG full, int inset, int live);
