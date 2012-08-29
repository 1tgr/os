#ifndef INBOX_H
#define INBOX_H

#include "array.h"

typedef struct inbox_t {
    obj_t obj;
    array_t *data;
} inbox_t;

inbox_t *inbox_alloc();
void inbox_post(inbox_t *i, obj_t *o);
obj_t *inbox_read(inbox_t *i);

#endif
