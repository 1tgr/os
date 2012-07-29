#include <stdlib.h>
#include "inbox.h"
#include "lock.h"
#include "thread.h"
#include "types.h"

static thread_t thread_idle, *thread_current = &thread_idle, *thread_running_first = &thread_idle, *thread_running_last = &thread_idle, *thread_waiting_first, *thread_waiting_last;
static lock_t lock;

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
    if (setjmp(thread_current->buf) == 0) {
        thread_current = t;
        lock_leave(&lock);
        longjmp(t->buf, 1);
    }
}

static thread_t *find_runnable() {
    for (thread_t *t = thread_waiting_first; t != NULL; t = t->u.waiting.next) {
        if (t->u.waiting.inbox->data->count > 0)
            return t;
    }

    if (thread_current->u.running.next != NULL)
       return thread_current->u.running.next;

    return thread_running_first;
}

void thread_yield() {
    thread_t *t;
    lock_enter(&lock);

    {
        t = find_runnable();

        if (t->state == thread_waiting) {
            LIST_REMOVE(thread_waiting, t, u.waiting);
            t->state = thread_running;
            LIST_ADD(thread_running, t, u.running);
        }
    }

    unlock_and_switch_to(t);
}

void thread_wait(struct inbox_t *inbox) {
    thread_t *new_current;
    lock_enter(&lock);

    {
        thread_t *t = thread_current;
        new_current = t->u.running.next == NULL ? thread_running_first : t->u.running.next;
        LIST_REMOVE(thread_running, t, u.running);
        t->state = thread_waiting;
        t->u.waiting.inbox = obj_retain(&inbox->obj);
        LIST_ADD(thread_waiting, t, u.waiting);
    }

    unlock_and_switch_to(new_current);
}

void thread_exit() {
    thread_t *old_current, *new_current;
    lock_enter(&lock);

    {
        old_current = thread_current;
        LIST_REMOVE(thread_running, thread_current, u.running);
        new_current = find_runnable();
        thread_current = new_current;
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

    lock_enter(&lock);

    {
        LIST_ADD(thread_running, t, u.running);
    }

    unlock_and_switch_to(t);
    return t;
}

