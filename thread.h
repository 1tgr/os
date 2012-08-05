#ifndef THREAD_H
#define THREAD_H

#include <setjmp.h>
#include "obj.h"

typedef enum {
    thread_running,
    thread_waiting,
    thread_sleeping,
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
        struct {
            struct thread_t *prev, *next;
            unsigned until;
        } sleeping;
    } u;
} thread_t;

void thread_exit();
unsigned thread_get_quantum();
unsigned thread_get_uptime();
void thread_init();
void thread_sleep(unsigned milliseconds);
thread_t *thread_start(void (*entry)(void*), void *arg);
void thread_wait(struct inbox_t *inbox);
void thread_yield();

#endif

