/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/string.h>
#include <arch/x86/segment.h>
#include <lib/c/stdio.h>

#include "cpu-context.h"

struct cpu_state
{
	uint16_t gs;
	uint16_t fs;
	uint16_t es;
	uint16_t ds;
	uint16_t ss;
	uint16_t padding; // Unused
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t ebp;

	/* The followig declaratins must be kept untouched since this order is
		required by the iret instruction. */
	uint32_t error_code;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
}__attribute__((packed));


/* Describes an interrupted kernel thread's context */
struct cpu_kstate //thread_cpu_context
{
	struct cpu_state regs;
}__attribute__((packed));


static void core_routine(cpu_kstate_function_arg1_t *start_func,
	uint32_t start_arg,
	cpu_kstate_function_arg1_t *exit_func,
	uint32_t exit_arg) __attribute__((noreturn));


static void
core_routine(cpu_kstate_function_arg1_t *start_func,
		uint32_t start_arg,
		cpu_kstate_function_arg1_t *exit_func,
		uint32_t exit_arg)
{
	start_func(start_arg);
	exit_func(exit_arg);

	for(;;);
}


void
cpu_kstate_init(struct cpu_state **ctx,
		cpu_kstate_function_arg1_t *start_func,
		uint32_t start_arg,
		uint32_t stack_base,
		uint32_t stack_size,
		cpu_kstate_function_arg1_t *exit_func,
		uint32_t exit_arg)
{
	/* We are initializing a Kernel thread's context */
	struct cpu_kstate *kctx;

	uint32_t tmp_vaddr = stack_base + stack_size;
	uint32_t *stack = (uint32_t *)tmp_vaddr;

	/* Simulate a call to the core_routine() function: prepare its arguments */
	*(--stack) = exit_arg;
	*(--stack) = (uint32_t)exit_func;
	*(--stack) = start_arg;
	*(--stack) = (uint32_t)start_func;
	*(--stack) = 0; /* Return address of core_routine => force page fault */

	/*
	 * Setup the initial context structure, so that the CPU will execute
	 * the function core_routine() once this new context has beencrestored on CPU
	 */

	/* Compute the base address of the structure, which must be located
	 * below the previous elements */
	tmp_vaddr = ((uint32_t)stack) - sizeof(struct cpu_kstate);
	kctx = (struct cpu_kstate *)tmp_vaddr;

	/* Initialize the CPU context structure */
	kmemset(kctx, 0x0, sizeof(struct cpu_kstate));

	/* Tell the CPU context structure that the first instruction
	 * to execute will be that of the core_routine() function */
	kctx->regs.eip = (uint32_t)core_routine;

	/* Setup the segment registers */
	kctx->regs.cs =
		X86_BUILD_SEGMENT_REGISTER_VALUE(KERNEL_CODE_SEGMENT); /* Code */
	kctx->regs.ds =
		X86_BUILD_SEGMENT_REGISTER_VALUE(KERNEL_DATA_SEGMENT); /* Data */
	kctx->regs.es =
		X86_BUILD_SEGMENT_REGISTER_VALUE(KERNEL_DATA_SEGMENT); /* Data */
	kctx->regs.ss =
		X86_BUILD_SEGMENT_REGISTER_VALUE(KERNEL_DATA_SEGMENT); /* Stack */

	/* fs and gs unused for the moment. */

	/* The newly created context is initially interruptible */
	kctx->regs.eflags = (1 << 9); /* set IF bit */

	/* Finally, update the generic kernel thread context */
	*ctx = (struct cpu_state *)kctx;
}

