#include <lib/c/stdint.h>
#include <lib/c/string.h>
#include <memory/frame.h>

#include "paging.h"

// Mask that clears the low 12 flag bits, keeping the page frame address
#define PAGE_FRAME_MASK		(~(uint32_t)PAGE_MASK)

// Page-directory and page-table indices of a virtual address
#define PDE_INDEX(vaddr)	(((uint32_t)(vaddr) >> 22) & 0x3FFu)
#define PTE_INDEX(vaddr)	(((uint32_t)(vaddr) >> 12) & 0x3FFu)

#define KERNEL_PDE		(KERNEL_VIRTUAL_BASE >> 22)	// 768 entries
#define KERNEL_PDE_START	768	//PDEs 768..1023 are the shared kernel half
#define NB_KERNEL_PDES		64	// 64 * 4MB = 256MB to map

extern void x86_load_page_directory(uint32_t *);
extern void x86_enable_paging();

uint32_t page_directory[1024] __attribute__((aligned(4096)));


void
x86_paging_setup(void)
{
	uint32_t flags = PAGE_PRESENT | PAGE_RW | PAGE_PS;

	// Identity mapping of 4GB with 4MB pages
	for (uint32_t pde = 0; pde < 1024; pde++)
	{
		page_directory[pde] = (pde << 22) | flags;
	}

	/*
	 * Higher-half 0xC000000 + X translated to physical + X
	 * so the kernel keeps running at its link addresses after we switch
	 * page directory with cr3
	 */
	for (uint32_t i = 0; i < NB_KERNEL_PDES; i++)
	{
		page_directory[KERNEL_PDE + i] = (i << 22) | flags;
	}

	// CR3 needs to know the physical address of a page directory
	x86_load_page_directory((uint32_t *)VA2PA(page_directory));
}

uint32_t
page_directory_create(void)
{
	// Physical address of a page directory for CR3
	uint32_t pd_physical = frame_alloc();

	if (!pd_physical)
	{
		return 0;
	}

	uint32_t *pd = (uint32_t *)PA2VA(pd_physical);

	for (int i = 0; i < 1024; i++)
	{
		pd[i] = 0;
	}

	// Share the kernel higher half
	for (int i = KERNEL_PDE_START; i < 1024; i++)
	{
		pd[i] = page_directory[i];
	}

	return pd_physical;
}

uint32_t
page_directory_kernel(void)
{
	return VA2PA((uint32_t)page_directory);
}

uint32_t
page_directory_clone(uint32_t src_pd)
{
	uint32_t dst_pd;
	uint32_t *src;
	uint32_t i;
	uint32_t j;

	if (!src_pd)
		return 0;

	dst_pd = page_directory_create();
	if (!dst_pd)
		return 0;

	src = (uint32_t *)PA2VA(src_pd);

	for (i = 0; i < KERNEL_PDE_START; i++)
	{
		uint32_t *pt;

		if (!(src[i] & PAGE_PRESENT))
			continue;

		/* User mappings are 4KiB pages, not PSE. */
		if (src[i] & PAGE_PS)
			goto fail;

		pt = (uint32_t *)PA2VA(src[i] & PAGE_FRAME_MASK);

		for (j = 0; j < 1024; j++)
		{
			uint32_t pte = pt[j];
			uint32_t src_frame;
			uint32_t dst_frame;
			uint32_t vaddr;
			uint32_t flags;

			if (!(pte & PAGE_PRESENT))
				continue;

			src_frame = pte & PAGE_FRAME_MASK;
			dst_frame = frame_alloc();
			if (!dst_frame)
				goto fail;

			if (memcpy_s(PA2VA(dst_frame), PAGE_SIZE,
					PA2VA(src_frame), PAGE_SIZE) == NULL)
			{
				frame_free(dst_frame);
				goto fail;
			}

			vaddr = (i << 22) | (j << 12);
			flags = (pte & PAGE_RW) ? PAGE_RW : 0;

			if (page_map_user(dst_pd, vaddr, dst_frame, flags) != KERNEL_OK)
			{
				frame_free(dst_frame);
				goto fail;
			}
		}
	}

	return dst_pd;

fail:
	page_directory_destroy(dst_pd);
	return 0;
}

status_t
page_map_user(uint32_t pd_physical, uint32_t vaddr, uint32_t paddr, uint32_t flags)
{
	uint32_t pde_i = PDE_INDEX(vaddr);
	uint32_t pte_i = PTE_INDEX(vaddr);

	/* Never let user mappings touch the kernel half. */
	if (pde_i >= KERNEL_PDE_START)
	{
		return -KERNEL_INVALID_VALUE;
	}

	uint32_t *pd = (uint32_t *)PA2VA(pd_physical);
	uint32_t *pt;

	if (!(pd[pde_i] & PAGE_PRESENT))
	{
		uint32_t pt_physical = frame_alloc();

		if (!pt_physical)
		{
			return -KERNEL_NO_MEMORY;
		}

		pt = (uint32_t *)PA2VA(pt_physical);
		memset(pt, 0, PAGE_SIZE);
		pd[pde_i] = pt_physical | PAGE_PRESENT | PAGE_RW | PAGE_USER;
	}
	else
	{
		pt = (uint32_t *)PA2VA(pd[pde_i] & PAGE_FRAME_MASK);
	}

	pt[pte_i] = (paddr & PAGE_FRAME_MASK) | (flags & PAGE_MASK) | PAGE_PRESENT | PAGE_USER;

	return KERNEL_OK;
}

void
page_directory_switch(uint32_t pd_physical)
{
	x86_load_page_directory((uint32_t *)pd_physical);   /* cr3 = physical */
}

void
page_directory_clear_user(uint32_t pd_physical)
{
	if (!pd_physical)
	{
		return;
	}

	uint32_t *pd = (uint32_t *)PA2VA(pd_physical);

	/* Only the user half is private; the kernel half is shared, never freed. */
	for (uint32_t i = 0; i < KERNEL_PDE_START; i++)
	{
		if (!(pd[i] & PAGE_PRESENT))
		{
			continue;
		}

		uint32_t pt_physical = pd[i] & PAGE_FRAME_MASK;
		uint32_t *pt = (uint32_t *)PA2VA(pt_physical);

		for (uint32_t j = 0; j < 1024; j++)
		{
			if (pt[j] & PAGE_PRESENT)
			{
				frame_free(pt[j] & PAGE_FRAME_MASK);
			}
		}

		frame_free(pt_physical);
		pd[i] = 0;
	}
}

void
page_directory_destroy(uint32_t pd_physical)
{
	if (!pd_physical)
	{
		return;
	}

	page_directory_clear_user(pd_physical);
	frame_free(pd_physical);
}

uint32_t
page_lookup(uint32_t pd_physical, uint32_t vaddr)
{
	uint32_t pde_i = PDE_INDEX(vaddr);
	uint32_t pte_i = PTE_INDEX(vaddr);

	uint32_t *pd = (uint32_t *)PA2VA(pd_physical);

	if (!(pd[pde_i] & PAGE_PRESENT))
	{
		return 0;                       /* not mapped */
	}

	if (pd[pde_i] & PAGE_PS)
	{
		return (pd[pde_i] & PAGE_FRAME_MASK) + (vaddr & 0x3FFFFF);
	}

	uint32_t *pt = (uint32_t *)PA2VA(pd[pde_i] & PAGE_FRAME_MASK);

	if (!(pt[pte_i] & PAGE_PRESENT))
	{
		return 0;
	}

	return pt[pte_i] & PAGE_FRAME_MASK; /* page-aligned physical frame */
}
