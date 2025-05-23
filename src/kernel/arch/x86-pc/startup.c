/*
 * Copyright (c) 2017, 2020 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/types.h>
#include <lib/status.h>
#include <lib/c/assert.h>
#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <arch/x86-pc/bootstrap/multiboot.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <arch/x86/isr.h>
#include <arch/x86/irq.h>
#include <arch/x86/pic.h>
#include <arch/x86/pit.h>
#include <arch/x86/ioapic.h>
#include <arch/x86/lapic.h>
#include <arch/x86/acpi.h>
#include <arch/x86/smp.h>
#include <memory/frame.h>
#include <memory/heap.h>
#include <arch/x86/paging.h>
#include <process/thread.h>
#include <process/scheduler.h>
#include <fs/tarfs.h>
#include <drivers/vbe.h>
#include <drivers/pci.h>
#include <drivers/rtl8139.h>
#include <test-suite/tarfs-test.h>

#define TIMER_INTERRUPT    0x20
#define SPURIOUS_INTERRUPT 0xff

extern void pit_interrupt();
extern void spurious_interrupt_handler();

void
interrupts_setup(void)
{
	// Spurious interrupt
	x86_idt_set_handler(TIMER_INTERRUPT, (uint32_t)pit_interrupt);
	x86_idt_set_handler(SPURIOUS_INTERRUPT, (uint32_t)spurious_interrupt_handler);

	// Disable PIC to use Local APIC
	x86_pic_disable();

	// ACPI
	AcpiInit();
	LocalApicInit();
	IoApicInit();
	// Timer: Raise IRQ0 at 100 HZ rate.
	status_t status = x86_pit_set_frequency(100);
	assert(status == KERNEL_OK);

	// Enable IO APIC entries
	IoApicSetEntry(g_ioApicAddr, AcpiRemapIrq(IRQ_TIMER), TIMER_INTERRUPT);
}

void
extra_kernel(uint32_t initrd_start, uint32_t initrd_end)
{
	// File system
	vfs_list_init();

	status_t status = tarfs_init(initrd_start, initrd_end);
	assert(status == KERNEL_OK);

	struct superblock *root_fs;
	status = vfs_init("initrd", "tarfs", "/", NULL, &root_fs);
	assert(status == KERNEL_OK);

	tarfs_test(root_fs->root);

	// PCI devices
	pci_scan();
	pci_devices_print();

	// RTL8139 network card
	rtl8139_setup();
}


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

	// VBE
	struct vbe_mode_info *vbe_mode_info =
		(struct vbe_mode_info *)mbi->vbe_mode_info;
	uint32_t framebuffer_start = vbe_mode_info->framebuffer_addr;
	uint32_t framebuffer_end = vbe_mode_info->framebuffer_addr
			+ vbe_mode_info->pitch
			* vbe_mode_info->y_res;

	vbe_setup(vbe_mode_info);

	// Interrupts
	interrupts_setup();

	// Initrd
	uint32_t initrd_start = *(uint32_t *)mbi->mods_addr;
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

	// Paging
	x86_paging_setup();

	// Scheduler
	scheduler_setup();

	// Theading
	threading_setup();

	printf("Aragveli\n");

	// Enable interrupts
	asm volatile("sti");

	// SMP
	//SmpInit();

	extra_kernel(initrd_start, initrd_end);
}
