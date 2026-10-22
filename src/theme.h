// Palette.
#pragma once
#define C_CPU       RGB(150, 230, 60)
#define C_CPU_DARK  RGB(70, 125, 35)
#define C_CPU_PEAK  RGB(215, 255, 160)     // busiest-core line on the CPU graph
#define C_DSK       RGB(70, 215, 200)
#define C_DSK_DARK  RGB(30, 110, 105)
#define C_RX        RGB(100, 190, 255)
#define C_RX_DARK   RGB(40, 80, 150)
#define C_MEM       RGB(190, 120, 255)
#define C_MEM_DARK  RGB(90, 50, 140)
#define C_GPU       RGB(255, 110, 140)
#define C_GPU_DARK  RGB(140, 45, 70)
#define C_VRAM      RGB(80, 235, 255)      // video memory line over the GPU load (contrasts with the pink columns)

// Two-way traffic, the same everywhere (tables, bars, graph, icon): in = disk read / network received,
// out = disk write / network sent. _TOT: "today" column; _BAR: process bars (dark: text on them stays readable).
#define C_IN        RGB(110, 225, 110)
#define C_IN_TOT    RGB(80, 170, 80)
#define C_IN_BAR    RGB(40, 105, 40)
#define C_OUT       RGB(255, 150, 40)
#define C_OUT_TOT   RGB(180, 110, 40)
#define C_OUT_BAR   RGB(135, 75, 20)

#define C_ICON_BG     RGB(20, 30, 28)
#define C_ICON_BG_NET RGB(18, 24, 32)
#define C_POP_BG      RGB(18, 26, 24)
#define C_GRAPH_BG    RGB(26, 36, 34)
#define C_GRID        RGB(40, 54, 50)
#define C_TEXT        RGB(220, 255, 230)
#define C_DIM         RGB(120, 160, 140)
#define C_GHOST       RGB(110, 120, 115)   // dropped-out rows: text
#define C_GHOST_BAR   RGB(48, 56, 54)      // dropped-out rows: bar
#define C_GHOST_BAR2  RGB(72, 64, 54)      // dropped-out rows: out part (warm grey)
