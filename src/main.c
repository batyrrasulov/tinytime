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
#include <stdio.h>
#include <string.h>
#include <time.h>

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
    uint32_t pattern = 0;

    printf("Probe: blinking LEDs and sampling SW/KEY for ~5 seconds.\n");

    while (!g_stop && (now - start) < 5000u) {
        now = time_now_ms();

        if (now >= next_led) {
            pattern = (pattern + 1u) & 0x3u;
            io->write_leds(io->ctx, pattern);
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
    Debouncer key_db;
    uint64_t last_tick = time_now_ms();
    uint64_t last_log = last_tick;
    uint32_t last_sw0 = 0;

#if EMULATE_KEY_WITH_SW
    printf("%s v%s starting.\n", APP_NAME, APP_VERSION);
    printf("\n");
    printf("=== CONTROLS ===\n");
    printf("SW0 (rightmost switch):  Mode select (DOWN=clock, UP=stopwatch)\n");
    printf("SW1 (2nd from right):    START/STOP (flip DOWN to press, UP to release)\n");
    printf("SW2 (3rd from right):    RESET (flip DOWN to reset time)\n");
    printf("\n");
    printf("(Using SW1/SW2 to emulate KEY0/KEY1 - FPGA bitstream KEY buttons don't work)\n");
    printf("WATCH FOR '[SW CHANGED]' messages when you flip switches!\n");
    printf("\n");
#else
    printf("%s v%s starting. SW0=mode (0=clock,1=stopwatch). KEY0=start/stop, KEY1=reset.\n", APP_NAME,
           APP_VERSION);
#endif

    app_init(&state);

    {
        uint32_t raw_sw = io->read_switches(io->ctx);
        uint32_t raw_key = io->read_keys(io->ctx);
        uint32_t norm_sw = normalize_bits(raw_sw, SW_MASK, SW_ACTIVE_LOW);
        uint32_t norm_key = normalize_bits(raw_key, KEY_MASK, KEY_ACTIVE_LOW);
        debounce_init(&key_db, norm_key, last_tick);
        last_sw0 = norm_sw & SW0_MASK;
        printf("Initial SW=0x%08x KEY=0x%08x\n", norm_sw, norm_key);
    }

    {
        static uint32_t last_raw_key = 0xFFFFFFFF;
        static uint32_t last_raw_sw = 0xFFFFFFFF;
        
    while (!g_stop) {
        uint64_t now = time_now_ms();
        uint32_t raw_sw = io->read_switches(io->ctx);
        uint32_t raw_key = io->read_keys(io->ctx);
        
        if (raw_sw != last_raw_sw) {
            printf("[SW CHANGED] 0x%08x -> 0x%08x  SW0=%d SW1=%d SW2=%d\n",
                   last_raw_sw, raw_sw,
                   (raw_sw & SW0_MASK) ? 1 : 0,
                   (raw_sw & SW1_MASK) ? 1 : 0,
                   (raw_sw & SW2_MASK) ? 1 : 0);
            last_raw_sw = raw_sw;
        }
        
#if EMULATE_KEY_WITH_SW
        /* Emulate KEY using SW when bitstream doesn't have working KEY buttons.
         * SW1 (bit 1) -> KEY0 (bit 0)
         * SW2 (bit 2) -> KEY1 (bit 1)
         * Active-low: SW pressed (0) becomes KEY pressed (0)
         */
        raw_key = 0x00000000;  // Start with all keys pressed (active-low)
        if (raw_sw & SW1_MASK) raw_key |= KEY0_MASK;  // SW1 up = KEY0 released
        if (raw_sw & SW2_MASK) raw_key |= KEY1_MASK;  // SW2 up = KEY1 released
#endif
        
        uint32_t norm_sw = normalize_bits(raw_sw, SW_MASK, SW_ACTIVE_LOW);
        uint32_t norm_key = normalize_bits(raw_key, KEY_MASK, KEY_ACTIVE_LOW);
        uint32_t debounced_key = debounce_update(&key_db, norm_key, now);
        uint32_t sw_bits = norm_sw;
        uint32_t key_bits = debounced_key;
        int one_second = 0;
        
        if (raw_key != last_raw_key) {
            printf("[RAW KEY CHANGE] 0x%08x -> 0x%08x (norm=0x%08x, dbounce=0x%08x)\n", 
                   last_raw_key, raw_key, norm_key, debounced_key);
            last_raw_key = raw_key;
        }

        while ((now - last_tick) >= TICK_MS) {
            last_tick += TICK_MS;
            one_second = 1;
        }

        app_step(&state, sw_bits, key_bits, one_second != 0);

        if ((sw_bits & SW0_MASK) != last_sw0) {
            last_sw0 = sw_bits & SW0_MASK;
            // printf("SW0 changed: %u\n", (last_sw0 != 0u) ? 1u : 0u);
        }

        if (one_second) {
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
                printf("MODE=%c RUN=%u TIME=%02u:%02u SW=0x%08x KEY=0x%08x\n",
                       mode_char, state.running ? 1u : 0u, state.time.min, state.time.sec,
                       raw_sw, raw_key);
                last_log = now;
            }
        }

        sleep_ms(POLL_MS);
    }
    }
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
        (void)driver_init(&driver_ctx, DRIVER_KEYS_DEV, DRIVER_SWITCHES_DEV, DRIVER_LEDS_DEV, DRIVER_HEX_DEV);
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
