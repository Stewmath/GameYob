#include "timer.h"

#ifdef C_IO_FUNCTIONS

time_t getTime() {
    time_t t;
    time(&t);
    return t;
}

#endif
