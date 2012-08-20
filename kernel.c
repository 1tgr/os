#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inbox.h"
#include "interrupt.h"
#include "thread.h"
#include "types.h"

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

static uint8_t* const video = (uint8_t*)0xb8000;
static int x, y;

static void outb(uint16_t port, uint8_t val) {
    __asm("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor(int position) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, position);
    outb(0x3D4, 0x0E);
    outb(0x3D5, position >> 8);
}

#define PIC_M 0x20
#define PIC_S 0xA0

void i386_isr(i386_context_t context) {
    if (context.error == (uint32_t) -1) {
        __asm("sti");
        outb(PIC_M, 0x20);
        outb(PIC_S, 0x20);
        interrupt_deliver(context.interrupt);
        __asm("cli");
    }
}

static void i386_init_pic(uint8_t master_vector, uint8_t slave_vector) {
    outb(PIC_M, 0x11);                  // start 8259 initialization
    outb(PIC_S, 0x11);
    outb(PIC_M + 1, master_vector);     // master base interrupt vector
    outb(PIC_S + 1, slave_vector);      // slave base interrupt vector
    outb(PIC_M + 1, 1<<2);              // bitmask for cascade on IRQ2
    outb(PIC_S + 1, 2);                 // cascade on IRQ2
    outb(PIC_M + 1, 1);                 // finish 8259 initialization
    outb(PIC_S + 1, 1);

    outb(PIC_M + 1, 0);                 // enable all IRQs on master
    outb(PIC_S + 1, 0);                 // enable all IRQs on slave
}

static void i386_init_timer(unsigned hz) {
    uint16_t n = 3579545L / (hz * 3);
    outb(0x43, 0x36);
    outb(0x40, n & 0xff);
    outb(0x40, n >> 8);
}

int write(int file, char *ptr, int len) {
    if (file == 1) {
        uint8_t *write = video + (y * 80 + x) * 2;

        for (int i = 0; i < len; i++) {
            outb(0xe9, ptr[i]);

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

void test_thread(void *arg);

#define exception(n) void exception_##n();
#define irq(n) void irq_##n();
#define interrupt(n) void interrupt_##n();
#include "interrupt_handlers.h"
#undef exception
#undef irq
#undef interrupt

/* Access byte's flags */
#define ACS_PRESENT     0x80            /* present segment */
#define ACS_CSEG        0x18            /* code segment */
#define ACS_DSEG        0x10            /* data segment */
#define ACS_CONFORM     0x04            /* conforming segment */
#define ACS_READ        0x02            /* readable segment */
#define ACS_WRITE       0x02            /* writable segment */
#define ACS_IDT         ACS_DSEG        /* segment type is the same type */
#define ACS_INT_GATE    0x0E            /* int gate for 386 */
#define ACS_TSS_GATE    0x09
#define ACS_DPL_0       0x00            /* descriptor privilege level #0 */
#define ACS_DPL_1       0x20            /* descriptor privilege level #1 */
#define ACS_DPL_2       0x40            /* descriptor privilege level #2 */
#define ACS_DPL_3       0x60            /* descriptor privilege level #3 */
#define ACS_LDT         0x02            /* ldt descriptor */
#define ACS_TASK_GATE   0x05

/* Ready-made values */
#define ACS_INT         (ACS_PRESENT | ACS_INT_GATE) /* present int gate */
#define ACS_TSS         (ACS_PRESENT | ACS_TSS_GATE) /* present tss gate */
#define ACS_TASK        (ACS_PRESENT | ACS_TASK_GATE) /* present task gate */
#define ACS_CODE        (ACS_PRESENT | ACS_CSEG | ACS_READ)
#define ACS_DATA        (ACS_PRESENT | ACS_DSEG | ACS_WRITE)
#define ACS_STACK       (ACS_PRESENT | ACS_DSEG | ACS_WRITE)

/* Attributes (in terms of size) */
#define ATTR_GRANULARITY 0x80           /* segment limit is given in 4KB pages rather than in bytes */
#define ATTR_BIG         0x40           /* ESP is used rather than SP */
#define ATTR_DEFAULT     0x40           /* 32-bit code segment rather than 16-bit */

static void set_descriptor(descriptor_t *item, uint32_t limit, uint8_t access, uint8_t attribs) {
    item->base_l = 0;
    item->base_m = 0;
    item->base_h = 0;
    item->limit = limit & 0xFFFF;
    item->attribs = attribs | ((limit >> 16) & 0x0F);
    item->access = access;
}

static void set_gdt(descriptor_t *gdt) {
    set_descriptor(gdt + 0, 0, 0, 0);
    set_descriptor(gdt + 1, 0xfffff, ACS_CODE | ACS_DPL_0, ATTR_DEFAULT | ATTR_GRANULARITY);
    set_descriptor(gdt + 2, 0xfffff, ACS_DATA | ACS_DPL_0, ATTR_BIG | ATTR_GRANULARITY);
}

static void set_idt(descriptor_int_t *d, void (*handler)()) {
    uint32_t offset = (uint32_t) handler;
    d->offset_l = offset;
    d->selector = 8;
    d->param_cnt = 0;
    d->access = 0x8e;
    d->offset_h = offset >> 16;
}

#define INT32_BYTES(l) ((l) >> 24) & 0xff, ((l) >> 16) & 0xff, ((l) >> 8) & 0xff, (l) & 0xff

uint32_t i386_smp_lock_1, i386_smp_lock_2 = 1;
volatile int i386_cpu_count = 1;

static void i386_init_smp() {
    extern uint8_t trampoline[1], trampoline_locate[1], trampoline_end[1];
    volatile uint32_t *SVR     = (volatile uint32_t *) 0xfee000f0;
    volatile uint32_t *ICR_LOW = (volatile uint32_t *) 0xfee00300;
    printf("Hello from bootstrap processor!\n");

    *SVR = *SVR | 0x100;
    *ICR_LOW = 0xc4500;
    thread_sleep(10);

    uint8_t *trampoline_low = (uint8_t*) 0x1000;
    memcpy(trampoline_low, trampoline, trampoline_end - trampoline);

    uint8_t *trampoline_locate_low = trampoline_low + (trampoline_locate - trampoline);
    *(void**) (trampoline_locate_low + 2) = trampoline_low;

    uint32_t trampoline_low8 = (uint32_t) trampoline_low / 4096;
    *ICR_LOW = 0xc4600 | trampoline_low8;
    thread_sleep(1); // should be 200 microseconds

    *ICR_LOW = 0xc4600 | trampoline_low8;
    thread_sleep(100);

    printf("Got %d CPUs\n", i386_cpu_count);
    __sync_lock_release(&i386_smp_lock_2);
}

void i386_ap_main(int cpu_num) {
    printf("Hello from application processor %d! @ %p\n", cpu_num, &cpu_num);
}

static void init_thread(void *arg) {
    i386_init_smp();
    printf("Finished initialization\n");
}

void kmain(void) {
    static descriptor_t gdt[3];
    static descriptor_int_t idt[256];
    extern descriptor_t trampoline_gdt[1];

    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 7;
    }

    update_cursor(0);
    set_gdt(gdt);
    set_gdt(trampoline_gdt);
    thread_init_reent();

    {
        putchar('*');
        fflush(stdout);

        uint32_t gdtr[] = { sizeof(gdt) << 16, (uint32_t) gdt };
        __asm(
            "lgdt %0\n"
            "ljmp %1,$reload_cs\n"
            "reload_cs:\n"
            "mov %2, %%ds\n"
            "mov %2, %%es\n"
            "mov %2, %%fs\n"
            "mov %2, %%gs\n"
            "mov %2, %%ss\n"
            : : "m" (((char *) gdtr)[2]), "i" (8), "r" (16)
        );
    }

    {
        putchar('*');
        fflush(stdout);

        int i = 0;

#define exception(n) set_idt(&idt[i++], exception_##n);
#define irq(n) set_idt(&idt[i++], irq_##n);
#define interrupt(n) set_idt(&idt[i++], interrupt_##n);
#include "interrupt_handlers.h"
#undef exception
#undef irq
#undef interrupt

        while (i < 256)
            set_idt(&idt[i++], interrupt_30);

        uint32_t idtr[] = { sizeof(idt) << 16, (uint32_t) idt };
        __asm("lidt %0" : : "m" (((char *) idtr)[2]));
    }

    i386_init_pic(32, 40);
    i386_init_timer(1000 / thread_get_quantum());
    thread_init();
    puts("*");
    __asm("sti");
    thread_start(init_thread, NULL);
    thread_start(test_thread, NULL);

    while (1)
        __asm("hlt");
}
