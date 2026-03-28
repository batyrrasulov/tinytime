#ifndef TINYTIME_CONFIG_H
#define TINYTIME_CONFIG_H

/*
 * TinyTime configuration constants.
 */

#define APP_NAME "TinyTime"
#define APP_VERSION "3.0.0"

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
 * CUSTOM_FPGA: Set to 1 when building for the custom TinyTime FPGA
 * bitstream that includes the hardware BCD-to-7-seg decoder and
 * key debouncer circuits.  Set to 0 for the stock DE10 Computer image.
 * Can also be overridden at runtime with --custom-fpga.
 */
#define CUSTOM_FPGA 0

/*
 * EMULATE_KEY_WITH_SW: Use SW1 as KEY0 and SW2 as KEY1.
 * Enable this when the FPGA bitstream doesn't have working KEY buttons.
 * Automatically disabled when CUSTOM_FPGA is set.
 */
#if CUSTOM_FPGA
#define EMULATE_KEY_WITH_SW 0
#else
#define EMULATE_KEY_WITH_SW 1
#endif

#define LED_MODE_BIT (1u << 0)
#define LED_RUN_BIT (1u << 1)

#define KEY_ACTIVE_LOW 1
#define SW_ACTIVE_LOW 0

/*
 * Use terminal commands over the SSH session as the primary control path.
 * This avoids unstable FPGA switch inputs on the current board image.
 */
#define TERMINAL_CONTROL_MODE 1

#define DEFAULT_MMIO_SPAN 0x1000u

/*
 * DE10-Standard Computer (class GHRD) address map.
 * Offsets from LW bridge base 0xFF200000.
 */
#define LW_BRIDGE_BASE   0xFF200000u
#define CLASS_LEDR_BASE  0xFF200000u
#define CLASS_HEX30_BASE 0xFF200020u
#define CLASS_HEX54_BASE 0xFF200030u
#define CLASS_SW_BASE    0xFF200040u
#define CLASS_KEY_BASE   0xFF200050u
#define CLASS_PIO_SPAN   0x10u

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
