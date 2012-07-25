#ifndef OBJ_H
#define OBJ_H

#include <stddef.h>

typedef struct {
    int refs;
    void (*dealloc)(void *);
} obj_t;

typedef struct pool_t {
    obj_t obj;
    struct pool_t *parent;
    struct array_t *objs;
} pool_t;

extern pool_t *obj_pool;

void *obj_alloc(size_t size);
void *obj_retain(obj_t *o);
int obj_release(obj_t *o);
pool_t *obj_alloc_pool();
void *obj_autorelease(obj_t *o);

#endif

