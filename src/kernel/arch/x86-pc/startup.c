/*
 * Copyright (c) 2017-2026 Konstantin Tcholokachvili.
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
#include <arch/x86/gdt.h>
#include <arch/x86/per_cpu.h>
#include <arch/x86/idt.h>
#include <arch/x86/idt.h>
#include <arch/x86/syscall.h>
#include <arch/x86/isr.h>
#include <arch/x86/irq.h>
#include <arch/x86/pic.h>
#include <arch/x86/pit.h>
#include <arch/x86/ioapic.h>
#include <arch/x86/lapic.h>
#include <arch/x86/acpi.h>
#include <arch/x86/smp.h>
#include <arch/x86/tlb.h>
#include <memory/frame.h>
#include <memory/heap.h>
#include <arch/x86/paging.h>
#include <process/thread.h>
#include <process/process.h>
#include <process/scheduler.h>
#include <fs/tarfs.h>
#include <drivers/vbe.h>
#include <drivers/pci.h>
#include <drivers/rtl8139.h>
#include <drivers/ps2_keyboard.h>
#include <net/net.h>
#include <process/elf_loader.h>

#include <fs/fatfs_vfs.h>
#include <drivers/blockdev.h>
#include <drivers/ramdisk.h>
#include <lib/c/stdlib.h>

#define TIMER_INTERRUPT    0x20
#define SPURIOUS_INTERRUPT 0xff

#define MAX_BOOT_MODULES 8

struct superblock *root_fs;

struct boot_module
{
	char    name[16]; /* Flawfinder: ignore */
	paddr_t start, end;
};

static struct boot_module boot_modules[MAX_BOOT_MODULES];
static uint32_t boot_modules_count;

extern void pit_interrupt();
extern void spurious_interrupt_handler();
extern void tlb_shootdown_interrupt();
extern void lapic_timer_interrupt();
extern void resched_interrupt();

void
interrupts_setup(void)
{
	// Spurious interrupt
	x86_idt_set_handler(TIMER_INTERRUPT, (uint32_t)pit_interrupt, 0);
	x86_idt_set_handler(SPURIOUS_INTERRUPT, (uint32_t)spurious_interrupt_handler, 0);
	x86_idt_set_handler(TLB_SHOOTDOWN_VECTOR, (uint32_t)tlb_shootdown_interrupt, 0);
	x86_idt_set_handler(LAPIC_TIMER_VECTOR, (uint32_t)lapic_timer_interrupt, 0);
	x86_idt_set_handler(RESCHED_VECTOR, (uint32_t)resched_interrupt, 0);

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

static void
mount_fat_volumes(void)
{
	static const char *const mounts[][2] =
	{
		{ "fat16", "/mnt/fat16" },
		{ "fat32", "/mnt/fat32" },
		{ "exfat", "/mnt/exfat" },
	};
	status_t status = fatfs_init();

	assert(status == KERNEL_OK);

	status = vfs_create("/", "/mnt", VFS_NODE_DIRECTORY);
	assert(status == KERNEL_OK || status == -KERNEL_FILE_ALREADY_EXISTS);

	for (size_t i = 0; i < sizeof(mounts) / sizeof(mounts[0]); i++)
	{
		status = vfs_create("/", mounts[i][1], VFS_NODE_DIRECTORY);
		if (status == KERNEL_OK || status == -KERNEL_FILE_ALREADY_EXISTS)
			status = vfs_mount(mounts[i][0], mounts[i][1], "fatfs", NULL);

		if (status != KERNEL_OK)
			kprintf("mount %s on %s failed: %d\n",
					mounts[i][0], mounts[i][1], status);
	}
}

static void
test_ls(const char *path)
{
	struct dirent entry;
	struct vnode *vn;

	if (vfs_lookup("/", path, &vn) != KERNEL_OK)
	{
		kprintf("ls %s: not found\n", path);
		return;
	}

	kprintf("ls %s\n", path);
	for (uint32_t i = 0; vfs_readdir(vn, i, &entry) == 1; i++)
		kprintf("  %s%s (%d bytes)\n", entry.d_name,
			entry.d_type == VFS_NODE_DIRECTORY ? "/" : "",
			(int)entry.d_size);
	vnode_put(vn);
}

static void
test_cat(const char *path)
{
	char buf[64]; /* Flawfinder: ignore */
	struct vnode *vn;
	ssize_t n;

	if (vfs_lookup("/", path, &vn) != KERNEL_OK)
	{
		kprintf("cat %s: not found\n", path);
		return;
	}

	n = vfs_read(vn, buf, sizeof(buf) - 1, 0);
	buf[n > 0 ? n : 0] = '\0';
	kprintf("cat %s (size %d): %s\n", path, (int)vn->size, buf);
	vnode_put(vn);
}

static void
fatfs_vfs_test(void)
{
	static const char msg[] = "hello through the VFS";
	struct vnode *vn;

	test_ls("/mnt");
	test_ls("/mnt/fat16");
	test_cat("/mnt/fat16/hello.txt");
	test_cat("/mnt/fat16/A long file name.txt");
	test_cat("/mnt/fat32/hello.txt");

	kprintf("mkdir:  %d\n", vfs_create("/", "/mnt/exfat/dir", VFS_NODE_DIRECTORY));
	kprintf("touch:  %d\n", vfs_create("/mnt/exfat/dir", "note.txt", VFS_NODE_REGULAR_FILE));

	if (vfs_lookup("/", "/mnt/exfat/dir/note.txt", &vn) == KERNEL_OK)
	{
		kprintf("write:  %d\n", (int)vfs_write(vn, msg, sizeof(msg) - 1, 0));
		vnode_put(vn);
	}
	test_cat("/mnt/exfat/dir/note.txt");

	kprintf("rename: %d\n", vfs_rename("/", "/mnt/exfat/dir/note.txt",
				"/mnt/exfat/dir/renamed.txt"));
	kprintf("xmount: %d\n", vfs_rename("/", "/mnt/exfat/dir/renamed.txt",
				"/mnt/fat32/x.txt"));
	kprintf("rmdir non-empty: %d\n", vfs_remove("/", "/mnt/exfat/dir", VFS_NODE_DIRECTORY));
	test_ls("/mnt/exfat/dir/../dir");
	kprintf("rm:     %d\n", vfs_remove("/", "/mnt/exfat/dir/renamed.txt", VFS_NODE_REGULAR_FILE));
	kprintf("rmdir:  %d\n", vfs_remove("/", "/mnt/exfat/dir", VFS_NODE_DIRECTORY));
	kprintf("rmdir mountpoint: %d\n", vfs_remove("/", "/mnt/fat32", VFS_NODE_DIRECTORY));

	if (vfs_lookup("/", "/mnt/fat32/hello.txt", &vn) == KERNEL_OK)
	{
		kprintf("umount busy: %d\n", vfs_umount("/mnt/fat32"));
		vnode_put(vn);
	}
	kprintf("umount: %d\n", vfs_umount("/mnt/fat32"));
	kprintf("remount: %d\n", vfs_mount("fat32", "/mnt/fat32", "fatfs", NULL));
}

void
extra_kernel(uint32_t initrd_start, uint32_t initrd_end)
{
	// File system
	vfs_list_init();

	status_t status = tarfs_init(initrd_start, initrd_end);
	assert(status == KERNEL_OK);

	status = vfs_mount("initrd", "/", "tarfs", NULL);
	assert(status == KERNEL_OK);
	root_fs = tarfs_superblock();

	// Create one RAM disk per recorded module
	for (uint32_t i = 0; i < boot_modules_count; i++)
	{
		status = ramdisk_create(boot_modules[i].name,
				boot_modules[i].start, boot_modules[i].end);
		assert(status == KERNEL_OK);
	}

	mount_fat_volumes();
	fatfs_vfs_test();

	// PCI devices
	pci_scan();

	// RTL8139 network card
	rtl8139_setup();
	net_setup();

	// PS/2 keyboard
	keyboard_setup();

	vbe_set_color(NORMAL_GREEN);
}

static void
smp_init_thread(uint32_t arg)
{
	(void)arg;
	SmpInit();
}

// The kernel entry point. All starts from here!
void
aragveli_main(uint32_t magic, uint32_t address)
{
	status_t status;

	assert(magic == 0x2BADB002);

	// GDT
	gdt_setup_cpu(&g_cpus[0]);

	// IDT
	x86_idt_setup();

	// ISRs
	x86_isr_setup();

	// Paging
	x86_paging_setup();

	multiboot_info_t *mbi = (multiboot_info_t *)address;

	// Record every module
	multiboot_module_t *mods = (multiboot_module_t *)mbi->mods_addr;

	uint32_t initrd_start = mods[0].mod_start;
	uint32_t initrd_end   = mods[0].mod_end;
	paddr_t modules_end   = initrd_end;

	for (uint32_t i = 1; i < mbi->mods_count && i <= MAX_BOOT_MODULES; i++)
	{
		struct boot_module *bm = &boot_modules[boot_modules_count++];
		const char *cmdline = (const char *)mods[i].string;
		const char *last = strrchr(cmdline, ' ');

		strzcpy(bm->name, last ? last + 1 : cmdline, sizeof(bm->name));
		bm->start = mods[i].mod_start;
		bm->end   = mods[i].mod_end;

		if (bm->end > modules_end)
		{
			modules_end = bm->end;
		}
	}

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

	// Boot CPU identity, needed by TLB shootdown and the scheduler
	g_cpus[0].id      = 0;
	g_cpus[0].apic_id = LocalApicGetId();
	g_online_cpus     = 1u << 0;

	// Initrd
	initrd_start = *(uint32_t *)mbi->mods_addr;
	initrd_end   = *(uint32_t *)(mbi->mods_addr + 4);

	// Physical memory
	paddr_t identity_mapping_start, identity_mapping_end;
	size_t ram_size = (mbi->mem_upper << 10) + (1 << 20);
	status = frame_setup(ram_size,
			vbe_mode_info,
			&identity_mapping_start,
			&identity_mapping_end,
			modules_end);
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

	kprintf("Aragveli\n");

	// System calls
	system_calls_setup();

	// SMP
	thread_kernel_create("smp-init", smp_init_thread, NULL);

	uint32_t kernel_stack = frame_alloc();
	set_kernel_stack((uint32_t)PA2VA(kernel_stack) + PAGE_SIZE);

	extra_kernel(initrd_start, initrd_end);

	process_init();

	/* ELF loading (IRQs stay off until the first scheduled thread irets) */
	process_create_from_elf("/shell.elf", root_fs->root);
	scheduler_start();
}
