#include "interrupt.h"
#include "lock.h"

static lock_t lock;
static inbox_t *handlers[16];
static obj_t *handler_data[16];

void interrupt_subscribe(unsigned n, inbox_t *inbox, obj_t *data) {
    lock_enter(&lock);
    handlers[n] = obj_retain(&inbox->obj);
    handler_data[n] = obj_retain(data);
    lock_leave(&lock);
}

void interrupt_deliver(unsigned n) {
    lock_enter(&lock);

    inbox_t *handler = handlers[n];
    obj_t *data = handler_data[n];

    lock_leave(&lock);

    if (handler != NULL)
        inbox_post(handler, data);
}
