; Copyright (c) 2017, 2020 Konstantin Tcholokachvili.
; All rights reserved.
; Use of this source code is governed by a MIT license that can be
; found in the LICENSE file.

; Interrupt Requests (IRQs)

section .text

; The address of the table of handlers (defined in irq.c)
[extern x86_irq_handler_array]

; The address of the table of wrappers (defined below, and shared with irq.c)
[global x86_irq_wrapper_array]

;
[extern g_pit_ticks]
[extern g_localApicAddr]
[global pit_interrupt]
[global spurious_interrupt_handler]
[extern timer_interrupt_handler]

; Same packed layout of cpu_state, other irq wrappers (o16)
%macro SAVE_REGISTERS 0
	push edi
	push esi
	push edx
	push ecx
	push ebx
	push eax
	sub  esp, 2
	o16 push word ss
	o16 push word ds
	o16 push word es
	o16 push word fs
	o16 push word gs
%endmacro

%macro RESTORE_REGISTERS 0
	o16 pop word  gs
	o16 pop word  fs
	o16 pop word  es
	o16 pop word  ds
	o16 pop word  ss
	add esp, 2
	pop eax
	pop ebx
	pop ecx
	pop edx
	pop esi
	pop edi
	pop ebp
%endmacro

; These pre-handlers are for IRQ (Master PIC)
%macro X86_IRQ_WRAPPER_MASTER 1
	align 4

	x86_irq_wrapper_%1:
		; Fake error code
		push 0

		; Backup the actual context
		push ebp
		mov ebp, esp

		SAVE_REGISTERS

		; Call the handler with IRQ number as argument
		push %1
		lea  edi, [x86_irq_handler_array]
		call [edi+4*%1]
		add  esp, 4

		; LAPIC EOI (PIC is masked; IRQs arrive via IOAPIC)
		mov edi, [g_localApicAddr]
		add edi, 0xb0
		xor eax, eax
		stosd

		RESTORE_REGISTERS

		; Remove fake error code
		add esp, 4

		iret
%endmacro

; These pre-handlers are for IRQ (Slave PIC)
%macro X86_IRQ_WRAPPER_SLAVE 1
	align 4

	x86_irq_wrapper_%1:
		; Fake error code
		push 0

		; Backup the actual context
		push ebp
		mov ebp, esp

		SAVE_REGISTERS

		; Call the handler with IRQ number as argument
		push %1
		lea  edi, [x86_irq_handler_array]
		call [edi+4*%1]
		add  esp, 4

		; LAPIC EOI (PIC is masked; IRQs arrive via IOAPIC)
		mov edi, [g_localApicAddr]
		add edi, 0xb0
		xor eax, eax
		stosd

		RESTORE_REGISTERS

		; Remove fake error code
		add  esp, 4

		iret
%endmacro

X86_IRQ_WRAPPER_MASTER 0
X86_IRQ_WRAPPER_MASTER 1
X86_IRQ_WRAPPER_MASTER 2
X86_IRQ_WRAPPER_MASTER 3
X86_IRQ_WRAPPER_MASTER 4
X86_IRQ_WRAPPER_MASTER 5
X86_IRQ_WRAPPER_MASTER 6
X86_IRQ_WRAPPER_MASTER 7
X86_IRQ_WRAPPER_SLAVE  8
X86_IRQ_WRAPPER_SLAVE  9
X86_IRQ_WRAPPER_SLAVE  10
X86_IRQ_WRAPPER_SLAVE  11
X86_IRQ_WRAPPER_SLAVE  12
X86_IRQ_WRAPPER_SLAVE  13
X86_IRQ_WRAPPER_SLAVE  14
X86_IRQ_WRAPPER_SLAVE  15

; PIT interrupt
pit_interrupt:
	; Fake error code + frame, matching the IRQ wrappers
	push 0
	push ebp
	mov  ebp, esp
	SAVE_REGISTERS

	mov edi, [g_localApicAddr]
	add edi, 0xb0 ; Write to the register with offset 0xB0...
	xor eax, eax  ; ...using the value 0 to signal an end of interrupt.
	stosd

	; Call the C handler (argument is unused)
	push 0
	call timer_interrupt_handler
	add  esp, 4

	RESTORE_REGISTERS

	; Remove fake error code
	add  esp, 4
	iret

; Spurious interrupt
spurious_interrupt_handler:
	iret

section .rodata

; Build the x86_irq_wrapper_array, shared with irq.c
align 32, db 0
x86_irq_wrapper_array:
	dd x86_irq_wrapper_0
	dd x86_irq_wrapper_1
	dd x86_irq_wrapper_2
	dd x86_irq_wrapper_3
	dd x86_irq_wrapper_4
	dd x86_irq_wrapper_5
	dd x86_irq_wrapper_6
	dd x86_irq_wrapper_7
	dd x86_irq_wrapper_8
	dd x86_irq_wrapper_9
	dd x86_irq_wrapper_10
	dd x86_irq_wrapper_11
	dd x86_irq_wrapper_12
	dd x86_irq_wrapper_13
	dd x86_irq_wrapper_14
	dd x86_irq_wrapper_15

section .note.GNU-stack noalloc noexec nowrite
