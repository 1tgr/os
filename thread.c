#include <sys/reent.h>
#include <stdlib.h>
#include <string.h>
#include "inbox.h"
#include "interrupt.h"
#include "lock.h"
#include "thread.h"
#include "types.h"

static thread_t idle, *current = &idle, *running_first = &idle, *running_last = &idle, *waiting_first, *waiting_last, *sleeping_first, *sleeping_last;
static lock_t lock;
static unsigned uptime;
const unsigned quantum = 10;

#define LIST_ADD(list, obj, member) \
    { \
        (obj)->member.prev = list##_last; \
        (obj)->member.next = NULL; \
        if (list##_last != NULL) \
            list##_last->member.next = t; \
        if (list##_first == NULL) \
            list##_first = t; \
        list##_last = t; \
    }

#define LIST_REMOVE(list, obj, member) \
    { \
        *((obj)->member.prev == NULL ? &list##_first : &(obj)->member.prev->member.next) = (obj)->member.next; \
        *((obj)->member.next == NULL ? &list##_last  : &(obj)->member.next->member.prev) = (obj)->member.prev; \
    }

static void unlock_and_switch_to(thread_t *t) {
    if (setjmp(current->buf) == 0) {
        current = t;
        lock_leave(&lock);
        longjmp(t->buf, 1);
    }
}

static thread_t *find_runnable() {
    for (thread_t *t = waiting_first; t != NULL; t = t->u.waiting.next) {
        if (t->u.waiting.inbox->data->count > 0)
            return t;
    }

    for (thread_t *t = sleeping_first; t != NULL; t = t->u.sleeping.next) {
        if (t->u.sleeping.until <= uptime) {
            return t;
        }
    }

    if (current->u.running.next != NULL)
       return current->u.running.next;

    return running_first;
}

static void ensure_running(thread_t *t) {
    switch (t->state) {
        case thread_running:
            return;

        case thread_waiting:
            LIST_REMOVE(waiting, t, u.waiting);
            break;

        case thread_sleeping:
            LIST_REMOVE(sleeping, t, u.sleeping);
            break;
    }

    t->state = thread_running;
    LIST_ADD(running, t, u.running);
}

static thread_t *ensure_not_running(thread_t *t) {
    thread_t *new_current = t->u.running.next == NULL ? running_first : t->u.running.next;
    LIST_REMOVE(running, t, u.running);
    return new_current;
}

void thread_yield() {
    thread_t *t;
    lock_enter(&lock);

    {
        t = find_runnable();
        ensure_running(t);
    }

    unlock_and_switch_to(t);
}

void thread_exit() {
    thread_t *old_current, *new_current;
    lock_enter(&lock);

    {
        old_current = current;
        LIST_REMOVE(running, current, u.running);
        new_current = find_runnable();
        ensure_running(new_current);
        current = new_current;
    }

    lock_leave(&lock);
    obj_release(&old_current->obj);
    longjmp(new_current->buf, 1);
}

thread_t *thread_start(void (*entry)(void*), void *arg) {
    int stack_size = 65536;
    char *stack_start = malloc(stack_size);
    void **stack_end = (void**) (stack_start + stack_size);
    stack_end--;
    *stack_end = arg;
    stack_end--;
    *stack_end = thread_exit;

    jmp_buf buf = { {
        .eax = 0, .ebx = 0, .ecx = 0, .edx = 0, .esi = 0, .edi = 0, .ebp = 0,
        .esp = (uint32_t) stack_end,
        .eip = (uint32_t) entry,
    } };
    
    thread_t *t = obj_alloc(sizeof(*t));
    t->buf[0] = buf[0];
    t->state = thread_running;
    _REENT_INIT_PTR(&t->reent);

    lock_enter(&lock);

    {
        LIST_ADD(running, t, u.running);
    }

    unlock_and_switch_to(t);
    return t;
}

void thread_wait(struct inbox_t *inbox) {
    thread_t *new_current;
    lock_enter(&lock);

    {
        thread_t *t = current;
        new_current = ensure_not_running(t);
        t->state = thread_waiting;
        t->u.waiting.inbox = obj_retain(&inbox->obj);
        LIST_ADD(waiting, t, u.waiting);
    }

    unlock_and_switch_to(new_current);
}

void thread_sleep(unsigned milliseconds) {
    thread_t *new_current;
    lock_enter(&lock);

    {
        thread_t *t = current;
        new_current = ensure_not_running(t);
        t->state = thread_sleeping;
        t->u.sleeping.until = uptime + milliseconds;
        LIST_ADD(sleeping, t, u.sleeping);
    }

    unlock_and_switch_to(new_current);
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
    return &current->reent;
}

void thread_init_reent() {
    _REENT_INIT_PTR(&current->reent);
}

void thread_init() {
    thread_yield();

    inbox_t *inbox = inbox_alloc();
    interrupt_subscribe(0, inbox, obj_alloc(sizeof(obj_t)));
    thread_start(timer_thread, inbox);
}
