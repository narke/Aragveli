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
#include <arch/x86/paging.h>
#include <memory/heap.h>
#include <process/thread.h>
#include <process/scheduler.h>


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
	uint32_t framebuffer_start = vbe_mode_info->framebuffer_addr;
	uint32_t framebuffer_end = vbe_mode_info->framebuffer_addr
			+ vbe_mode_info->pitch
			* vbe_mode_info->y_res;

	vbe_setup(vbe_mode_info);

	// Physical memory
	paddr_t identity_mapping_start, identity_mapping_end;
	status = physical_memory_setup((mbi->mem_upper << 10) + (1 << 20),
			vbe_mode_info,
			&identity_mapping_start,
			&identity_mapping_end);
	assert(status == KERNEL_OK);

	// Paging
	status = paging_setup(identity_mapping_start,
			identity_mapping_end,
			vbe_mode_info);
	assert(status == KERNEL_OK);

	// Handle page faults
	isr_set_handler(14, page_fault);

	// Heap
	heap_setup(identity_mapping_start,
			identity_mapping_end,
			framebuffer_start,
			framebuffer_end);

	// Scheduler
	scheduler_setup();

	// Theading
	threading_setup();

	printf("Aragveli");

	// Enable interrupts
	asm volatile("sti");
}
