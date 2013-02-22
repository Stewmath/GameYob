#pragma once

typedef struct
{
	//The clock time when the timer started
	int startTicks;

	//The ticks stored when the Timer was paused
	int pausedTicks;

	//The timer status
	int paused;
	int started;
} Timer;

//The various clock actions
void startTimer(Timer* timer);
void stopTimer(Timer* timer);
void pause(Timer* timer);
void unpause(Timer* timer);

//Gets the timer's time
int getTimerTicks(Timer* timer);

//Checks the status of the timer
int isStarted(Timer* timer);
int isPaused(Timer* timer);

void delay(Timer* timer);
