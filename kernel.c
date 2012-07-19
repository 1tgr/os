#include <stdio.h>

static unsigned char * const video = (unsigned char *)0xb8000;
static int x, y;
 
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

        return len;
    } else
        return -1;
}

void kmain(void) {
    printf("hello world\n");
}
