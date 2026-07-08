/*
 * Copyright (c) 2018 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/status.h>
#include "idt.h"
#include "syscall.h"

#define SYSCALL_INTERRUPT 0x80

// Defined in user-mode.asm: restores the kernel context and never returns.
extern void back_to_kernel(void);

// A system call handler receives the saved user frame and returns a value
// that is written back into the user's eax.
typedef uint32_t (*syscall_t)(struct syscall_frame *frame);

static uint32_t
sys_exit(struct syscall_frame *frame)
{
	(void)frame;

	// Hand control back to the kernel; this does not return.
	back_to_kernel();

	return 0;
}

static uint32_t
sys_write(struct syscall_frame *frame)
{
	// Memory is identity-mapped, so the user pointer is directly usable.
	const char *message = (const char *)frame->ebx;

	kprintf("%s", message);

	return 0;
}

static syscall_t syscall_table[] = {
	[SYS_EXIT]  = sys_exit,
	[SYS_WRITE] = sys_write,
};

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_table[0]))

void
syscall_dispatch(struct syscall_frame *frame)
{
	uint32_t number = frame->eax;

	if (number < SYSCALL_COUNT && syscall_table[number] != NULL)
	{
		frame->eax = syscall_table[number](frame);
	}
	else
	{
		kprintf("Unknown syscall: %d\n", (int)number);
		frame->eax = (uint32_t)-1;
	}
}

void
system_calls_setup(void)
{
	extern void syscall_stub(void);

	// DPL 3 so ring 3 code is allowed to trigger the gate.
	status_t ret = x86_idt_set_handler(SYSCALL_INTERRUPT,
			(uint32_t)syscall_stub, 3);

	if (ret != KERNEL_OK)
	{
		kprintf("Syscall idt error\n");
	}
}
