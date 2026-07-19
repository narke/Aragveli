section .text

global enter_user_mode

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

section .note.GNU-stack noalloc noexec nowrite
