#include <sys/reent.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "inbox.h"
#include "interrupt.h"
#include "lock.h"
#include "thread.h"

static thread_t *runnable_first, *runnable_last, *waiting_first, *waiting_last, *sleeping_first, *sleeping_last;
static lock_t lock;
static unsigned uptime;
cpu_t **cpus;
static int cpu_count;

const unsigned quantum = 10;

#define LIST_ADD(list, obj, member) \
    { \
        __typeof__ (obj) __obj = obj; \
        __obj->member.prev = list##_last; \
        __obj->member.next = NULL; \
        if (list##_last != NULL) \
            list##_last->member.next = __obj; \
        if (list##_first == NULL) \
            list##_first = __obj; \
        list##_last = __obj; \
    }

#define LIST_REMOVE(list, obj, member) \
    { \
        __typeof__ (obj) __obj = obj; \
        *(__obj->member.prev == NULL ? &list##_first : &__obj->member.prev->member.next) = __obj->member.next; \
        *(__obj->member.next == NULL ? &list##_last  : &__obj->member.next->member.prev) = __obj->member.prev; \
    }

static void unlock_and_switch(int is_exit) {
    cpu_t *cpu = thread_get_current_cpu();
    thread_t *old_current = cpu->current;
    thread_t *volatile new_current = NULL;

    for (thread_t *t = waiting_first; new_current == NULL && t != NULL; t = t->u.waiting.next) {
        if (t->u.waiting.inbox->data->count > 0) {
            new_current = t;
            LIST_REMOVE(waiting, new_current, u.waiting);
        }
    }

    for (thread_t *t = sleeping_first; new_current == NULL && t != NULL; t = t->u.sleeping.next) {
        if (t->u.sleeping.until <= uptime) {
            new_current = t;
            LIST_REMOVE(sleeping, new_current, u.sleeping);
        }
    }

    if (new_current == NULL) {
        new_current = runnable_first;
        LIST_REMOVE(runnable, new_current, u.runnable);
    }

    if (new_current == NULL)
        new_current = &cpu->idle;

    new_current->state = thread_current;

    if (is_exit) {
        old_current->state = thread_exited;
    } else if (old_current->state == thread_current && old_current != &cpu->idle) {
        old_current->state = thread_runnable;
        LIST_ADD(runnable, old_current, u.runnable);
    }

    if (setjmp(old_current->buf) == 0) {
        cpu->current = new_current;

        if (is_exit)
            obj_release(&old_current->obj);

        lock_leave(&lock);
        longjmp(new_current->buf, 1);
    }
}

void thread_yield() {
    lock_enter(&lock);
    unlock_and_switch(0);
}

void thread_exit() {
    lock_enter(&lock);
    unlock_and_switch(1);
}

thread_t *thread_start(void (*entry)(void*), void *arg) {
    int stack_size = 65536;
    char *stack_start = malloc(stack_size);
    void **stack_end = (void**) (stack_start + stack_size);
    stack_end--;
    *stack_end = arg;
    stack_end--;
    *stack_end = thread_exit;

#if defined (__i386__)
    jmp_buf buf = { {
        .eax = 0, .ebx = 0, .ecx = 0, .edx = 0, .esi = 0, .edi = 0, .ebp = 0,
        .esp = (uintptr_t) stack_end,
        .eip = (uintptr_t) entry,
    } };
#else
    jmp_buf buf = { 0 };
#endif
    
    thread_t *t = obj_alloc(sizeof(*t));
    t->buf[0] = buf[0];
    t->state = thread_runnable;
    _REENT_INIT_PTR(&t->reent);

    lock_enter(&lock);
    LIST_ADD(runnable, t, u.runnable);
    unlock_and_switch(0);
    return t;
}

void thread_wait(struct inbox_t *inbox) {
    lock_enter(&lock);

    {
        thread_t *t = thread_get_current();
        t->state = thread_waiting;
        t->u.waiting.inbox = obj_retain(&inbox->obj);
        LIST_ADD(waiting, t, u.waiting);
    }

    unlock_and_switch(0);
}

void thread_sleep(unsigned milliseconds) {
    lock_enter(&lock);

    {
        thread_t *t = thread_get_current();
        t->state = thread_sleeping;
        t->u.sleeping.until = uptime + milliseconds;
        LIST_ADD(sleeping, t, u.sleeping);
    }

    unlock_and_switch(0);
}

thread_t *thread_get_current() {
#if defined(__i386__)
    thread_t *current;
    __asm("movl %%fs:(4), %0" : "=a" (current));
    return current;
#else
    return thread_get_current_cpu()->current;
#endif
}

cpu_t *thread_get_current_cpu() {
#if defined(__i386__)
    cpu_t *self;
    __asm("movl %%fs:(0), %0" : "=a" (self));
    return self;
#else
    return cpus[0];
#endif
}

unsigned thread_get_quantum() {
    return quantum;
}

unsigned thread_get_uptime() {
    return uptime;
}

static void timer_thread(void *arg) {
    pool_t *pool = obj_alloc_pool();
    while (1) {
        inbox_read(arg);
        lock_enter(&lock);
        uptime += quantum;
        lock_leave(&lock);
        thread_yield();
        // obj_drain_pool(pool);
    }

    obj_release(&pool->obj);
}

struct _reent *__getreent() {
    return &thread_get_current()->reent;
}

void thread_init() {
    inbox_t *inbox = inbox_alloc();
    interrupt_subscribe(0, inbox, obj_alloc(sizeof(obj_t)));
    thread_start(timer_thread, inbox);
}

void thread_set_cpu_count(unsigned count) {
    lock_enter(&lock);

    {
        if (cpus == NULL)
            cpus = malloc(count * sizeof(*cpus));
        else
            cpus = realloc(cpus, count * sizeof(*cpus));;

        for (unsigned i = cpu_count; i < cpu_count + count; i++) {
            cpu_t *cpu = malloc(sizeof(*cpu));
            cpu->self = cpu;
            cpu->current = &cpu->idle;
            cpu->num = i;
            _REENT_INIT_PTR(&cpu->idle.reent);
            cpu->idle.state = thread_current;
            cpus[i] = cpu;
        }

        cpu_count = count;
    }

    lock_leave(&lock);
}
