#ifndef TINYTIME_HW_IO_H
#define TINYTIME_HW_IO_H

/*
 * Memory-mapped I/O backend interface.
 */

#include <stdint.h>
#include <stddef.h>

typedef enum {
    HEX_FORMAT_UNKNOWN = 0,
    HEX_FORMAT_7SEG = 1,
    HEX_FORMAT_PACKED_BCD = 2
} HexFormat;

typedef struct {
    uint64_t base;
    uint64_t span;
    int found;
    char source[32];
} HwAddr;

typedef struct {
    HwAddr hex;
    HwAddr leds;
    HwAddr switches;
    HwAddr keys;
    HexFormat hex_format;
} HwAddrs;

typedef struct {
    int mem_fd;
    void *hex_map;
    void *led_map;
    void *sw_map;
    void *key_map;
    size_t hex_map_size;
    size_t led_map_size;
    size_t sw_map_size;
    size_t key_map_size;
    size_t hex_offset;
    size_t led_offset;
    size_t sw_offset;
    size_t key_offset;
    HexFormat hex_format;
    int hex_warned;
} HwContext;

/*
 * hw_discover_addresses
 * Locates device addresses via device tree and /proc/iomem.
 */
int hw_discover_addresses(HwAddrs *out);

/*
 * hw_print_addrs
 * Prints discovered address ranges to stdout.
 */
void hw_print_addrs(const HwAddrs *addrs);

/*
 * hw_init
 * Maps all I/O regions for MMIO access.
 */
int hw_init(HwContext *ctx, const HwAddrs *addrs);

/*
 * hw_close
 * Unmaps all I/O regions and closes /dev/mem.
 */
void hw_close(HwContext *ctx);

/*
 * hw_read_switches
 * Reads raw switch bits from MMIO.
 */
uint32_t hw_read_switches(const HwContext *ctx);

/*
 * hw_read_keys
 * Reads raw key bits from MMIO.
 */
uint32_t hw_read_keys(const HwContext *ctx);

/*
 * hw_write_leds
 * Writes LED bits to MMIO.
 */
void hw_write_leds(const HwContext *ctx, uint32_t bits);

/*
 * hw_write_hex_digits
 * Writes 4 digits to the HEX display.
 */
int hw_write_hex_digits(const HwContext *ctx, const uint8_t digits[4]);

#endif
