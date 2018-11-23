/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 *
 * Handling interruptions descriptor table.
 */

#include <lib/c/string.h>
#include <arch/x86/segment.h>
#include <lib/status.h>
#include "idt.h"

// Maximum number of interruptions which can be handled.
#define INTERRUPTS_MAX_LIMIT 256

/**
 * The IDT register stores the address and size of the IDT.
 *
 * @see Intel x86 doc vol 3, section 2.4, figure 2-4
 */
typedef struct x86_idtr
{
	uint16_t limit;
	uint32_t base_address;
} __attribute__((packed, aligned(8))) x86_idtr_t;

/**
 * An entry in the IDT is a reference to a interrupt/trap routine or
 * a task gate to handle the software/hardware interrupts and
 * exceptions.
 *
 * @see figure 5-2, intel x86 doc, vol 3
 */
typedef struct x86_idt_entry
{
	/*** Low dword ***/

	// 0..15, offset of the routine in the segment.
	uint16_t offset_low;
	// 16..31, the ID of the segment
	uint16_t segment_selector;

	/*** High dword ***/

	// 32..39
	uint8_t unused:8;
	// 40..43 (interrupt gate, trap gate...)
	uint8_t gate_type:4;
	// 44
	uint8_t storage_segment:1;
	// 45,46
	uint8_t descriptor_privilege_level:2;
	// 47
	uint8_t present:1;
	// 48..63
	uint16_t offset_high;
} __attribute__((packed)) x86_idt_entry_t;

static x86_idt_entry_t idt_array[INTERRUPTS_MAX_LIMIT];

void
x86_idt_setup(void)
{
	x86_idtr_t idtr;

	for (uint16_t i = 0; i < INTERRUPTS_MAX_LIMIT; i++)
	{
		x86_idt_entry_t *idt_entry = idt_array+i;

		idt_entry->segment_selector 	= 0x08;
		idt_entry->unused		= 0;

		// = 0xE for i386 32-bit interrupt gate.
		idt_entry->gate_type		= 0xE;

		// = 0 for interrupt gates.
		idt_entry->storage_segment	= 0;

		// This IDT entry is disabled by default.
		x86_idt_set_handler(i, (uint32_t)NULL);
	}

	idtr.base_address = (uint32_t)idt_array;
	idtr.limit        = sizeof(idt_array) - 1;

	asm volatile ("lidt %0\n"::"m"(idtr):"memory");
}


status_t
x86_idt_set_handler(uint32_t index, uint32_t handler_address)
{
	if (index >= INTERRUPTS_MAX_LIMIT)
		return -KERNEL_INVALID_VALUE;

	x86_idt_entry_t *idt_entry = idt_array + index;

	if (handler_address != (uint32_t)NULL)
	{
		// Enabling IDT entry.
		idt_entry->offset_low  = handler_address & 0xffff;
		idt_entry->offset_high = (handler_address >> 16) & 0xffff;
		idt_entry->descriptor_privilege_level = 0;
		idt_entry->present     = 1;	// Yes, there is a handler.
	}
	else
	{
		// Disabling IDT entry.
		idt_entry->offset_low  = 0;
		idt_entry->offset_high = 0;
		idt_entry->descriptor_privilege_level = 0;
		idt_entry->present     = 0;	// No, there is no handler.
	}

	return KERNEL_OK;
}
