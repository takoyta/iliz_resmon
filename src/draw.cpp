// Popup drawing primitives.
#include "draw.h"
#include "lang.h"
#include "history.h"
#include "format.h"

int dpi = 96;
static HFONT hFont; static int fontDpi;

HFONT DrawFont() {
    if (fontDpi != dpi) {
        NONCLIENTMETRICSW ncm = {sizeof(ncm)};
        SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi);
        ncm.lfMessageFont.lfQuality = CLEARTYPE_QUALITY;
        if (hFont) DeleteObject(hFont);
        hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        fontDpi = dpi;
    }
    return hFont;
}

int RowY(const PCTX *c) { return ROW_TOP + ROWH * c->row; }

void Text(PCTX *c, int l, int r, const WCHAR *s, COLORREF col, UINT al) {
    RECT t = {l, RowY(c), r, RowY(c) + ROWH};
    SetTextColor(c->dc, col);
    DrawTextW(c->dc, s, -1, &t, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | al);
}

void Bar(PCTX *c, int x0, int w, int inset, HBRUSH br) {
    int y = RowY(c);
    RECT b = {x0, y + SC(inset), x0 + w, y + ROWH - SC(inset) + SC(2)};
    FillRect(c->dc, &b, br);
}

void RxTx(PCTX *c, LONGLONG rx, LONGLONG tx, COLORREF cRx, COLORREF cTx) {
    WCHAR a[24], s[32]; SIZE sz; int R = c->R;
    FmtRate(a, tx); wsprintfW(s, L"↑ %s", a);
    GetTextExtentPoint32W(c->dc, s, lstrlenW(s), &sz);
    int tw = sz.cx > SC(85) ? sz.cx : SC(85);
    Text(c, R - tw, R, s, cTx, DT_LEFT);
    FmtRate(a, rx); wsprintfW(s, L"↓ %s", a);
    Text(c, R - tw - SC(95), R - tw - SC(8), s, cRx, DT_LEFT);
}

static void Curve(const int *v, int n, POINT *pt, int x0, int W, int y1, int H) {
    for (int i = 0; i < n; i++) {
        pt[i].x = x0 + (HISTN - n + i) * (W - 1) / (HISTN - 1);
        pt[i].y = y1 - 1 - v[i] * (H - 2) / 1000;
    }
}

void PaintGraph(PCTX *c, const int *fill, const int *l2, int n, COLORREF c2, int l2Top, const WCHAR *topLabel) {
    int y0 = RowY(c) + SC(2), H = ROWH * GRAPH_ROWS - SC(4), y1 = y0 + H;
    int x0 = c->L, W = c->R - c->L;
    RECT r = {x0, y0, c->R, y1};
    HBRUSH bg = CreateSolidBrush(C_GRAPH_BG); FillRect(c->dc, &r, bg); DeleteObject(bg);
    HGDIOBJ oldPen = SelectObject(c->dc, GetStockObject(DC_PEN)), oldBr = SelectObject(c->dc, c->bar);
    SetDCPenColor(c->dc, C_GRID);
    for (int q = 1; q < 4; q++) { int y = y1 - 1 - q * (H - 2) / 4; MoveToEx(c->dc, x0, y, 0); LineTo(c->dc, c->R, y); }
    static POINT pt[HISTN + 2];
    if (n > 1) {
        Curve(fill, n, pt, x0, W, y1, H);
        pt[n].x = pt[n - 1].x; pt[n].y = y1;
        pt[n + 1].x = pt[0].x; pt[n + 1].y = y1;
        SelectObject(c->dc, GetStockObject(NULL_PEN));
        Polygon(c->dc, pt, n + 2);
        SelectObject(c->dc, GetStockObject(DC_PEN));
        for (int pass = 0; pass < 2; pass++) {
            if (pass == (l2Top ? 0 : 1)) { Curve(fill, n, pt, x0, W, y1, H); SetDCPenColor(c->dc, c->cMain); }
            else if (l2) { Curve(l2, n, pt, x0, W, y1, H); SetDCPenColor(c->dc, c2); }
            else continue;
            Polyline(c->dc, pt, n);
        }
    }
    SelectObject(c->dc, oldPen); SelectObject(c->dc, oldBr);
    RECT t = {x0 + SC(3), y0, c->R - SC(3), y1};
    SetTextColor(c->dc, c->cDim);
    DrawTextW(c->dc, topLabel, -1, &t, DT_SINGLELINE | DT_TOP | DT_RIGHT);
    DrawTextW(c->dc, T(S_5MIN), -1, &t, DT_SINGLELINE | DT_BOTTOM | DT_LEFT);
    c->row += GRAPH_ROWS;
}

COLORREF ListRow(PCTX *c, int x0, int nameR, const WCHAR *name, LONGLONG v, LONGLONG full, int inset, int live) {
    int W = c->R - x0;
    int bw = (int)(v * W / (full ? full : 1)); if (bw < 1) bw = 1; if (bw > W) bw = W;
    Bar(c, x0, bw, inset, live ? c->bar : c->ghost);
    COLORREF ct = live ? c->cText : C_GHOST;
    Text(c, x0 + SC(4), nameR, name, ct, DT_LEFT);
    return ct;
}
