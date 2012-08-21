#include <reent.h>
#include "lock.h"
#include "thread.h"

typedef struct {
    lock_t lock;
    int counter;
    thread_t *owner;
    int recursion;
} rlock_t;

static rlock_t malloc_lock;

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

void rlock_enter(rlock_t *lock) {
    thread_t *t = thread_get_current();
    if (__sync_add_and_fetch(&lock->counter, 1) > 1 && lock->owner != t)
        lock_enter(&lock->lock);

    lock->owner = t;
    lock->recursion++;
}

void rlock_leave(rlock_t *lock) {
    int recur = --lock->recursion;
    if (recur == 0)
        lock->owner = NULL;

    int result = __sync_sub_and_fetch(&lock->counter, 1);
    if (result > 0 && recur == 0)
        lock_leave(&lock->lock);
}

void __malloc_lock(struct _reent *reent) {
    rlock_enter(&malloc_lock);
}

void __malloc_unlock(struct _reent *reent) {
    rlock_leave(&malloc_lock);
}
