#pragma once
#include "common.h"
enum { ETW_OFF, ETW_ON, ETW_DENIED, ETW_DIED };          // etwState
extern int etwOn, etwState, etwTicks;      // etwOn == (etwState == ETW_ON); etwTicks: ticks since session start
extern ULONG etwErr, etwLost;              // last start error; events lost (from session query)
int EtwNote();                             // !etwOn: S_NOTE_ADMIN (not elevated) or S_NOTE_BLOCKED; 0 while tracing
void EtwInit();
void EtwStart();                           // at startup; EtwTick retries every 15 s while down, health check every 10 s
void EtwStop();                            // on exit
void EtwTick();                            // every timer tick: disk / net traffic into ptDisk / ptNet
