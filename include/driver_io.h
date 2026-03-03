#ifndef TINYTIME_DRIVER_IO_H
#define TINYTIME_DRIVER_IO_H

/*
 * Linux device driver I/O backend for TinyTime.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int fd_keys;
    int fd_switches;
    int fd_leds;
    int fd_hex;
    bool stub_mode;
    uint32_t stub_counter;
} DriverContext;

int driver_init(DriverContext *ctx, const char *keys_path, const char *switches_path,
                const char *leds_path, const char *hex_path);

void driver_close(DriverContext *ctx);

uint32_t driver_read_switches(DriverContext *ctx);

uint32_t driver_read_keys(DriverContext *ctx);

void driver_write_leds(DriverContext *ctx, uint32_t bits);

int driver_write_hex_digits(DriverContext *ctx, const uint8_t digits[4]);

#endif
