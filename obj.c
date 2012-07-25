#include <stdlib.h>
#include "array.h"
#include "obj.h"

pool_t *obj_pool;

void *obj_alloc(size_t size) {
    obj_t *o = malloc(size);
    o->refs = 1;
    o->dealloc = NULL;
    return o;
}

void *obj_retain(obj_t *o) {
    o->refs++;
    return o;
}

int obj_release(obj_t *o) {
    int refs = --o->refs;
    if (refs <= 0) {
        if (o->dealloc != NULL)
            o->dealloc(o);

        free(o);
    }

    return refs;
}

static void obj_dealloc_pool(void *o) {
    pool_t *p = o;
    obj_pool = p->parent;
    obj_release(&p->objs->obj);
}

pool_t *obj_alloc_pool() {
    pool_t *p = obj_alloc(sizeof(*p));
    p->obj.dealloc = obj_dealloc_pool;
    p->parent = obj_pool;
    p->objs = array_alloc();
    obj_pool = p;
    return p;
}

void *obj_autorelease(obj_t *o) {
    pool_t *p = obj_pool;
    array_add(p->objs, o);
    obj_release(o);
    return o;
}

