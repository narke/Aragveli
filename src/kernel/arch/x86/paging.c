/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <memory/frame.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <lib/c/stdio.h>
#include <lib/types.h>
#include <lib/status.h>
#include "paging.h"

typedef struct page_directory
{
	uint32_t present:1;
	uint32_t rw:1;
	uint32_t mode:1;
	uint32_t write_through:1;
	uint32_t cache_disabled:1;
	uint32_t accessed:1;
	uint32_t reserved:1;
	uint32_t page_size:1;
	uint32_t ignored:1;
	uint32_t available:3;
	uint32_t page_table_address:20;
} __attribute__((packed)) page_directory_t;


typedef struct page_table
{
	uint32_t present:1;
	uint32_t rw:1;
	uint32_t mode:1;
	uint32_t write_through:1;
	uint32_t cache_disabled:1;
	uint32_t accessed:1;
	uint32_t dirty:1;
	uint32_t reserved:1;
	uint32_t global:1;
	uint32_t available:3;
	uint32_t frame_address:20;
} __attribute__((packed)) page_table_t;

// Get an index into page directory for a given address.
#define VA2PDE(va) (((unsigned)(va)) >> 22)

// Get an index into page table for a given address.
#define VA2PTE(va) ((((unsigned)(va)) >> 12) & 0x3ff)

#define VA2PAGE_OFFSET(va) (((unsigned)(vaddr)) & PAGE_MASK)

// Invalidate the TLB entry for the given page.
#define invlpg(vaddr) \
	do { asm volatile("invlpg %0"::"m"(*((unsigned *)(vaddr)))); } while (0)

static status_t
identity_map(page_directory_t *pd, paddr_t pa, vaddr_t va)
{
	page_table_t *pt;
	uint32_t pde = VA2PDE(va);
	uint32_t pte = VA2PTE(va);

	if (pd[pde].present)
	{
		pt = (page_table_t *)(pd[pde].page_table_address << 12);

		if (pt[pte].present)
			assert(false);
		else
			frame_ref_at((paddr_t)pt); // Increment reference count.
	}
	else
	{
		pt = (page_table_t *)frame_alloc();
		if (!pt)
			return -KERNEL_NO_MEMORY;

		memset(pt, 0, PAGE_SIZE);
		pd[pde].present	= true;
		pd[pde].rw	= 1;
		pd[pde].page_table_address = ((paddr_t)pt >> 12);
	}

	// Map the page into the page table.
	pt[pte].present	= true;
	pt[pte].rw	= 1;
	pt[pte].mode	= 0;
	pt[pte].frame_address = pa >> 12;

	return KERNEL_OK;
}

status_t
paging_setup(paddr_t identity_mapping_start,
		paddr_t identity_mapping_end,
		struct vbe_mode_info *vbe_mode_info)
{
	uint32_t cr0;
	paddr_t paddr;

	page_directory_t *pd = (page_directory_t *)frame_alloc();
	if (!pd)
		return -KERNEL_NO_MEMORY;

	memset(pd, 0, PAGE_SIZE);

	for (paddr = identity_mapping_start;
		paddr < identity_mapping_end;
		paddr += PAGE_SIZE)
	{
		if (identity_map(pd, paddr, paddr))
			return -KERNEL_NO_MEMORY;
	}

	for (paddr = vbe_mode_info->framebuffer_addr;
		paddr < (vbe_mode_info->framebuffer_addr
			+ vbe_mode_info->pitch * vbe_mode_info->y_res);
		paddr += PAGE_SIZE)
	{
		if (identity_map(pd, paddr, paddr))
			return -KERNEL_NO_MEMORY;
	}

	// Setup the mirroring.
	pd[VA2PDE(PAGING_MIRROR_VADDR)].present = true;
	pd[VA2PDE(PAGING_MIRROR_VADDR)].rw = 1;
	pd[VA2PDE(PAGING_MIRROR_VADDR)].mode = 0;
	pd[VA2PDE(PAGING_MIRROR_VADDR)].page_table_address = ((paddr_t)pd) >> 12;

	// Enable paging, the CR3 register must point to the page directory
	asm volatile("mov %0, %%cr3":: "b"(pd));
	asm volatile("mov %%cr0, %0": "=b"(cr0));	// read cr0
	cr0 |= 0x80000000;			// switch bit 31 (paging) on
	asm volatile("mov %0, %%cr0":: "b"(cr0));	// write back

	return KERNEL_OK;
}

status_t
paging_map(paddr_t paddr, vaddr_t vaddr, bool is_user_page, uint32_t flags)
{
	uint32_t pde = VA2PDE(vaddr);
	uint32_t pte = VA2PTE(vaddr);

	// Current context's page directory
	page_directory_t *pd = (page_directory_t *)
		(PAGING_MIRROR_VADDR + PAGE_SIZE * VA2PDE(PAGING_MIRROR_VADDR));

	//  Page table in the mirroring
	page_table_t *pt = (page_table_t *)
		(PAGING_MIRROR_VADDR + PAGE_SIZE * pde);

	// Don't map into mirroring
	if ((vaddr >= PAGING_MIRROR_VADDR)
		&& (vaddr < PAGING_MIRROR_VADDR + PAGING_MIRROR_SIZE))
		return -KERNEL_INVALID_VALUE;

	if (!pd[pde].present)
	{
		paddr_t pt_page = frame_alloc();
		if (!pt_page)
			return -KERNEL_NO_MEMORY;

		pd[pde].present = true;
		pd[pde].rw = 1;
		pd[pde].mode |= is_user_page? 1 : 0;
		pd[pde].page_table_address = ((paddr_t)pt_page) >> 12;

		invlpg(pt);
		memset(pt, 0, PAGE_SIZE);
	}
	else if (!pt[pte].present)
	{
		frame_ref_at(pd[pde].page_table_address << 12);
	}
	else
	{
		frame_free(pt[pte].frame_address << 12);
	}

	// Map the page into the page table
	pt[pte].present = true;
	pt[pte].rw = (flags & VM_MAP_PROT_WRITE)? 1 : 0;
	pt[pte].mode = is_user_page? 1 : 0;
	pt[pte].frame_address = paddr >> 12;
	frame_ref_at(paddr);

	invlpg(vaddr);

	return KERNEL_OK;
}

status_t
paging_unmap(vaddr_t vaddr)
{
	status_t status;

	unsigned pde = VA2PDE(vaddr);
	unsigned pte = VA2PTE(vaddr);

	// Current context's page directory
	page_directory_t *pd = (page_directory_t *)
		(PAGING_MIRROR_VADDR + PAGE_SIZE * VA2PDE(PAGING_MIRROR_VADDR));

	//  Page table in the mirroring
	page_table_t *pt = (page_table_t *)
		(PAGING_MIRROR_VADDR + PAGE_SIZE * pde);

	if (!pd[pde].present)
		return -KERNEL_INVALID_VALUE;

	if (!pt[pte].present)
		return -KERNEL_INVALID_VALUE;

	// Don't unmap in the mirroring
	if ((vaddr >= PAGING_MIRROR_VADDR)
		&& (vaddr < PAGING_MIRROR_VADDR + PAGING_MIRROR_SIZE))
		return -KERNEL_INVALID_VALUE;

	frame_free(pt[pte].frame_address << 12);

	memset(pt + pte, 0, sizeof(page_table_t));
	invlpg(vaddr);

	status = frame_free(pd[pde].page_table_address << 12);
	assert(status >= 0);

	if (status > 0)
	{
		memset(pd + pde, 0, sizeof(page_directory_t));
		invlpg(pt);
	}

	return KERNEL_OK;
}

paddr_t
paging_get_frame(vaddr_t vaddr)
{
	unsigned pde = VA2PDE(vaddr);
	unsigned pte = VA2PTE(vaddr);
	unsigned offset_in_page = VA2PAGE_OFFSET(vaddr);

	page_directory_t *pd = (page_directory_t *)
		(PAGING_MIRROR_VADDR + PAGE_SIZE * VA2PDE(PAGING_MIRROR_VADDR));

	page_table_t *pt = (page_table_t *)
		(PAGING_MIRROR_VADDR + PAGE_SIZE * pde);

	if (!pd[pde].present)
		return (paddr_t)NULL;
	if (pt[pte].present)
		return (paddr_t)NULL;

	return (pt[pte].frame_address << 12) + offset_in_page;

}

void page_fault(struct registers *r)
{
	uint32_t fault_address;
	(void)r;

	vbe_set_color(NORMAL_RED);

	asm("mov %%cr2, %0":"=r"(fault_address));
	printf("Page fault at %x\n", fault_address);

	while (1)
		asm("hlt");
}
