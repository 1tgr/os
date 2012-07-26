#include <string.h>
#include "inbox.h"
#include "thread.h"

static void inbox_dealloc(void *o) {
    inbox_t *i = o;

    if (i->waiter != NULL)
        obj_release(&i->waiter->obj);

    obj_release(&i->data->obj);
}

inbox_t *inbox_alloc() {
    inbox_t *i = obj_alloc(sizeof(*i));
    i->obj.dealloc = inbox_dealloc;
    i->waiter = NULL;
    i->data = array_alloc();
    return i;
}

void inbox_post(inbox_t *i, obj_t *o) {
    array_add(i->data, o);

    thread_t *w = i->waiter;
    if (w == NULL)
        thread_yield();
    else {
        i->waiter = NULL;
        thread_switch_to(w);
        obj_release(&w->obj);
    }
}

obj_t *inbox_read(inbox_t *i) {
    while (i->data->count == 0) {
        if (i->waiter == NULL)
            i->waiter = obj_retain(&thread_current->obj);
        
        thread_yield();
    }

    obj_t *o = obj_autorelease(i->data->objs[0]);
    i->data->count--;
    memmove(i->data->objs, i->data->objs + 1, i->data->count * sizeof(*i->data->objs));
    return o;
}

