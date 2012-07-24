#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <malloc.h>
#include <string.h>
#include "cutest/CuTest.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct {
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
} i386_pusha_t;

typedef struct {
	uint32_t fault_addr;
	i386_pusha_t regs;
	uint32_t gs, fs, es, ds;
	uint32_t interrupt, error;
	uint32_t eip, cs, eflags;
    union {
        struct {
            uint32_t esp, ss;
        } user_mode;
        struct {
            uint32_t caller, arg;
        } kernel_mode;
    } u;
} i386_context_t;

typedef struct {
	uint16_t limit;
	uint16_t base_l;
	uint8_t base_m;
	uint8_t access;
	uint8_t attribs;
	uint8_t base_h;
} __attribute__((packed)) descriptor_t;

typedef struct {
	uint16_t offset_l, selector;
	uint8_t param_cnt, access;
	uint16_t offset_h;
} __attribute__((packed)) descriptor_int_t;

typedef struct {
    int refs;
    void (*dealloc)(void *);
} obj_t;

typedef struct {
    obj_t obj;
    obj_t **objs;
    size_t count;
} array_t;

typedef struct pool_t {
    obj_t obj;
    struct pool_t *parent;
    array_t *objs;
} pool_t;

typedef struct thread_t {
    obj_t obj;
    struct thread_t *prev, *next;
    jmp_buf buf;
} thread_t;

typedef struct {
    obj_t obj;
    thread_t *waiter;
    array_t *data;
} inbox_t;

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

static pool_t *obj_pool;

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

static uint8_t* const video = (uint8_t*)0xb8000;
static int x, y;
thread_t thread_idle, *thread_first = &thread_idle, *thread_last = &thread_idle, *thread_current = &thread_idle;
 
static void outb(uint16_t port, uint8_t val) {
    __asm("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor(int position) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, position);
    outb(0x3D4, 0x0E);
    outb(0x3D5, position >> 8);
}

void i386_isr(i386_context_t context) {
    context = context;

    if (setjmp(thread_current->buf) == 0) {
        thread_current = thread_current->next == NULL ? thread_first : thread_current->next;
        outb(0x20,0x20);
        outb(0xa0,0x20);
        longjmp(thread_current->buf, 1);
    }
}

int write(int file, char *ptr, int len) {
    if (file == 1) {
        uint8_t *write = video + (y * 80 + x) * 2;

        for (int i = 0; i < len; i++) {
            switch (ptr[i]) {
                case '\n':
                    x = 0;
                    y++;
                    if (y >= 25) {
                        y = 0;
                    }
                    write = video + (y * 80 + x) * 2;
                    break;

                default:
                    write[0] = ptr[i];
                    write[1] = 7;
                    write += 2;
                    x++;
                    if (x >= 80) {
                        x = 0;
                        y++;
                        if (y >= 25) {
                            y = 0;
                        }
                    }
                    break;
            }
        }

        update_cursor((write - video) / 2);
        return len;
    } else
        return -1;
}

static void halt() {
    while (1) {
        __asm("hlt");
    }
}

void thread_switch_to(thread_t *t) {
    if (setjmp(thread_current->buf) == 0) {
        thread_current = t;
        longjmp(thread_current->buf, 1);
    }
}

thread_t *thread_start(void (*entry)(void*), void *arg) {
    int stack_size = 65536;
    uint8_t *stack_start = malloc(stack_size);
    void **stack_end = (void**) (stack_start + stack_size);
    stack_end--;
    *stack_end = arg;
    stack_end--;
    *stack_end = halt;

    jmp_buf buf = { {
        .eax = 0, .ebx = 0, .ecx = 0, .edx = 0, .esi = 0, .edi = 0, .ebp = 0,
        .esp = (uint32_t) stack_end,
        .eip = (uint32_t) entry,
    } };
    
    thread_t *t = obj_alloc(sizeof(*t));
    t->prev = thread_last;
    t->next = NULL;
    t->buf[0] = buf[0];
    
    if (thread_last != NULL)
        thread_last->next = t;

    if (thread_first == NULL)
        thread_first = t;

    thread_last = t;
    thread_switch_to(t);
    return t;
}

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
    if (w != NULL) {
        i->waiter = NULL;
        thread_switch_to(w);
        obj_release(&w->obj);
    }
}

obj_t *inbox_read(inbox_t *i) {
    while (i->data->count == 0) {
        if (i->waiter == NULL)
            i->waiter = obj_retain(&thread_current->obj);
        
        __asm("hlt");
    }

    obj_t *o = obj_autorelease(i->data->objs[0]);
    i->data->count--;
    memmove(i->data->objs, i->data->objs + 1, i->data->count * sizeof(*i->data->objs));
    return o;
}

static void test_obj_alloc_release(CuTest *ct) {
    obj_t *o = obj_alloc(sizeof(*o));
	CuAssertIntEquals(ct, 1, o->refs);
    CuAssertIntEquals(ct, 0, obj_release(o));
}

static void test_obj_alloc_retain_release(CuTest *ct) {
    obj_t *o = obj_alloc(sizeof(*o));
	CuAssertIntEquals(ct, 1, o->refs);

    obj_t *o2 = obj_retain(o);
    CuAssertPtrEquals(ct, o, o2);
    CuAssertIntEquals(ct, 2, o2->refs);
    CuAssertIntEquals(ct, 1, obj_release(o2));
    CuAssertIntEquals(ct, 0, obj_release(o2));
}

static void test_obj_alloc_pool_autorelease(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *p = obj_alloc_pool();
    CuAssertIntEquals(ct, 0, p->objs->count);
    CuAssertPtrEquals(ct, p, obj_pool);

    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    CuAssertIntEquals(ct, 1, p->objs->count);
    CuAssertIntEquals(ct, 0, obj_release(&p->obj));
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_obj_alloc_pool_parent_autorelease(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *outer = obj_alloc_pool();
    CuAssertIntEquals(ct, 0, outer->objs->count);
    CuAssertPtrEquals(ct, outer, obj_pool);

    pool_t *inner = obj_alloc_pool();
    CuAssertIntEquals(ct, 0, outer->objs->count);
    CuAssertIntEquals(ct, 0, inner->objs->count);
    CuAssertPtrEquals(ct, inner, obj_pool);

    obj_t * o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    CuAssertIntEquals(ct, 1, inner->objs->count);
    CuAssertIntEquals(ct, 0, obj_release(&inner->obj));
    CuAssertIntEquals(ct, 0, obj_release(&outer->obj));
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_array_alloc(CuTest *ct) {
    array_t *a = array_alloc();
    CuAssertIntEquals(ct, 0, a->count);
    obj_release(&a->obj);
}

static void test_array_add_release(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    array_t *a = array_alloc();
    CuAssertIntEquals(ct, 0, a->count);

    obj_t *o = obj_alloc(sizeof(*o));
    o->dealloc = dealloc;
    array_add(a, o);
    CuAssertIntEquals(ct, 1, obj_release(o));
    CuAssertIntEquals(ct, 0, is_dealloc);
    CuAssertIntEquals(ct, 0, obj_release(&a->obj));
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_inbox_post(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *pool = obj_alloc_pool();
    inbox_t *i = obj_autorelease(&inbox_alloc()->obj);
    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;

    inbox_post(i, o);
    obj_release(&pool->obj);
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void test_inbox_post_read_sync(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *pool = obj_alloc_pool();
    inbox_t *i = obj_autorelease(&inbox_alloc()->obj);
    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    inbox_post(i, o);

    obj_t *o2 = inbox_read(i);
    CuAssertPtrEquals(ct, o, o2);
    obj_release(&pool->obj);
    CuAssertIntEquals(ct, 1, is_dealloc);
}

static void async_inbox_reader(void *arg) {
    void **args = arg;
    inbox_t *inbox = args[0];
    obj_t **o = args[1];
    *o = inbox_read(inbox);
}

static void test_inbox_post_read_async(CuTest *ct) {
    int is_dealloc = 0;

    void dealloc(void *o) {
        is_dealloc++;
    }

    pool_t *pool = obj_alloc_pool();
    inbox_t *i = obj_autorelease(&inbox_alloc()->obj);
    volatile obj_t *result = NULL;
    void *args[] = { i, &result };
    thread_start(async_inbox_reader, args);

    obj_t *o = obj_autorelease(obj_alloc(sizeof(*o)));
    o->dealloc = dealloc;
    inbox_post(i, o);

    obj_t *o2 = (obj_t*) result;
    CuAssertPtrEquals(ct, o, o2);
    obj_release(&pool->obj);
    CuAssertIntEquals(ct, 1, is_dealloc);
}

void test_thread(void *arg) {
	CuSuite* suite = CuSuiteNew();

    {
        CuSuite* s = CuSuiteNew();
        SUITE_ADD_TEST(s, test_obj_alloc_release);
        SUITE_ADD_TEST(s, test_obj_alloc_retain_release);
        SUITE_ADD_TEST(s, test_obj_alloc_pool_autorelease);
        SUITE_ADD_TEST(s, test_obj_alloc_pool_parent_autorelease);
        CuSuiteAddSuite(suite, s);
    }

    {
        CuSuite* s = CuSuiteNew();
        SUITE_ADD_TEST(s, test_array_alloc);
        SUITE_ADD_TEST(s, test_array_add_release);
        CuSuiteAddSuite(suite, s);
    }

    {
        CuSuite* s = CuSuiteNew();
        SUITE_ADD_TEST(s, test_inbox_post);
        SUITE_ADD_TEST(s, test_inbox_post_read_sync);
        SUITE_ADD_TEST(s, test_inbox_post_read_async);
        CuSuiteAddSuite(suite, s);
    }

	CuSuiteRun(suite);

	CuString *output = CuStringNew();
	CuSuiteSummary(suite, output);
	CuSuiteDetails(suite, output);
	printf("%s\n", output->buffer);
}

#define exception(n) void exception_##n();
#define irq(n) void irq_##n();
#define interrupt(n) void interrupt_##n();
#include "interrupt_handlers.h"
#undef exception
#undef irq
#undef interrupt

static void set_idt(descriptor_int_t *d, uint16_t code_segment, void (*handler)()) {
    uint32_t offset = (uint32_t) handler;
    d->offset_l = offset;
    d->selector = code_segment;
    d->param_cnt = 0;
    d->access = 0x8e;
    d->offset_h = offset >> 16;
}

static void lidt(void *base, unsigned int size) {
    uint32_t i[] = { size << 16, (uint32_t) base };
    __asm( "lidt (%0)" : : "p"(((char *) i)+2) );
}

void kmain(void) {
    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 7;
    }

    x = y = 0;
    update_cursor(0);

    descriptor_int_t *idt = malloc(sizeof(*idt) * 256);
    uint16_t code_segment;
    __asm("mov %%cs, %0" : "=r"(code_segment));

#define exception(n) set_idt(idt + 0x##n, code_segment, exception_##n);
#define irq(n) set_idt(idt + 0x##n, code_segment, irq_##n);
#define interrupt(n) set_idt(idt + 0x##n, code_segment, interrupt_##n);
#include "interrupt_handlers.h"
#undef exception
#undef irq
#undef interrupt

    for (int i = 0x31; i < 256; i++) {
        set_idt(idt + i, code_segment, interrupt_30);
    }

    lidt(idt, sizeof(*idt) * 256);

    if (setjmp(thread_idle.buf) == 0) {
        __asm("sti");
        thread_start(test_thread, NULL);
    }

    halt();
}
