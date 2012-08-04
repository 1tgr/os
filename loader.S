.global loader                          # making entry point visible to linker

# setting up the Multiboot header - see GRUB docs for details
.set ALIGN,    1<<0                     # align loaded modules on page boundaries
.set MEMINFO,  1<<1                     # provide memory map
.set FLAGS,    ALIGN | MEMINFO          # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002               # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS)         # checksum required

.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# reserve initial kernel stack space
.set STACKSIZE, 0x4000                  # that is, 16k.
.lcomm stack, STACKSIZE                 # reserve 16k stack on a doubleword boundary
.comm  mbd, 4                           # we will use this in kmain
.comm  magic, 4                         # we will use this in kmain

loader:
    movl  $(stack + STACKSIZE), %esp    # set up the stack
    movl  %eax, magic                   # Multiboot magic number
    movl  %ebx, mbd                     # Multiboot data structure

    call  kmain                         # call kernel proper

    cli

hang:
    hlt                                 # halt machine should kernel return
    jmp   hang

.code16
.global trampoline
trampoline:
    cli

.global trampoline_locate
trampoline_locate:
    movl $0, %ebx

    mov %cs, %ax
    mov %ax, %ds
    movw $(3 * 8), trampoline_gdtr - trampoline(%ebx)
    movl $trampoline_gdt, trampoline_gdtr + 2 - trampoline(%ebx)
    lgdtl trampoline_gdtr - trampoline(%ebx) 
    mov $1, %eax
    mov %eax, %cr0
    ljmpl $0x8, $trampoline_end

trampoline_gdtr:
    .word 3 * 8
    .int -1

.code32
.global trampoline_end
trampoline_end:
    mov $0x10, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs
    mov %eax, %ss
    mov $i386_smp_lock, %edx

acquire_smp_lock:
    mov $1, %eax
    lock xchg %eax, (%edx)
    test %eax, %eax
    jne acquire_smp_lock

    mov (i386_smp_stack), %esp
    call i386_ap_main
    cli
    jmp hang

.global trampoline_gdt
trampoline_gdt:
    .fill 3, 8, 0 

