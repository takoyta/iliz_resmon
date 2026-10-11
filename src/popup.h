#pragma once
void ShowPopup(int id);                    // on NIN_POPUPOPEN
void HidePopup();
void PopupTick();                          // once per second while open
LRESULT CALLBACK PopProc(HWND h, UINT m, WPARAM w, LPARAM l);
