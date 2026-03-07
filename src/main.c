#define _POSIX_C_SOURCE 200809L

/*
 * TinyTime main application entry point.
 */

#include "app.h"
#include "config.h"
#include "driver_io.h"
#include "hw_io.h"
#include "time_utils.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    uint32_t stable;
    uint32_t last_raw;
    uint64_t last_change_ms;
} Debouncer;

typedef struct {
    void *ctx;
    uint32_t (*read_switches)(void *ctx);
    uint32_t (*read_keys)(void *ctx);
    void (*write_leds)(void *ctx, uint32_t bits);
    int (*write_hex)(void *ctx, const uint8_t digits[4]);
    void (*close)(void *ctx);
} IoBackend;

static volatile sig_atomic_t g_stop = 0;
static struct termios g_saved_termios;
static int g_termios_active = 0;

/*
 * handle_sigint
 * Handles Ctrl+C for clean shutdown.
 */
static void handle_sigint(int sig)
{
    (void)sig;
    g_stop = 1;
}

/*
 * restore_terminal_mode
 * Restores the SSH terminal to its prior settings.
 */
static void restore_terminal_mode(void)
{
    if (g_termios_active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_termios);
        g_termios_active = 0;
    }
}

/*
 * enable_terminal_mode
 * Enables noncanonical no-echo input so single-key commands can be read.
 */
static int enable_terminal_mode(void)
{
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &g_saved_termios) != 0) {
        return -1;
    }

    raw = g_saved_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return -1;
    }

    g_termios_active = 1;
    atexit(restore_terminal_mode);
    return 0;
}

/*
 * poll_terminal_command
 * Returns one pending command character from stdin, or 0 if none is ready.
 */
static int poll_terminal_command(void)
{
    fd_set readfds;
    struct timeval timeout;
    unsigned char ch = 0;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) <= 0) {
        return 0;
    }
    if (!FD_ISSET(STDIN_FILENO, &readfds)) {
        return 0;
    }
    if (read(STDIN_FILENO, &ch, 1) != 1) {
        return 0;
    }

    return (int)ch;
}

/*
 * debounce_init
 * Initializes a debouncer with the current raw state.
 */
static void debounce_init(Debouncer *d, uint32_t raw, uint64_t now_ms)
{
    d->stable = raw;
    d->last_raw = raw;
    d->last_change_ms = now_ms;
}

/*
 * debounce_update
 * Updates a debouncer and returns the stable value.
 */
static uint32_t debounce_update(Debouncer *d, uint32_t raw, uint64_t now_ms)
{
    if (raw != d->last_raw) {
        d->last_raw = raw;
        d->last_change_ms = now_ms;
    }

    if ((now_ms - d->last_change_ms) >= DEBOUNCE_MS) {
        d->stable = d->last_raw;
    }

    return d->stable;
}

/*
 * debounce_update_with_window
 * Updates a debouncer using a caller-supplied debounce interval.
 */
static uint32_t debounce_update_with_window(Debouncer *d, uint32_t raw, uint64_t now_ms,
                                            uint64_t debounce_ms)
{
    if (raw != d->last_raw) {
        d->last_raw = raw;
        d->last_change_ms = now_ms;
    }

    if ((now_ms - d->last_change_ms) >= debounce_ms) {
        d->stable = d->last_raw;
    }

    return d->stable;
}

/*
 * normalize_bits
 * Applies masking and active-low conversion to raw bits.
 */
static uint32_t normalize_bits(uint32_t raw, uint32_t mask, int active_low)
{
    uint32_t bits = raw & mask;

    if (active_low) {
        bits = (~bits) & mask;
    }

    return bits;
}

/*
 * print_sw_bits
 * Prints the low 10 switch bits from MSB to LSB.
 */
/*
 * emulate_key_raw_from_switches
 * Synthesizes the active-low KEY register value from SW1/SW2.
 */
static uint32_t emulate_key_raw_from_switches(uint32_t raw_sw)
{
    uint32_t raw_key = 0u;

    if (raw_sw & SW1_MASK) {
        raw_key |= KEY0_MASK;
    }
    if (raw_sw & SW2_MASK) {
        raw_key |= KEY1_MASK;
    }

    return raw_key;
}

/*
 * sleep_ms
 * Sleeps for the specified number of milliseconds.
 */
static void sleep_ms(unsigned int ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000u;
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
}

/*
 * print_usage
 * Prints the command line usage string.
 */
static void print_usage(const char *prog)
{
    printf("Usage: %s [--print-addrs|--probe|--probe-hex|--use-driver]\n", prog);
}

/*
 * mmio_* wrappers
 * Adapt MMIO functions to the IoBackend signature.
 */
static uint32_t mmio_read_switches(void *ctx)
{
    return hw_read_switches((const HwContext *)ctx);
}

static uint32_t mmio_read_keys(void *ctx)
{
    return hw_read_keys((const HwContext *)ctx);
}

static void mmio_write_leds(void *ctx, uint32_t bits)
{
    hw_write_leds((const HwContext *)ctx, bits);
}

static int mmio_write_hex(void *ctx, const uint8_t digits[4])
{
    return hw_write_hex_digits((const HwContext *)ctx, digits);
}

static void mmio_close(void *ctx)
{
    hw_close((HwContext *)ctx);
}

/*
 * driver_* wrappers
 * Adapt driver functions to the IoBackend signature.
 */
static uint32_t driver_read_switches_wrap(void *ctx)
{
    return driver_read_switches((DriverContext *)ctx);
}

static uint32_t driver_read_keys_wrap(void *ctx)
{
    return driver_read_keys((DriverContext *)ctx);
}

static void driver_write_leds_wrap(void *ctx, uint32_t bits)
{
    driver_write_leds((DriverContext *)ctx, bits);
}

static int driver_write_hex_wrap(void *ctx, const uint8_t digits[4])
{
    return driver_write_hex_digits((DriverContext *)ctx, digits);
}

static void driver_close_wrap(void *ctx)
{
    driver_close((DriverContext *)ctx);
}

/*
 * run_probe
 * Blinks LEDs and samples inputs for validation.
 */
static void run_probe(const IoBackend *io)
{
    uint64_t start = time_now_ms();
    uint64_t now = start;
    uint64_t next_led = start;
    uint64_t next_sw = start;
    uint32_t last_key = io->read_keys(io->ctx);
    uint32_t led_bit = 0;
    uint32_t pattern = 1u;

    printf("Probe: blinking LEDs and sampling SW/KEY for ~5 seconds.\n");

    while (!g_stop && (now - start) < 5000u) {
        now = time_now_ms();

        if (now >= next_led) {
            pattern = 1u << led_bit;
            io->write_leds(io->ctx, pattern);
            led_bit = (led_bit + 1u) % 10u;
            next_led += 1000u;
        }

        if (now >= next_sw) {
            uint32_t sw = io->read_switches(io->ctx);
            printf("SW=0x%08x\n", sw);
            next_sw += 1000u;
        }

        {
            uint32_t key = io->read_keys(io->ctx);
            if (key != last_key) {
                printf("KEY=0x%08x\n", key);
                last_key = key;
            }
        }

        sleep_ms(50u);
    }

    io->write_leds(io->ctx, 0);
}

/*
 * run_probe_hex
 * Cycles HEX digits to validate display wiring.
 */
static void run_probe_hex(const IoBackend *io)
{
    uint8_t digits[4] = {0, 0, 0, 0};
    int digit = 0;
    int value = 0;

    printf("Probe HEX: cycling each digit 0-9 (1 second each).\n");

    for (digit = 0; digit < 4 && !g_stop; digit++) {
        for (value = 0; value < 10 && !g_stop; value++) {
            digits[0] = 0;
            digits[1] = 0;
            digits[2] = 0;
            digits[3] = 0;
            digits[digit] = (uint8_t)value;

            io->write_hex(io->ctx, digits);
            printf("HEX digit %d -> %d\n", digit, value);
            sleep_ms(1000u);
        }
    }
}

/*
 * run_app
 * Main application loop.
 */
static void run_app(const IoBackend *io)
{
    AppState state;
    uint64_t last_tick = time_now_ms();
    uint64_t last_log = last_tick;
    uint32_t terminal_sw_bits = SW0_MASK;
    uint32_t pending_key_bits = 0u;
    int terminal_mode_ready = 0;

    printf("%s v%s starting.\n", APP_NAME, APP_VERSION);
    printf("\n");
    printf("=== CONTROLS ===\n");
    printf("m = toggle clock/stopwatch mode\n");
    printf("s = start/stop stopwatch\n");
    printf("r = reset stopwatch to 00:00\n");
    printf("q = quit\n");
    printf("\n");

    app_init(&state);
    terminal_mode_ready = (enable_terminal_mode() == 0);

    {
        app_step(&state, terminal_sw_bits, 0u, 0u);
        printf("Initial MODE=Stopwatch TIME=00:00\n");
        if (!terminal_mode_ready) {
            printf("Warning: terminal raw mode unavailable; commands may require Enter.\n");
        }
    }

    while (!g_stop) {
        uint64_t now = time_now_ms();
        uint32_t elapsed_seconds = 0u;
        uint32_t key_bits = 0u;
        int cmd = poll_terminal_command();

        if (cmd == 'q' || cmd == 'Q') {
            g_stop = 1;
            continue;
        }
        if (cmd == 'm' || cmd == 'M') {
            if ((terminal_sw_bits & SW0_MASK) != 0u) {
                terminal_sw_bits &= ~SW0_MASK;
                printf("[CMD] mode -> clock\n");
            } else {
                terminal_sw_bits |= SW0_MASK;
                printf("[CMD] mode -> stopwatch\n");
            }
        } else if (cmd == 's' || cmd == 'S') {
            pending_key_bits |= KEY0_MASK;
            printf("[CMD] start/stop\n");
        } else if (cmd == 'r' || cmd == 'R') {
            pending_key_bits |= KEY1_MASK;
            printf("[CMD] reset\n");
        }
        key_bits = pending_key_bits;

        while ((now - last_tick) >= TICK_MS) {
            last_tick += TICK_MS;
            elapsed_seconds++;
        }

        app_step(&state, terminal_sw_bits, key_bits, elapsed_seconds);
        pending_key_bits = 0u;

        {
            uint8_t digits[4];
            uint32_t leds = 0;
            char mode_char = (state.mode == APP_MODE_STOPWATCH) ? 'S' : 'C';

            time_to_digits(&state.time, digits);
            io->write_hex(io->ctx, digits);

            if (state.mode == APP_MODE_STOPWATCH) {
                leds |= LED_MODE_BIT;
            }
            if (state.running) {
                leds |= LED_RUN_BIT;
            }
            io->write_leds(io->ctx, leds);

            if ((now - last_log) >= TICK_MS) {
                printf("MODE=%c RUN=%u TIME=%02u:%02u CMD_SW0=%u\n",
                       mode_char, state.running ? 1u : 0u, state.time.min, state.time.sec,
                       (terminal_sw_bits & SW0_MASK) ? 1u : 0u);
                last_log = now;
            }
        }

        sleep_ms(POLL_MS);
    }

    restore_terminal_mode();
}

int main(int argc, char **argv)
{
    HwAddrs addrs;
    HwContext ctx;
    DriverContext driver_ctx;
    IoBackend io;
    int do_print = 0;
    int do_probe = 0;
    int do_probe_hex = 0;
    int use_driver = 0;
    int i = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print-addrs") == 0) {
            do_print = 1;
        } else if (strcmp(argv[i], "--probe") == 0) {
            do_probe = 1;
        } else if (strcmp(argv[i], "--probe-hex") == 0) {
            do_probe_hex = 1;
        } else if (strcmp(argv[i], "--use-driver") == 0) {
            use_driver = 1;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    signal(SIGINT, handle_sigint);

    printf("%s v%s\n", APP_NAME, APP_VERSION);

    if (use_driver) {
        if (driver_init(&driver_ctx, DRIVER_KEYS_DEV, DRIVER_SWITCHES_DEV, DRIVER_LEDS_DEV, DRIVER_HEX_DEV) != 0) {
            fprintf(stderr, "Driver backend initialization failed. Use MMIO mode on this board.\n");
            return 1;
        }
        io.ctx = &driver_ctx;
        io.read_switches = driver_read_switches_wrap;
        io.read_keys = driver_read_keys_wrap;
        io.write_leds = driver_write_leds_wrap;
        io.write_hex = driver_write_hex_wrap;
        io.close = driver_close_wrap;

        if (do_print) {
            fprintf(stderr, "--print-addrs is not available for driver backend.\n");
            io.close(io.ctx);
            return 1;
        }
    } else {
        if (hw_discover_addresses(&addrs) != 0) {
            hw_print_addrs(&addrs);
            fprintf(stderr, "Address discovery incomplete.\n");
            return 1;
        }

        if (do_print) {
            hw_print_addrs(&addrs);
            return 0;
        }

        if (hw_init(&ctx, &addrs) != 0) {
            fprintf(stderr, "Failed to initialize MMIO.\n");
            return 1;
        }

        io.ctx = &ctx;
        io.read_switches = mmio_read_switches;
        io.read_keys = mmio_read_keys;
        io.write_leds = mmio_write_leds;
        io.write_hex = mmio_write_hex;
        io.close = mmio_close;
    }

    if (do_probe) {
        run_probe(&io);
        io.close(io.ctx);
        return 0;
    }

    if (do_probe_hex) {
        run_probe_hex(&io);
        io.close(io.ctx);
        return 0;
    }

    run_app(&io);
    io.close(io.ctx);
    return 0;
}
