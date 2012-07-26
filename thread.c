#include <stdlib.h>
#include "thread.h"
#include "types.h"

thread_t thread_idle, *thread_first = &thread_idle, *thread_last = &thread_idle, *thread_current = &thread_idle;

void thread_switch_to(thread_t *t) {
    if (setjmp(thread_current->buf) == 0) {
        thread_current = t;
        longjmp(thread_current->buf, 1);
    }
}

void thread_yield() {
    thread_switch_to(thread_current->next == NULL ? thread_first : thread_current->next);
}

static void halt() {
    while (1) {
        __asm("hlt");
    }
}

thread_t *thread_start(void (*entry)(void*), void *arg) {
    int stack_size = 65536;
    char *stack_start = malloc(stack_size);
    void **stack_end = (void**) (stack_start + stack_size);
    stack_end--;
    *stack_end = arg;
    stack_end--;
    *stack_end = halt;

    jmp_buf buf = { {
        .eax = 0, .ebx = 0, .ecx = 0, .edx = 0, .esi = 0, .edi = 0, .ebp = 0,
        .esp = (uint32_t) stack_end,
        .eip = (uint32_t) entry,
    } };
    
    thread_t *t = obj_alloc(sizeof(*t));
    t->prev = thread_last;
    t->next = NULL;
    t->buf[0] = buf[0];
    
    if (thread_last != NULL)
        thread_last->next = t;

    if (thread_first == NULL)
        thread_first = t;

    thread_last = t;
    thread_switch_to(t);
    return t;
}

