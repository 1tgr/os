#include "lock.h"

void lock_enter(lock_t *l) {
    uint32_t flags;
    __asm(
        "pushf\n"
        "popl %0\n"
        "cli" : "=g" (flags));

    while (__sync_lock_test_and_set(&l->count, 1))
        ;

    l->flags = flags;
}

void lock_leave(lock_t *l) {
    uint32_t flags = l->flags;
    __sync_lock_release(&l->count);
    __asm(
        "pushl %0\n"
        "popf" : : "g" (flags));
}
