BITS 32
global _start
extern main

section .text
_start:
	mov esp, _stack_top
	call main
	push eax
	mov eax, 0	; SYS_EXIT
	int 0x80

section .bss
align 4096
stack:
	resb 4096
global _stack_top
_stack_top:

section .note.GNU-stack noalloc noexec nowrite
