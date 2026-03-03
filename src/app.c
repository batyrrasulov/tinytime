/*
 * TinyTime application state machine implementation.
 */

#include "app.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/*
 * app_init
 * Initializes the application state to default mode and running state.
 */
void app_init(AppState *state)
{
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->mode = APP_MODE_CLOCK;
    state->running = true;
}

/*
 * app_step
 * Updates the state machine based on inputs and tick events.
 */
void app_step(AppState *state, uint32_t sw_bits, uint32_t key_bits, bool one_second)
{
    bool key0_down = false;
    bool key1_down = false;
    bool key0_edge = false;
    bool key1_edge = false;

    if (!state) {
        return;
    }

    // SW0 selects clock or stopwatch mode.
    state->mode = (sw_bits & SW0_MASK) ? APP_MODE_STOPWATCH : APP_MODE_CLOCK;

    key0_down = (key_bits & KEY0_MASK) != 0u;
    key1_down = (key_bits & KEY1_MASK) != 0u;

    // Edge detection for button presses.
    key0_edge = key0_down && ((state->prev_keys & KEY0_MASK) == 0u);
    key1_edge = key1_down && ((state->prev_keys & KEY1_MASK) == 0u);

    if (state->mode == APP_MODE_CLOCK) {
        state->running = true;
        if (key0_edge || key1_edge) {
            printf("[CLOCK MODE] Buttons pressed but ignored in clock mode\n");
        }
    } else {
        if (key0_edge) {
            printf("[STOPWATCH] KEY0 pressed! Toggling run state\n");
            state->running = !state->running;
        }
        if (key1_edge) {
            printf("[STOPWATCH] KEY1 pressed! Resetting time\n");
            state->running = false;
            state->time.min = 0;
            state->time.sec = 0;
        }
    }

    if (one_second && state->running) {
        time_increment(&state->time);
    }

    state->prev_keys = key_bits;
}
