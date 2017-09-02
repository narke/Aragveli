/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86-pc/bootstrap/multiboot.h>
#include <lib/types.h>
#include <lib/status.h>
#include <lib/c/assert.h>
#include <lib/c/printf.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <arch/x86/isr.h>
#include <arch/x86/irq.h>
#include <arch/x86/pit.h>
#include <arch/x86-pc/vbe.h>
#include <memory/physical-memory.h>

// The kernel entry point. All starts from here!
void
aragveli_main(uint32_t magic, uint32_t address)
{
	status_t status;
	multiboot_info_t *mbi = (multiboot_info_t *)address;

	assert(magic == 0x2BADB002);

	// GDT
	x86_gdt_setup();

	// IDT
	x86_idt_setup();

	// ISRs
	x86_isr_setup();

	// IRQs
	x86_irq_setup();

	// Timer: Raise IRQ0 at 100 HZ rate.
	status = x86_pit_set_frequency(100);
	assert(status == KERNEL_OK);

	// Timer interrupt
	x86_irq_set_routine(IRQ_TIMER, timer_interrupt_handler);

	// VBE
	struct vbe_mode_info *vbe_mode_info =
		(struct vbe_mode_info *)mbi->vbe_mode_info;

	vbe_setup(vbe_mode_info);

	// Physical memory
	status = physical_memory_setup((mbi->mem_upper << 10) + (1 << 20));
	assert(status == KERNEL_OK);

	printf("Aragveli");

	// Enable interrupts
	asm volatile("sti");
}
