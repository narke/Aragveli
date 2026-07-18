/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/string.h>
#include <lib/status.h>

#include "idt.h"
#include "irq.h"

#define X86_IRQ_NUM	16

/* array of IRQ wrappers, defined in irq_wrappers.S */
extern uint32_t x86_irq_wrapper_array[X86_IRQ_NUM];

/* arrays of IRQ handlers, shared with irq_wrappers.S */
x86_irq_handler_t x86_irq_handler_array[X86_IRQ_NUM] = { NULL, };

status_t
x86_irq_set_routine(uint32_t irq_level, x86_irq_handler_t routine)
{
	status_t ret;
	uint32_t flags;

	if (irq_level >= X86_IRQ_NUM)
		return -KERNEL_INVALID_VALUE;

	X86_IRQs_DISABLE(flags);

	/* Set the irq routine to be called by the IRQ wrapper */
	x86_irq_handler_array[irq_level] = routine;

	/* Wire (or clear) the IDT entry. PIC stays masked; IOAPIC enables IRQs. */
	if (routine != NULL)
	{
		ret = x86_idt_set_handler(X86_IRQ_BASE + irq_level,
				(uint32_t) x86_irq_wrapper_array[irq_level], 0);

		/* A problem occured */
		if (ret != KERNEL_OK)
			x86_irq_handler_array[irq_level] = NULL;
	}
	else
	{
		ret = x86_idt_set_handler(X86_IRQ_BASE + irq_level,
				(uint32_t)NULL /* Disable IDTE */, 0);
	}

	X86_IRQs_ENABLE(flags);

	return ret;
}


x86_irq_handler_t
irq_get_routine(uint32_t irq_level)
{
	if (irq_level >= X86_IRQ_NUM)
		return NULL;

	// Expected to be atomic
	return x86_irq_handler_array[irq_level];
}
