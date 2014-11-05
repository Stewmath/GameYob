#pragma once

#include <time.h>

extern time_t rawTime;
extern time_t lastRawTime;

void clockUpdater(); 
time_t getTime();

#ifdef _3DS
time_t tickToTime(s64 tick);
#endif
