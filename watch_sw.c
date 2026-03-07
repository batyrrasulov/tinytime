#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    void *map;
    volatile uint32_t *sw;
    uint32_t last;

    if (fd < 0) {
        perror("open /dev/mem");
        return 1;
    }

    map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0xff204000);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    sw = (volatile uint32_t *)map;
    last = sw[0];
    printf("Watching SW register at 0xff204000\n");
    printf("Initial SW=0x%08x\n", last);
    fflush(stdout);

    while (1) {
        uint32_t now = sw[0];
        if (now != last) {
            printf("SW changed: 0x%08x -> 0x%08x\n", last, now);
            fflush(stdout);
            last = now;
        }
        usleep(20000);
    }
}