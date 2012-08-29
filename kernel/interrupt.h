#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "inbox.h"
#include "obj.h"

void interrupt_subscribe(unsigned n, inbox_t *inbox, obj_t *data);
void interrupt_deliver(unsigned n);

#endif
