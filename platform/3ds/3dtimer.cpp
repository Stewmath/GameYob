#include <time.h>
#include <3ds.h>

#define TICKS_PER_MSEC 268123.480


time_t tickToTime(s64 ticks) {
    struct tm baseTimeTm;
    baseTimeTm.tm_sec = 0;
    baseTimeTm.tm_min = 0;
    baseTimeTm.tm_hour = 0;
    baseTimeTm.tm_mday = 1;
    baseTimeTm.tm_mon = 0;
    baseTimeTm.tm_year = 0; // 1900
    baseTimeTm.tm_wday = 0;
    baseTimeTm.tm_yday = 0;
    baseTimeTm.tm_isdst = 0;

    time_t baseTime = mktime(&baseTimeTm);

    return (time_t)(ticks / TICKS_PER_MSEC) - baseTime;
}

time_t getTime() {
    struct tm baseTimeTm;
    baseTimeTm.tm_sec = 0;
    baseTimeTm.tm_min = 0;
    baseTimeTm.tm_hour = 0;
    baseTimeTm.tm_mday = 1;
    baseTimeTm.tm_mon = 0;
    baseTimeTm.tm_year = 0; // 1900
    baseTimeTm.tm_wday = 0;
    baseTimeTm.tm_yday = 0;
    baseTimeTm.tm_isdst = 0;

    time_t baseTime = mktime(&baseTimeTm);
    return osGetTime()/1000-baseTime;
}
