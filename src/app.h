#pragma once
void AppInit();                            // samplers, ETW (before windows exist)
void AppSample();                          // stage 1 of a tick: read the system
void AppTick();                            // timer: clock -> sample -> derive -> render
void AppResume();                          // after sleep: fresh baselines (no history/icon update), ETW check now
