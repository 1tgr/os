#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

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

typedef struct thread_t {
    struct thread_t *prev, *next;
    jmp_buf buf;
} thread_t;

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
    
    thread_t *t = malloc(sizeof(*t));
    t->prev = thread_last;
    t->next = NULL;
    t->buf[0] = buf[0];
    
    if (thread_last != NULL)
        thread_last->next = t;

    if (thread_first == NULL)
        thread_first = t;

    thread_last = t;
    return t;
}

static void thread1_entry(void *arg) {
    printf("hello from %s (%p) (%p)\n", (char *) arg, arg, &arg);
    for (int i = 0; i < 100; i++) {
        printf("1");
        fflush(stdout);
        __asm("hlt");
    }
}

static void thread2_entry(void *arg) {
    printf("hello from %s (%p) (%p)\n", (char *) arg, arg, &arg);
    for (int i = 0; i < 50; i++) {
        printf("2");
        fflush(stdout);
        __asm("hlt");
    }
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
    printf("hello world\n");

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
    thread_start(thread1_entry, "thread 1");
    thread_start(thread2_entry, "thread 2");
    if (setjmp(thread_idle.buf) == 0)
        __asm("sti");

    halt();
}
