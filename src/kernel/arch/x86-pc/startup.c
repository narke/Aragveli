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
#include <lib/c/stdio.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <arch/x86/isr.h>
#include <arch/x86/irq.h>
#include <arch/x86/pit.h>
#include <arch/x86/acpi.h>
#include <arch/x86/smp.h>
#include <memory/frame.h>
#include <memory/heap.h>
#include <process/thread.h>
#include <process/scheduler.h>
#include <fs/tarfs.h>
#include <lib/c/string.h>
#include <drivers/vbe.h>
#include <drivers/pci.h>
#include <drivers/rtl8139.h>
#include <test-suite/tarfs-test.h>


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

	// ACPI
	AcpiInit();

	// Initrd
	uint32_t initrd_start = *((uint32_t *)mbi->mods_addr);
	uint32_t initrd_end   = *(uint32_t *)(mbi->mods_addr + 4);

	// Physical memory
	paddr_t identity_mapping_start, identity_mapping_end;
	size_t ram_size = (mbi->mem_upper << 10) + (1 << 20);
	status = frame_setup(ram_size,
			vbe_mode_info,
			&identity_mapping_start,
			&identity_mapping_end,
			initrd_end);
	assert(status == KERNEL_OK);

	// Heap
	heap_setup(ram_size,
			identity_mapping_start,
			identity_mapping_end,
			framebuffer_start,
			framebuffer_end);

	// Scheduler
	scheduler_setup();

	// Theading
	threading_setup();

	// File system
	vfs_list_init();

	status = tarfs_init(initrd_start, initrd_end);
	assert(status == KERNEL_OK);

	struct superblock *root_fs;
	status = vfs_init("initrd", "tarfs", "/", NULL, &root_fs);
	assert(status == KERNEL_OK);

	printf("Aragveli\n");

	// Enable interrupts
	asm volatile("sti");

	// SMP
	SmpInit();

	tarfs_test(root_fs->root);

	// PCI devices
	pci_scan();
	pci_devices_print();

	// RTL8139 network card
	rtl8139_setup();
}
