; Application processor startup code, copied to AP_TRAMPOLINE_BASE.

section .text

TRAMPOLINE_BASE equ 0x8000
%define T(x) (TRAMPOLINE_BASE + ((x) - ap_trampoline_start))

[extern ap_main]
[global ap_trampoline_start]
[global ap_trampoline_end]
[global tramp_cr3]
[global tramp_stacks]

bits 16
ap_trampoline_start:
	cli
	cld
	xor  ax, ax
	mov  ds, ax
	lgdt [T(tramp_gdtr)]
	mov  eax, cr0
	or   eax, 1
	mov  cr0, eax
	jmp  dword 0x08:T(ap_protected_mode)

bits 32
ap_protected_mode:
	mov  ax, 0x10
	mov  ds, ax
	mov  es, ax
	mov  fs, ax
	mov  gs, ax
	mov  ss, ax

	mov  eax, cr4
	or   eax, 0x00000010		; CR4.PSE (4 MiB pages)
	mov  cr4, eax
	mov  eax, [T(tramp_cr3)]
	mov  cr3, eax
	mov  eax, cr0
	or   eax, 0x80000000
	mov  cr0, eax

	mov  eax, 1
	lock xadd [T(tramp_next_cpu)], eax	; eax = this AP's g_cpus index
	mov  esp, [T(tramp_stacks) + eax * 4]

	push eax
	mov  ecx, ap_main		; higher-half address
	call ecx

.hang:
	hlt
	jmp  .hang

align 8
tramp_gdt:
	dq 0
	dq 0x00CF9A000000FFFF		; flat code, selector 0x08
	dq 0x00CF92000000FFFF		; flat data, selector 0x10
tramp_gdtr:
	dw 3 * 8 - 1
	dd T(tramp_gdt)

tramp_cr3:	dd 0
tramp_next_cpu:	dd 1
tramp_stacks:	times 16 dd 0		; MAX_CPU_COUNT

ap_trampoline_end:

section .note.GNU-stack noalloc noexec nowrite
