; Copyright (c) 2017 Konstantin Tcholokachvili.
; All rights reserved.
; Use of this source code is governed by a MIT license that can be
; found in the LICENSE file.

; See http://wiki.osdev.org/Bare_Bones, slightly modified by me.

extern aragveli_main	; this is our kernel's entry point

; Setting up the Multiboot header - see GRUB docs for details
MBALIGN		equ 1<<0		; Align loaded modules on page
					; boundaries.
MEMINFO		equ 1<<1		; Provide memory map.
GRAPHICS	equ 1<<2		; Provide VBE info.
FLAGS		equ MBALIGN | MEMINFO | GRAPHICS	; Multiboot 'flag' field.
MAGIC		equ 0x1BADB002		; 'Magic number' lets bootloader find
					; the header.
CHECKSUM	equ -(MAGIC + FLAGS)	; Checksum required to prove that we
					; are multiboot.
STACK_SIZE	equ 0x4000		; Stack size is 16KiB.
VBE_MODE	equ 0
VBE_WIDTH	equ 1024
VBE_HEIGHT	equ 768
VBE_DEPTH	equ 32

KERNEL_VIRTUAL_BASE	equ 0xC0000000
KERNEL_PDE		equ (KERNEL_VIRTUAL_BASE >> 22)	; 768 entries(3GB)


; The multiboot header must come first.
section .multiboot

; Multiboot header must be aligned on a 4-byte boundary
align 4

multiboot_header:
	dd MAGIC
	dd FLAGS
	dd CHECKSUM
	dd 0
	dd 0
	dd 0
	dd 0
	dd 0
	dd VBE_MODE
	dd VBE_WIDTH
	dd VBE_HEIGHT
	dd VBE_DEPTH


; Low / identity-mapped boot section (runs with paging OFF)
section .boot
align 4096
global boot_page_directory
boot_page_directory:
	dd 0x00000083				; PDE 0:   identity map 0..4MiB (PS|RW|P)
	times (KERNEL_PDE - 1) dd 0
	dd 0x00000083				; PDE 768: 0xC0000000 -> phys 0 (4 MiB)
	times (1024 - KERNEL_PDE - 1) dd 0

global multiboot_entry
multiboot_entry:
	mov esi, eax				; multiboot magic
	mov edi, ebx				; multiboot info

	mov ecx, boot_page_directory		; physical address (linked low in .boot)
	mov cr3, ecx

	mov ecx, cr4
	or  ecx, 0x00000010			; CR4.PSE (4 MiB pages)
	mov cr4, ecx

	mov ecx, cr0
	or  ecx, 0x80000000
	mov cr0, ecx

	lea ecx, [higher_half]
	jmp ecx

; High / virtual code
section .text
higher_half:
	mov esp, stack + STACK_SIZE	; set up the stack
	push edi			; multiboot data structure
	push esi			; multiboot magic number

	call aragveli_main		; calling the kernel

hang:
	hlt				; something bad happened, machine halted
	jmp hang


section .bss nobits align=16
; Reserve initial kernel stack space
stack:          resb STACK_SIZE ; reserve 16 KiB stack

section .note.GNU-stack noalloc noexec nowrite
