#include <nds.h>
#include <time.h>
#include "timer.h"

extern time_t rawTime;

void clockUpdater()
{
    rawTime++;
}
