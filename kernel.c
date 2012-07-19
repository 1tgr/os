#include <stdio.h>
#include <stdlib.h>

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
	uint32_t intr, error;
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

typedef struct thread_t {
    struct thread_t *prev, *next;
    void *kernel_esp;
} thread_t;

void isr_switch_ret(i386_context_t *context);

static uint8_t* const video = (uint8_t*)0xb8000;
static int x, y;
static thread_t *thread_first, *thread_last;
 
static void outb(uint16_t port, uint8_t val) {
    __asm("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor(int position) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, position);
    outb(0x3D4, 0x0E);
    outb(0x3D5, position >> 8);
}

void i386_isr(void *kernel_esp, i386_context_t context) {
    printf("returning to cs:eip = %04x:%08x, esp = %p\n", context.cs, context.eip, kernel_esp);
    isr_switch_ret(kernel_esp);
}

int write(int file, char *ptr, int len) {
    if (file == 1) {
        uint8_t *write = video + (y * 80 + x) * 2;

        for (int i = 0; i < len; i++) {
            switch (ptr[i]) {
                case '\n':
                    x = 0;
                    y++;
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
    __asm("hlt");
}

thread_t *thread_start(void (*entry)(void*), void *arg) {
    int stack_size = 65536;
    uint8_t *stack_start = malloc(stack_size);
    void *stack_end = stack_start + stack_size - sizeof(i386_context_t);
    uint16_t code_segment, data_segment;
    __asm("mov %%cs, %0" : "=r"(code_segment));
    __asm("mov %%ds, %0" : "=r"(data_segment));

    i386_context_t context = {
        .fault_addr = 0,
        .regs = { 0, 0, 0, 0, 0, 0, 0, 0, },
        .gs = data_segment,
        .fs = data_segment,
        .es = data_segment,
        .ds = data_segment,
        .intr = 0,
        .error = 0,
        .eip = (uint32_t) entry,
        .cs = code_segment,
        .eflags = 0,
        .u.kernel_mode = {
            .caller = (uint32_t) halt,
            .arg = (uint32_t) arg
        },
    };
    
    *(i386_context_t *) stack_end = context;

    thread_t *t = malloc(sizeof(*t));
    t->prev = thread_last;
    t->next = NULL;
    t->kernel_esp = stack_end;

    if (thread_last != NULL)
        thread_last->next = t;

    if (thread_first == NULL)
        thread_first = t;

    thread_last = t;
    return t;
}

static void thread(void *arg) {
    printf("hello from %s (%p) (%p)\n", (char *) arg, arg, &arg);
}

void kmain(void) {
    for (int i = 0; i < 80 * 25; i++) {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 7;
    }

    x = y = 0;
    update_cursor(0);
    printf("hello world\n");

    thread_t *t = thread_start(thread, "thread 1");
    isr_switch_ret(t->kernel_esp);
}
