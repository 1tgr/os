#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/reent.h>

static unsigned int *const frameBuffer = (void *) (2 * 1024 * 1024);
extern const char _binary_font0_bin_start[];
static int x, y;

void print_char(const int x, const int y, const char c) {
    const char *data = _binary_font0_bin_start + c * 16;
    for (int cy = 0; cy < 16; cy++) {
        char bits = data[cy];
        for (int cx = 0; cx < 8; cx++) {
            frameBuffer[(y + cy) * 800 + x + cx] = (bits >> cx) & 1 ? 0xffffff : 0xff0000;
        }
    }
}

_ssize_t _write_r(struct _reent *reent, int file, const void *ptr, size_t len) {
    if (file == 1) {
        const char *s = ptr;
        for (size_t i = 0; i < len; i++) {
            switch (s[i]) {
                case '\n':
                    y += 16;
                    x = 0;
                    break;

                default:
                    print_char(x, y, s[i]);
                    x += 8;
                    if (x >= 800) {
                        y += 16;
                        x = 0;
                    }
                    break;
            }
        }

        return len;
    } else
        return -1;
}

int kmain(void) {
    volatile unsigned int *ptr1 = (void *) 0x10000000;
    volatile unsigned int *ptr2 = (void *) 0x10120000;
    ptr1[7] = 0x2cac; /* timing magic for SVGA 800x600 */
    ptr2[0] = 0x1313A4C4;
    ptr2[1] = 0x0505F657;
    ptr2[2] = 0x071F1800;
    ptr2[4] = (unsigned int) frameBuffer; /* base addr of frame buffer */
    ptr2[6] = 0x82b; /* control bits */
    /*
    char s[100] = "hello world\n";
    //sprintf(s, "hello world\n");
    return fwrite(s, 1, strlen(s), stdout);
    */
    return puts("hello world");
}
