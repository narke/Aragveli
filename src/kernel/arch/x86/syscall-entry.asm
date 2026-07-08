; Copyright (c) 2018 Konstantin Tcholokachvili.
; All rights reserved.
; Use of this source code is governed by a MIT license that can be
; found in the LICENSE file.

; System call entry stub (int 0x80).

section .text

[extern syscall_dispatch]
[global syscall_stub]

; Entered from ring 3 via `int 0x80`. The CPU has already switched to the
; kernel stack (TSS.esp0) and pushed the interrupt frame. We save the user
; register state, build a struct syscall_frame pointer, and hand off to the
; C dispatcher. On a normal syscall we restore state and iret back to user.
syscall_stub:
	cli
	pusha			; eax, ecx, edx, ebx, esp, ebp, esi, edi
	push ds
	push es
	push fs
	push gs

	mov ax, 0x10		; kernel data segment
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	mov eax, esp		; pointer to struct syscall_frame
	push eax
	call syscall_dispatch
	add esp, 4

	pop gs
	pop fs
	pop es
	pop ds
	popa
	iret

section .note.GNU-stack noalloc noexec nowrite
