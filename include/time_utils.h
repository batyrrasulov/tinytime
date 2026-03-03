#ifndef TINYTIME_TIME_UTILS_H
#define TINYTIME_TIME_UTILS_H

/*
 * Time utilities for MM:SS display.
 */

#include <stdint.h>

typedef struct {
    uint8_t min;
    uint8_t sec;
} TimeMMSS;

/*
 * time_now_ms
 * Returns the current monotonic time in milliseconds.
 */
uint64_t time_now_ms(void);

/*
 * time_increment
 * Increments a MM:SS time by one second.
 */
void time_increment(TimeMMSS *t);

/*
 * time_to_digits
 * Converts MM:SS into four decimal digits.
 */
void time_to_digits(const TimeMMSS *t, uint8_t digits[4]);

#endif
