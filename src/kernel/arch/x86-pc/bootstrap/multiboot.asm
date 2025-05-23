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

; The beginning of our kernel code
section .text

global multiboot_entry
multiboot_entry:
	mov esp, stack + STACK_SIZE	; set up the stack
	mov [magic], ebx		; multiboot magic number
	mov [multiboot_info], eax	; multiboot data structure

	call aragveli_main		; calling the kernel

hang:
	hlt				; something bad happened, machine halted
	jmp hang


section .bss nobits align=4
; Reserve initial kernel stack space
stack:          resb STACK_SIZE ; reserve 16 KiB stack
multiboot_info: resd 1          ; we will use this in kernel's main
magic:          resd 1          ; we will use this in kernel's main

section .note.GNU-stack noalloc noexec nowrite noread
