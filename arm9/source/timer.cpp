#include <nds.h>
#include "timer.h"

#define DSCLOCKSPEED 33513.982

void startTimer(Timer* timer)
{
	cpuStartTiming(0);
	timerElapsed(0);
	//startTicks = 0;
	//ticks = 0;
}

int getTimerTicks(Timer* timer)
{
	return (cpuGetTiming() / (DSCLOCKSPEED));
}

void delay(Timer* timer)
{
}
