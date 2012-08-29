#ifndef ARRAY_H
#define ARRAY_H

#include "obj.h"

typedef struct array_t {
    obj_t obj;
    obj_t **objs;
    size_t count;
} array_t;

array_t *array_alloc();
void array_add(array_t *a, obj_t *o);

#endif

