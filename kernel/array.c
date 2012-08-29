#include <malloc.h>
#include "array.h"

static void array_dealloc(void *o) {
    array_t *a = o;

    for (size_t i = 0; i < a->count; i++) {
        obj_release(a->objs[i]);
    }

    free(a->objs);
}

array_t *array_alloc() {
    array_t *a = obj_alloc(sizeof(*a));
    a->obj.dealloc = array_dealloc;
    a->objs = malloc(sizeof(*a->objs) * 4);
    a->count = 0;
    return a;
}

void array_add(array_t *a, obj_t *o) {
    size_t size = malloc_usable_size(a->objs);
    if ((a->count + 1) * sizeof(*a->objs) > size) {
        a->objs = realloc(a->objs, size * 2);
    }

    a->objs[a->count++] = obj_retain(o);
}

