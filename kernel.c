#include <stdio.h>
#include <stdlib.h>

static unsigned char * const video = (unsigned char *)0xb8000;
static int x, y;
 
static void outb(unsigned short port, unsigned char val ) {
    __asm("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor(int position) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, position);
    outb(0x3D4, 0x0E);
    outb(0x3D5, position >> 8);
}

int write(int file, char *ptr, int len) {
    if (file == 1) {
        unsigned char *write = video + (y * 80 + x) * 2;

        for (int i = 0; i < len; i++) {
            switch (ptr[i]) {
                case '\n':
                    x = 0;
                    y++;
                    write = video + (y * 80 + x) * 2;
                    break;

                default:
                    write[0] = ptr[i];
                    write[1] = 7;
                    write += 2;
                    x++;
                    if (x >= 80) {
                        x = 0;
                        y++;
                    }
                    break;
            }
        }

        update_cursor((write - video) / 2);
        return len;
    } else
        return -1;
}

void kmain(void) {
    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 7;
    }

    x = y = 0;
    update_cursor(0);
    printf("hello world\n");
}
