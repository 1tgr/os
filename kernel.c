#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inbox.h"
#include "interrupt.h"
#include "lock.h"
#include "screen.h"
#include "thread.h"

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

static void outb(uint16_t port, uint8_t val) {
    __asm("outb %0, %1" : : "a"(val), "Nd"(port));
}

#define PIC_M 0x20
#define PIC_S 0xA0

volatile uint32_t *const EOI        = (volatile uint32_t *) 0xfee000b0;
volatile uint32_t *const SVR        = (volatile uint32_t *) 0xfee000f0;
volatile uint32_t *const LVT_TMR    = (volatile uint32_t *) 0xfee00320;
volatile uint32_t *const ICR_LOW    = (volatile uint32_t *) 0xfee00300;
volatile uint32_t *const TMRINITCNT = (volatile uint32_t *) 0xfee00380;
volatile uint32_t *const TMRDIV     = (volatile uint32_t *) 0xfee003e0;

void i386_isr(i386_context_t context) {
    uint8_t *p = ((uint8_t *) 0xb8000) + (80 - thread_get_current_cpu()->num - 1) * 2;
    if (context.error == (uint32_t) -1) {
        *p = ~*p;

        if (context.interrupt > 0) {
            printf("Interrupt %d\n", (int) context.interrupt);
        }

        __asm("sti");
        outb(PIC_M, 0x20);
        outb(PIC_S, 0x20);
        *EOI = 0;
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

    outb(PIC_M + 1, 1);                 // mask IRQ0 = disable timer
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

static void set_descriptor(descriptor_t *item, uint32_t base, uint32_t limit, uint8_t access, uint8_t attribs) {
    item->base_l = base & 0xFFFF;
    item->base_m = (base >> 16) & 0xFF;
    item->base_h = base >> 24;
    item->limit = limit & 0xFFFF;
    item->attribs = attribs | ((limit >> 16) & 0x0F);
    item->access = access;
}

static void set_gdt(descriptor_t *gdt, void *fs_base) {
    set_descriptor(gdt + 0, 0, 0, 0, 0);
    set_descriptor(gdt + 1, 0, 0xfffff, ACS_CODE | ACS_DPL_0, ATTR_DEFAULT | ATTR_GRANULARITY);
    set_descriptor(gdt + 2, 0, 0xfffff, ACS_DATA | ACS_DPL_0, ATTR_BIG | ATTR_GRANULARITY);
    set_descriptor(gdt + 3, (uintptr_t) fs_base, 0xfffff, ACS_DATA | ACS_DPL_0, ATTR_BIG | ATTR_GRANULARITY);
}

static void set_idt_descriptor(descriptor_int_t *d, void (*handler)()) {
    uint32_t offset = (uintptr_t) handler;
    d->offset_l = offset;
    d->selector = 8;
    d->param_cnt = 0;
    d->access = 0x8e;
    d->offset_h = offset >> 16;
}

static void set_idt(descriptor_int_t *idt) {
    int i = 0;

#define exception(n) set_idt_descriptor(&idt[i++], exception_##n);
#define irq(n) set_idt_descriptor(&idt[i++], irq_##n);
#define interrupt(n) set_idt_descriptor(&idt[i++], interrupt_##n);
#include "interrupt_handlers.h"
#undef exception
#undef irq
#undef interrupt

    while (i < 256)
        set_idt_descriptor(&idt[i++], interrupt_30);

}

#define INT32_BYTES(l) ((l) >> 24) & 0xff, ((l) >> 16) & 0xff, ((l) >> 8) & 0xff, (l) & 0xff

uint32_t i386_smp_lock_1, i386_smp_lock_2 = 1;
uint32_t i386_cpu_count = 1;
descriptor_t i386_gdt[4];
static descriptor_int_t idt[256];
static uint32_t idtr[] = { sizeof(idt) << 16, (uintptr_t) idt };
extern cpu_t **cpus;

static void i386_init_timer() {
    *SVR = *SVR | 0x100 | 39;
    *LVT_TMR = 32 | 0x20000;
    *TMRINITCNT = 10000000; // TODO calibrate APIC counter to bus frequency
	*TMRDIV = 3;
}

static void i386_init_smp() {
    extern uint8_t trampoline[1], trampoline_locate[1], trampoline_end[1];
    printf("Hello from bootstrap processor!\n");

    *ICR_LOW = 0xc4500;
    thread_sleep(10);

    uint8_t *trampoline_low = (uint8_t*) 0x1000;
    memcpy(trampoline_low, trampoline, trampoline_end - trampoline);

    uint8_t *trampoline_locate_low = trampoline_low + (trampoline_locate - trampoline);
    *(void**) (trampoline_locate_low + 2) = trampoline_low;

    uint32_t trampoline_low8 = (uintptr_t) trampoline_low / 4096;
    *ICR_LOW = 0xc4600 | trampoline_low8;
    thread_sleep(1); // should be 200 microseconds

    *ICR_LOW = 0xc4600 | trampoline_low8;
    thread_sleep(100);

    printf("Got %d CPUs\n", (int) i386_cpu_count);
    thread_set_cpu_count(i386_cpu_count);
    __sync_lock_release(&i386_smp_lock_2);
}

void i386_ap_main(int cpu_num) {
    descriptor_t gdt_copy[sizeof(i386_gdt) / sizeof(*i386_gdt)];
    set_gdt(gdt_copy, cpus[cpu_num]);

    uint32_t gdtr[] = { sizeof(gdt_copy) << 16, (uintptr_t) gdt_copy };
    __asm(
        "lgdt %0\n"
        "mov %1, %%fs\n"
        "lidt %2\n"
        "sti\n"
        : : "m" (((char *) gdtr)[2]), "r" (0x18), "m" (((char *) idtr)[2])
    );

    cpu_t *cpu = thread_get_current_cpu();
    printf("Hello from application processor %d! @ stack = %p, cpu = %p, current = %p\n", cpu_num, &cpu_num, cpu, cpu->current);
    i386_init_timer();
    thread_yield();

    while (1)
        __asm("hlt");
}

static void init_thread(void *arg) {
    i386_init_smp();
    printf("Finished initialization\n");
    thread_start(test_thread, NULL);
}

void kmain(void) {
    screen_clear();
    thread_set_cpu_count(1);
    set_gdt(i386_gdt, cpus[0]);
    set_idt(idt);

    uint32_t gdtr[] = { sizeof(i386_gdt) << 16, (uintptr_t) i386_gdt };
    __asm(
        "lgdt %0\n"
        "ljmp %1,$reload_cs\n"
        "reload_cs:\n"
        "mov %2, %%ds\n"
        "mov %2, %%es\n"
        "mov %2, %%gs\n"
        "mov %2, %%ss\n"
        "mov %3, %%fs\n"
        "lidt %4\n"
        : : "m" (((char *) gdtr)[2]), "i" (0x8), "r" (0x10), "r" (0x18), "m" (((char *) idtr)[2])
    );

    i386_init_pic(32, 40);
    thread_init();
    __asm("sti");
    i386_init_timer();
    thread_start(init_thread, NULL);

    while (1)
        __asm("hlt");
}
