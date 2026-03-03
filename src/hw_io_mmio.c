/*
 * Memory-mapped I/O backend for TinyTime.
 */

#include "hw_io.h"
#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define DT_ROOT "/proc/device-tree"
#define IOMEM_PATH "/proc/iomem"

/*
 * contains_token_ci
 * Checks whether a string contains a token (case-insensitive).
 */
static int contains_token_ci(const char *haystack, const char *needle)
{
    size_t i = 0;
    size_t j = 0;
    size_t hay_len = 0;
    size_t needle_len = 0;

    if (!haystack || !needle) {
        return 0;
    }

    hay_len = strlen(haystack);
    needle_len = strlen(needle);
    if (needle_len == 0 || hay_len < needle_len) {
        return needle_len == 0 ? 1 : 0;
    }

    for (i = 0; i + needle_len <= hay_len; i++) {
        for (j = 0; j < needle_len; j++) {
            char a = (char)tolower((unsigned char)haystack[i + j]);
            char b = (char)tolower((unsigned char)needle[j]);
            if (a != b) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }

    return 0;
}

/*
 * read_file_bytes
 * Reads up to max_len bytes from a file.
 */
static int read_file_bytes(const char *path, uint8_t *buf, size_t max_len)
{
    int fd = -1;
    ssize_t rc = 0;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    rc = read(fd, buf, max_len);
    close(fd);

    if (rc < 0) {
        return -1;
    }

    return (int)rc;
}

/*
 * read_be32
 * Decodes a big-endian 32-bit integer.
 */
static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) | ((uint32_t)p[2] << 8u) | (uint32_t)p[3];
}

/*
 * read_be64
 * Decodes a big-endian 64-bit integer.
 */
static uint64_t read_be64(const uint8_t *p)
{
    uint64_t hi = read_be32(p);
    uint64_t lo = read_be32(p + 4);
    return (hi << 32u) | lo;
}

/*
 * parse_reg_property
 * Parses a device-tree reg property into base/span.
 */
static int parse_reg_property(const char *reg_path, uint64_t *base, uint64_t *span)
{
    uint8_t buf[32];
    int len = read_file_bytes(reg_path, buf, sizeof(buf));

    if (len < 8) {
        return -1;
    }

    if (len >= 16) {
        *base = read_be64(buf);
        *span = read_be64(buf + 8);
        return 0;
    }

    *base = read_be32(buf);
    *span = read_be32(buf + 4);
    return 0;
}

/*
 * set_addr
 * Records an address range if it has not already been set.
 */
static void set_addr(HwAddr *addr, uint64_t base, uint64_t span, const char *source)
{
    if (!addr || addr->found) {
        return;
    }

    addr->base = base;
    addr->span = (span == 0) ? DEFAULT_MMIO_SPAN : span;
    addr->found = 1;
    snprintf(addr->source, sizeof(addr->source), "%s", source);
}

/*
 * inspect_compatible
 * Examines a device-tree node for HEX format hints.
 */
static void inspect_compatible(const char *node_path, HwAddrs *addrs)
{
    char path[512];
    uint8_t buf[256];
    int len = 0;

    if (!addrs || addrs->hex_format != HEX_FORMAT_UNKNOWN) {
        return;
    }

    snprintf(path, sizeof(path), "%s/compatible", node_path);
    len = read_file_bytes(path, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return;
    }

    buf[len] = '\0';
    if (contains_token_ci((const char *)buf, "seven") || contains_token_ci((const char *)buf, "7seg")) {
        addrs->hex_format = HEX_FORMAT_7SEG;
    }
}

/*
 * scan_dt_dir
 * Recursively scans the device tree for I/O devices.
 */
static void scan_dt_dir(const char *path, HwAddrs *addrs)
{
    DIR *dir = NULL;
    struct dirent *ent = NULL;

    dir = opendir(path);
    if (!dir) {
        return;
    }

    while ((ent = readdir(dir)) != NULL) {
        char child[512];
        struct stat st;
        const char *name = ent->d_name;
        uint64_t base = 0;
        uint64_t span = 0;

        if (name[0] == '.') {
            continue;
        }

        snprintf(child, sizeof(child), "%s/%s", path, name);
        if (stat(child, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!addrs->hex.found && (contains_token_ci(name, "hex") || contains_token_ci(name, "seven") || contains_token_ci(name, "7seg"))) {
                char reg_path[512];
                snprintf(reg_path, sizeof(reg_path), "%s/reg", child);
                if (parse_reg_property(reg_path, &base, &span) == 0) {
                    set_addr(&addrs->hex, base, span, "device-tree");
                    inspect_compatible(child, addrs);
                }
            }

            if (!addrs->leds.found && contains_token_ci(name, "led")) {
                char reg_path[512];
                snprintf(reg_path, sizeof(reg_path), "%s/reg", child);
                if (parse_reg_property(reg_path, &base, &span) == 0) {
                    set_addr(&addrs->leds, base, span, "device-tree");
                }
            }

            if (!addrs->switches.found && contains_token_ci(name, "switch")) {
                char reg_path[512];
                snprintf(reg_path, sizeof(reg_path), "%s/reg", child);
                if (parse_reg_property(reg_path, &base, &span) == 0) {
                    set_addr(&addrs->switches, base, span, "device-tree");
                }
            }

            if (!addrs->keys.found && (contains_token_ci(name, "key") || contains_token_ci(name, "button"))) {
                char reg_path[512];
                snprintf(reg_path, sizeof(reg_path), "%s/reg", child);
                if (parse_reg_property(reg_path, &base, &span) == 0) {
                    set_addr(&addrs->keys, base, span, "device-tree");
                }
            }

            scan_dt_dir(child, addrs);
        }
    }

    closedir(dir);
}

/*
 * scan_iomem
 * Scans /proc/iomem for I/O device regions.
 */
static void scan_iomem(HwAddrs *addrs)
{
    FILE *fp = NULL;
    char line[512];

    fp = fopen(IOMEM_PATH, "r");
    if (!fp) {
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long long start = 0;
        unsigned long long end = 0;
        char name[256];

        if (sscanf(line, "%llx-%llx : %255[^\n]", &start, &end, name) != 3) {
            continue;
        }

        if (!addrs->hex.found && (contains_token_ci(name, "hex") || contains_token_ci(name, "seven") || contains_token_ci(name, "7seg"))) {
            set_addr(&addrs->hex, (uint64_t)start, (uint64_t)(end - start + 1u), "iomem");
            if (contains_token_ci(name, "seven") || contains_token_ci(name, "7seg")) {
                addrs->hex_format = HEX_FORMAT_7SEG;
            }
        }

        if (!addrs->leds.found && contains_token_ci(name, "led")) {
            set_addr(&addrs->leds, (uint64_t)start, (uint64_t)(end - start + 1u), "iomem");
        }

        if (!addrs->switches.found && contains_token_ci(name, "switch")) {
            set_addr(&addrs->switches, (uint64_t)start, (uint64_t)(end - start + 1u), "iomem");
        }

        if (!addrs->keys.found && (contains_token_ci(name, "key") || contains_token_ci(name, "button"))) {
            set_addr(&addrs->keys, (uint64_t)start, (uint64_t)(end - start + 1u), "iomem");
        }
    }

    fclose(fp);
}

/*
 * hw_discover_addresses
 * Populates I/O base addresses from device tree and iomem.
 */
int hw_discover_addresses(HwAddrs *out)
{
    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->hex_format = HEX_FORMAT_UNKNOWN;

    fprintf(stderr, "Address discovery: scanning %s...\n", DT_ROOT);
    scan_dt_dir(DT_ROOT, out);

    fprintf(stderr, "Address discovery: scanning %s...\n", IOMEM_PATH);
    scan_iomem(out);

    if (!out->hex.found || !out->leds.found || !out->switches.found || !out->keys.found) {
        return -1;
    }

    return 0;
}

/*
 * hw_print_addrs
 * Prints discovered addresses and formats.
 */
void hw_print_addrs(const HwAddrs *addrs)
{
    if (!addrs) {
        return;
    }

    if (addrs->hex.found) {
        printf("HEX:   0x%08llx span 0x%llx (%s)\n", (unsigned long long)addrs->hex.base,
               (unsigned long long)addrs->hex.span, addrs->hex.source);
    } else {
        printf("HEX:   NOT FOUND\n");
    }

    if (addrs->leds.found) {
        printf("LED:   0x%08llx span 0x%llx (%s)\n", (unsigned long long)addrs->leds.base,
               (unsigned long long)addrs->leds.span, addrs->leds.source);
    } else {
        printf("LED:   NOT FOUND\n");
    }

    if (addrs->switches.found) {
        printf("SW:    0x%08llx span 0x%llx (%s)\n", (unsigned long long)addrs->switches.base,
               (unsigned long long)addrs->switches.span, addrs->switches.source);
    } else {
        printf("SW:    NOT FOUND\n");
    }

    if (addrs->keys.found) {
        printf("KEY:   0x%08llx span 0x%llx (%s)\n", (unsigned long long)addrs->keys.base,
               (unsigned long long)addrs->keys.span, addrs->keys.source);
    } else {
        printf("KEY:   NOT FOUND\n");
    }

    if (addrs->hex_format == HEX_FORMAT_7SEG) {
        printf("HEX format: 7-seg\n");
    } else if (addrs->hex_format == HEX_FORMAT_PACKED_BCD) {
        printf("HEX format: packed BCD\n");
    } else {
        printf("HEX format: unknown (use --probe-hex to confirm)\n");
    }
}

/*
 * map_region
 * Maps a physical address range into user space.
 */
static int map_region(int fd, uint64_t base, uint64_t span, void **map, size_t *map_size, size_t *offset)
{
    long page_size = sysconf(_SC_PAGESIZE);
    uint64_t page_mask = (uint64_t)page_size - 1u;
    uint64_t page_base = base & ~page_mask;
    uint64_t page_off = base - page_base;
    uint64_t size = span + page_off;

    if (page_size <= 0) {
        return -1;
    }

    size = (size + page_mask) & ~page_mask;

    *map = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)page_base);
    if (*map == MAP_FAILED) {
        return -1;
    }

    *map_size = (size_t)size;
    *offset = (size_t)page_off;
    return 0;
}

/*
 * hw_init
 * Initializes MMIO mappings for all I/O blocks.
 */
int hw_init(HwContext *ctx, const HwAddrs *addrs)
{
    if (!ctx || !addrs) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (ctx->mem_fd < 0) {
        perror("open /dev/mem");
        return -1;
    }

    if (map_region(ctx->mem_fd, addrs->hex.base, addrs->hex.span, &ctx->hex_map, &ctx->hex_map_size,
                   &ctx->hex_offset) != 0) {
        perror("mmap HEX");
        hw_close(ctx);
        return -1;
    }

    if (map_region(ctx->mem_fd, addrs->leds.base, addrs->leds.span, &ctx->led_map, &ctx->led_map_size,
                   &ctx->led_offset) != 0) {
        perror("mmap LED");
        hw_close(ctx);
        return -1;
    }

    if (map_region(ctx->mem_fd, addrs->switches.base, addrs->switches.span, &ctx->sw_map, &ctx->sw_map_size,
                   &ctx->sw_offset) != 0) {
        perror("mmap SW");
        hw_close(ctx);
        return -1;
    }

    if (map_region(ctx->mem_fd, addrs->keys.base, addrs->keys.span, &ctx->key_map, &ctx->key_map_size,
                   &ctx->key_offset) != 0) {
        perror("mmap KEY");
        hw_close(ctx);
        return -1;
    }

    ctx->hex_format = addrs->hex_format;
    ctx->hex_warned = 0;
    return 0;
}

/*
 * hw_close
 * Releases MMIO mappings and closes /dev/mem.
 */
void hw_close(HwContext *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->hex_map && ctx->hex_map != MAP_FAILED) {
        munmap(ctx->hex_map, ctx->hex_map_size);
    }
    if (ctx->led_map && ctx->led_map != MAP_FAILED) {
        munmap(ctx->led_map, ctx->led_map_size);
    }
    if (ctx->sw_map && ctx->sw_map != MAP_FAILED) {
        munmap(ctx->sw_map, ctx->sw_map_size);
    }
    if (ctx->key_map && ctx->key_map != MAP_FAILED) {
        munmap(ctx->key_map, ctx->key_map_size);
    }

    if (ctx->mem_fd >= 0) {
        close(ctx->mem_fd);
    }

    memset(ctx, 0, sizeof(*ctx));
}

/*
 * hw_read_switches
 * Reads raw switch bits from the mapped registers.
 */
uint32_t hw_read_switches(const HwContext *ctx)
{
    volatile uint32_t *reg = NULL;

    if (!ctx || !ctx->sw_map) {
        return 0;
    }

    reg = (volatile uint32_t *)((uint8_t *)ctx->sw_map + ctx->sw_offset);
    return *reg;
}

/*
 * hw_read_keys
 * Reads raw key bits from the mapped registers.
 */
uint32_t hw_read_keys(const HwContext *ctx)
{
    volatile uint32_t *reg = NULL;

    if (!ctx || !ctx->key_map) {
        return 0;
    }

    reg = (volatile uint32_t *)((uint8_t *)ctx->key_map + ctx->key_offset);
    return *reg;
}

/*
 * hw_write_leds
 * Writes LED bitfields to the mapped registers.
 */
void hw_write_leds(const HwContext *ctx, uint32_t bits)
{
    volatile uint32_t *reg = NULL;

    if (!ctx || !ctx->led_map) {
        return;
    }

    reg = (volatile uint32_t *)((uint8_t *)ctx->led_map + ctx->led_offset);
    *reg = bits;
}

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
 * hw_write_hex_digits
 * Writes four digits to the HEX display MMIO.
 */
int hw_write_hex_digits(const HwContext *ctx, const uint8_t digits[4])
{
    volatile uint32_t *reg = NULL;
    uint32_t value = 0;
    HexFormat format = HEX_FORMAT_UNKNOWN;

    if (!ctx || !ctx->hex_map || !digits) {
        return -1;
    }

    format = ctx->hex_format;
    if (format == HEX_FORMAT_UNKNOWN) {
        if (!ctx->hex_warned) {
            fprintf(stderr, "HEX format unknown; assuming 7-seg. Use --probe-hex to verify.\n");
            ((HwContext *)ctx)->hex_warned = 1;
        }
        format = HEX_FORMAT_7SEG;
    }

    if (format == HEX_FORMAT_7SEG) {
        value = (uint32_t)seven_seg_encode(digits[0]);
        value |= (uint32_t)seven_seg_encode(digits[1]) << 8u;
        value |= (uint32_t)seven_seg_encode(digits[2]) << 16u;
        value |= (uint32_t)seven_seg_encode(digits[3]) << 24u;
    } else if (format == HEX_FORMAT_PACKED_BCD) {
        value = ((uint32_t)(digits[0] & 0x0f)) |
                ((uint32_t)(digits[1] & 0x0f) << 4u) |
                ((uint32_t)(digits[2] & 0x0f) << 8u) |
                ((uint32_t)(digits[3] & 0x0f) << 12u);
    } else {
        return -1;
    }

    reg = (volatile uint32_t *)((uint8_t *)ctx->hex_map + ctx->hex_offset);
    *reg = value;
    return 0;
}
