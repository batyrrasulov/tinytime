#ifndef TINYTIME_APP_H
#define TINYTIME_APP_H

/*
 * Application state machine interface.
 */

#include <stdbool.h>
#include <stdint.h>
#include "time_utils.h"

typedef enum {
    APP_MODE_CLOCK = 0,
    APP_MODE_STOPWATCH = 1
} AppMode;

typedef struct {
    AppMode mode;
    bool running;
    TimeMMSS time;
    uint32_t prev_keys;
} AppState;

/*
 * app_init
 * Initializes the application state to defaults.
 */
void app_init(AppState *state);

/*
 * app_step
 * Advances the application state based on inputs and the 1 Hz tick.
 */
void app_step(AppState *state, uint32_t sw_bits, uint32_t key_bits, bool one_second);

#endif
