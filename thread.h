#ifndef THREAD_H
#define THREAD_H

#include "obj.h"
#include <setjmp.h>

typedef struct thread_t {
    obj_t obj;
    struct thread_t *prev, *next;
    jmp_buf buf;
} thread_t;

extern thread_t thread_idle, *thread_first, *thread_last, *thread_current;

void thread_switch_to(thread_t *t);
void thread_yield();
thread_t *thread_start(void (*entry)(void*), void *arg);

#endif

