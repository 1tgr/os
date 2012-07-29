#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>
#include "obj.h"

typedef enum {
    thread_running,
    thread_waiting,
} thread_state;

typedef struct thread_t {
    obj_t obj;
    jmp_buf buf;
    thread_state state;
    union {
        struct {
            struct thread_t *prev, *next;
        } running;
        struct {
            struct thread_t *prev, *next;
            struct inbox_t *inbox;
        } waiting;
    } u;
} thread_t;

void thread_yield();
void thread_wait(struct inbox_t *inbox);
thread_t *thread_start(void (*entry)(void*), void *arg);

#endif

