#include <lib/c/stdint.h>

#include "paging.h"


#define PAGE_PRESENT		0x1
#define PAGE_RW		0x2
#define PAGE_USER_ACCESSIBLE	(1 << 2)
#define PAGE_PS		0x80				// 4MB page
#define KERNEL_PDE		(KERNEL_VIRTUAL_BASE >> 22)	// 768 entries

#define NB_KERNEL_PDES 16	// 16 * 4MB = 64MB to map

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
