section .text

global jump_to_user_mode
global back_to_kernel
global enter_user_mode

; void jump_to_user_mode(uint32_t entry, uint32_t user_stack_top);
;
; Enters ring 3 at 'entry' using 'user_stack_top' as the user stack.
; The kernel context is saved so that the exit syscall can resume the
; caller (aragveli_main) through back_to_kernel.
jump_to_user_mode:
	; Save callee-saved registers so back_to_kernel can restore them
	push ebp
	push ebx
	push esi
	push edi

	; Save the kernel stack pointer for the exit path
	mov [back_to_kernel_esp], esp

	mov eax, [esp + 20]	; entry point (4 saved regs + return addr)
	mov ecx, [esp + 24]	; user stack top

	mov dx, (4 * 8) | 3	; ring 3 data selector
	mov ds, dx
	mov es, dx
	mov fs, dx
	mov gs, dx		; SS is handled by iret

	; Build the stack frame iret expects
	push (4 * 8) | 3	; SS: user data selector
	push ecx		; user ESP
	pushf			; EFLAGS (IF is set: kernel did sti)
	push (3 * 8) | 3	; CS: ring 3 code selector
	push eax		; EIP: user function entry
	iret

; void back_to_kernel(void);
;
; Restores the kernel context saved by jump_to_user_mode and returns into
; its caller. Invoked from the exit syscall; it never returns to the stub.
back_to_kernel:
	mov esp, [back_to_kernel_esp]
	pop edi
	pop esi
	pop ebx
	pop ebp
	sti
	ret

; void enter_user_mode(uint32_t entry, uint32_t user_stack_top);
enter_user_mode:
	mov eax, [esp + 4]      ; entry point
	mov ecx, [esp + 8]      ; user stack top

	mov dx, (4 * 8) | 3	; ring 3 data selector
	mov ds, dx
	mov es, dx
	mov fs, dx
	mov gs, dx

	; Build the stack frame iret expects
	push (4 * 8) | 3        ; SS: user data selector
	push ecx		; user ESP
	pushf			; EFLAGS (IF is set: kernel did sti)
	push (3 * 8) | 3	; CS: ring 3 code selector
	push eax		; EIP: user function entry
	iret

section .data
back_to_kernel_esp: dd 0

section .note.GNU-stack noalloc noexec nowrite
