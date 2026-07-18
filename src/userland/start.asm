BITS 32
global _start
extern main

section .text
_start:
	; System V i386: [esp]=argc, [esp+4]=argv
	mov eax, [esp]		; argc
	lea ecx, [esp + 4]	; argv
	push ecx
	push eax
	call main
	add esp, 8
	push eax
	pop ebx			; exit status
	mov eax, 0		; SYS_EXIT
	int 0x80

section .note.GNU-stack noalloc noexec nowrite
