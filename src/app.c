/*
 * TinyTime application state machine implementation.
 */

#include "app.h"
#include "config.h"
#include <string.h>

/*
 * app_init
 * Initializes the application state to a stopped default state.
 */
void app_init(AppState *state)
{
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->mode = APP_MODE_CLOCK;
    state->running = false;
}

/*
 * app_step
 * Updates the state machine based on inputs and elapsed whole-second ticks.
 */
void app_step(AppState *state, uint32_t sw_bits, uint32_t key_bits, uint32_t elapsed_seconds)
{
    bool key0_down = false;
    bool key1_down = false;
    bool key0_edge = false;
    bool key1_edge = false;
    uint32_t ticks = 0;

    if (!state) {
        return;
    }

    state->mode = (sw_bits & SW0_MASK) ? APP_MODE_STOPWATCH : APP_MODE_CLOCK;

    key0_down = (key_bits & KEY0_MASK) != 0u;
    key1_down = (key_bits & KEY1_MASK) != 0u;

    key0_edge = key0_down && ((state->prev_keys & KEY0_MASK) == 0u);
    key1_edge = key1_down && ((state->prev_keys & KEY1_MASK) == 0u);

    if (state->mode == APP_MODE_CLOCK) {
        state->running = true;
    } else {
        if (key0_edge) {
            state->running = !state->running;
        }
        if (key1_edge) {
            state->running = false;
            state->time.min = 0;
            state->time.sec = 0;
        }
    }

    if (state->running) {
        for (ticks = 0; ticks < elapsed_seconds; ticks++) {
            time_increment(&state->time);
        }
    }

    state->prev_keys = key_bits;
}
