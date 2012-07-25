#ifndef INBOX_H
#define INBOX_H

#include "thread.h"
#include "array.h"

typedef struct {
    obj_t obj;
    thread_t *waiter;
    array_t *data;
} inbox_t;

inbox_t *inbox_alloc();
void inbox_post(inbox_t *i, obj_t *o);
obj_t *inbox_read(inbox_t *i);

#endif
