#include "lock.h"

int exchange(int value, int *at) {   
    int result;
    __asm (
        "lock\n"
        "xchgl %0, %1" : "+m" (*at), "=g" (result) : "1" (value) : "cc");
    return result;
} 

void lock_enter(lock_t *l) {
    uint32_t flags;
    __asm(
        "pushf\n"
        "popl %0\n"
        "cli" : "=g" (flags));

    while (exchange(1, &l->count))
        ;

    l->flags = flags;
}

void lock_leave(lock_t *l) {
    uint32_t flags = l->flags;
    l->count = 0;
    __asm(
        "pushl %0\n"
        "popf" : : "g" (flags));
}
