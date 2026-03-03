#define _POSIX_C_SOURCE 200809L

/*
 * Time helper functions for TinyTime.
 */

#include "time_utils.h"
#include <time.h>

/*
 * time_now_ms
 * Returns the current monotonic time in milliseconds.
 */
uint64_t time_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
}

/*
 * time_increment
 * Advances a MM:SS timestamp by one second.
 */
void time_increment(TimeMMSS *t)
{
    if (!t) {
        return;
    }

    t->sec++;
    if (t->sec >= 60) {
        t->sec = 0;
        t->min++;
        if (t->min >= 60) {
            t->min = 0;
        }
    }
}

/*
 * time_to_digits
 * Converts a MM:SS timestamp into four digits.
 */
void time_to_digits(const TimeMMSS *t, uint8_t digits[4])
{
    uint8_t min = 0;
    uint8_t sec = 0;

    if (t) {
        min = t->min;
        sec = t->sec;
    }

    digits[0] = (uint8_t)(min / 10u);
    digits[1] = (uint8_t)(min % 10u);
    digits[2] = (uint8_t)(sec / 10u);
    digits[3] = (uint8_t)(sec % 10u);
}
