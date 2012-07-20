	.text

isr_and_switch:
	push	%ds
	push	%es
	push	%fs
	push	%gs
	pushal

	mov		%cr2, %eax
	push	%eax
	xor		%eax, %eax
	mov		%eax, %cr2

/*
	mov		$KERNEL_FLAT_DATA, %bx
	mov		%ebx, %ds
	mov		%ebx, %es
	mov		%ebx, %gs
	mov		$USER_THREAD_INFO, %bx
	mov		%ebx, %fs

	mov		4 + _thr_cpu_single, %eax
	mov		(%eax), %eax
	push	%eax
*/

	mov		%esp, %ebx
	push	%ebx

	call	i386_isr

	.globl	isr_switch_ret
isr_switch_ret:
	lea		4(%eax), %esp

	popal
	pop		%gs
	pop		%fs
	pop		%es
	pop		%ds
	add		$8, %esp
	iretl

	.align	8


#define exception(n) \
	.globl	exception_##n ;\
	\
exception_##n: \
	pushl	$0x##n ;\
	jmp		isr_and_switch ;\
	.align	8


#define irq(n) \
	.globl	irq_##n ;\
	\
irq_##n: \
	pushl	$-1 ;\
	pushl	$0x##n; \
	jmp		isr_and_switch ;\
	.align	8
	
	
#define interrupt(n) \
	.globl	interrupt_##n ;\
	\
interrupt_##n: \
	pushl	$0 ;\
	pushl	$0x##n; \
	jmp		isr_and_switch ;\
	.align	8

#include "interrupt_handlers.h"	
