#include <sys/reent.h>
#include <stdint.h>
#include "../kernel/lock.h"
#include "../kernel/screen.h"
#include "../kernel/thread.h"

static lock_t lock;
static uint8_t* const video = (uint8_t*)0xb8000;
static int x, y;

static void outb(uint16_t port, uint8_t val) {
    __asm("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor() {
    int position = y * 80 + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, position);
    outb(0x3D4, 0x0E);
    outb(0x3D5, position >> 8);
}

void screen_clear() {
    lock_enter(&lock);

    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 7;
    }

    x = y = 0;
    update_cursor();
    lock_leave(&lock);
}

_ssize_t _write_r(struct _reent *reent, int file, const void *ptr, size_t len) {
    if (file == 1) {
        uint8_t colour = 7 + thread_get_current_cpu()->num;
        const char *s = ptr;

        for (size_t i = 0; i < len; i++) {
            outb(0xe9, s[i]);
            lock_enter(&lock);

            switch (s[i]) {
                case '\n':
                    x = 0;
                    y++;
                    if (y >= 25) {
                        y = 0;
                    }
                    break;

                default: {
                    uint8_t *at = video + (y * 80 + x) * 2;
                    at[0] = s[i];
                    at[1] = colour;
                    x++;
                    if (x >= 80) {
                        x = 0;
                        y++;
                        if (y >= 25) {
                            y = 0;
                        }
                    }
                    break;
                }
            }

            lock_leave(&lock);
        }

        lock_enter(&lock);
        update_cursor();
        lock_leave(&lock);
        return len;
    } else
        return -1;
}

