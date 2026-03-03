#ifndef TINYTIME_CONFIG_H
#define TINYTIME_CONFIG_H

/*
 * TinyTime configuration constants.
 */

#define APP_NAME "TinyTime"
#define APP_VERSION "2.0.1"

#define TICK_MS 1000
#define POLL_MS 10
#define DEBOUNCE_MS 50

#define SW_MASK 0x3ffu
#define KEY_MASK 0x03u

#define SW0_MASK (1u << 0)
#define SW1_MASK (1u << 1)
#define SW2_MASK (1u << 2)
#define KEY0_MASK (1u << 0)
#define KEY1_MASK (1u <<1)

/*
 * EMULATE_KEY_WITH_SW: Use SW1 as KEY0 and SW2 as KEY1.
 * Enable this when the FPGA bitstream doesn't have working KEY buttons.
 */
#define EMULATE_KEY_WITH_SW 1

#define LED_MODE_BIT (1u << 0)
#define LED_RUN_BIT (1u << 1)

#define KEY_ACTIVE_LOW 1
#define SW_ACTIVE_LOW 0

#define DEFAULT_MMIO_SPAN 0x1000u

/*
 * Optional character device paths for the Linux driver backend.
 * These can be updated when the kernel driver is installed.
 */
#define DRIVER_KEYS_DEV "/dev/tinytime_keys"
#define DRIVER_SWITCHES_DEV "/dev/tinytime_switches"
#define DRIVER_LEDS_DEV "/dev/tinytime_leds"
#define DRIVER_HEX_DEV "/dev/tinytime_hex"

/*
 * Driver HEX encoding format.
 * Use packed BCD unless the driver expects raw 7-seg bit patterns.
 */
#define DRIVER_HEX_FORMAT_PACKED_BCD 1
#define DRIVER_HEX_FORMAT_7SEG 2
#define DRIVER_HEX_FORMAT DRIVER_HEX_FORMAT_PACKED_BCD

#endif
