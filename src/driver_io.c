#include "driver_io.h"
#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * seven_seg_encode
 * Converts a decimal digit into a 7-seg bit pattern.
 */
static uint8_t seven_seg_encode(uint8_t digit)
{
    static const uint8_t table[10] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66,
        0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };

    if (digit > 9) {
        return 0x00;
    }

    return table[digit];
}

/*
 * driver_init
 * Opens the Linux character devices for keys, switches, LEDs, and HEX.
 * Returns 0 when all devices are available, or -1 on failure.
 */
int driver_init(DriverContext *ctx, const char *keys_path, const char *switches_path,
                const char *leds_path, const char *hex_path)
{
    if (!ctx) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->fd_keys = -1;
    ctx->fd_switches = -1;
    ctx->fd_leds = -1;
    ctx->fd_hex = -1;
    ctx->stub_mode = false;
    ctx->stub_counter = 0u;

    ctx->fd_keys = open(keys_path, O_RDONLY);
    ctx->fd_switches = open(switches_path, O_RDONLY);
    ctx->fd_leds = open(leds_path, O_WRONLY);
    ctx->fd_hex = open(hex_path, O_WRONLY);

    if (ctx->fd_keys < 0 || ctx->fd_switches < 0 || ctx->fd_leds < 0 || ctx->fd_hex < 0) {
        fprintf(stderr, "Driver backend not ready (%s).\n", strerror(errno));
        driver_close(ctx);
        return -1;
    }

    return 0;
}

/*
 * driver_close
 * Closes all device file descriptors.
 */
void driver_close(DriverContext *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->fd_keys >= 0) {
        close(ctx->fd_keys);
    }
    if (ctx->fd_switches >= 0) {
        close(ctx->fd_switches);
    }
    if (ctx->fd_leds >= 0) {
        close(ctx->fd_leds);
    }
    if (ctx->fd_hex >= 0) {
        close(ctx->fd_hex);
    }

    ctx->fd_keys = -1;
    ctx->fd_switches = -1;
    ctx->fd_leds = -1;
    ctx->fd_hex = -1;
}

/*
 * driver_read_switches
 * Reads a 32-bit switch value from the driver.
 */
uint32_t driver_read_switches(DriverContext *ctx)
{
    uint32_t value = 0;
    ssize_t rc = 0;

    if (!ctx || ctx->fd_switches < 0) {
        return 0;
    }

    rc = read(ctx->fd_switches, &value, sizeof(value));
    if (rc != (ssize_t)sizeof(value)) {
        return 0;
    }

    return value;
}

/*
 * driver_read_keys
 * Reads a 32-bit key value from the driver.
 */
uint32_t driver_read_keys(DriverContext *ctx)
{
    uint32_t value = 0;
    ssize_t rc = 0;

    if (!ctx || ctx->fd_keys < 0) {
        return 0;
    }

    rc = read(ctx->fd_keys, &value, sizeof(value));
    if (rc != (ssize_t)sizeof(value)) {
        return 0;
    }

    return value;
}

/*
 * driver_write_leds
 * Writes LED bitfields to the driver.
 */
void driver_write_leds(DriverContext *ctx, uint32_t bits)
{
    ssize_t rc = 0;

    if (!ctx || ctx->fd_leds < 0) {
        return;
    }

    rc = write(ctx->fd_leds, &bits, sizeof(bits));
    (void)rc;
}

/*
 * driver_write_hex_digits
 * Writes packed BCD digits to the driver.
 */
int driver_write_hex_digits(DriverContext *ctx, const uint8_t digits[4])
{
    uint32_t value = 0;
    ssize_t rc = 0;

    if (!ctx || !digits || ctx->fd_hex < 0) {
        return -1;
    }

    if (DRIVER_HEX_FORMAT == DRIVER_HEX_FORMAT_7SEG) {
        value = (uint32_t)seven_seg_encode(digits[0]);
        value |= (uint32_t)seven_seg_encode(digits[1]) << 8u;
        value |= (uint32_t)seven_seg_encode(digits[2]) << 16u;
        value |= (uint32_t)seven_seg_encode(digits[3]) << 24u;
    } else {
        value = ((uint32_t)(digits[0] & 0x0f)) |
                ((uint32_t)(digits[1] & 0x0f) << 4u) |
                ((uint32_t)(digits[2] & 0x0f) << 8u) |
                ((uint32_t)(digits[3] & 0x0f) << 12u);
    }

    rc = write(ctx->fd_hex, &value, sizeof(value));
    if (rc != (ssize_t)sizeof(value)) {
        return -1;
    }

    return 0;
}
