#ifndef THREAD_H
#define THREAD_H

#include <reent.h>
#include <setjmp.h>
#include "obj.h"

typedef enum {
    thread_current,
    thread_runnable,
    thread_waiting,
    thread_sleeping,
    thread_exited,
} thread_state;

typedef struct thread_t {
    obj_t obj;
    jmp_buf buf;
    thread_state state;
    union {
        struct {
            struct thread_t *prev, *next;
        } runnable;
        struct {
            struct thread_t *prev, *next;
            struct inbox_t *inbox;
        } waiting;
        struct {
            struct thread_t *prev, *next;
            unsigned until;
        } sleeping;
    } u;
    struct _reent reent;
} thread_t;

typedef struct cpu_t {
    struct cpu_t *self;
    thread_t *current;
    thread_t idle;
} cpu_t;

void thread_exit();
thread_t *thread_get_current();
cpu_t *thread_get_current_cpu();
unsigned thread_get_quantum();
unsigned thread_get_uptime();
void thread_init();
void thread_set_cpu_count(unsigned count);
void thread_sleep(unsigned milliseconds);
thread_t *thread_start(void (*entry)(void*), void *arg);
void thread_wait(struct inbox_t *inbox);
void thread_yield();

#endif

