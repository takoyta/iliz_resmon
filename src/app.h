#pragma once
void AppInit();                            // samplers, ETW (before windows exist)
void AppSample();                          // stage 1 of a tick: read the system
void AppTick();                            // timer: clock -> sample -> derive -> render
