#include "paging.h"

#include <lib/c/stdint.h>

#define PAGE_PRESENT	0x1
#define PAGE_RW		0x2
#define PAGE_PRESENT_RW	(PAGE_PRESENT | PAGE_RW)

extern void x86_load_page_directory(uint32_t *);
extern void x86_enable_paging();

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t page_tables[1024][1024] __attribute__((aligned(4096)));


void
x86_paging_setup(void)
{
	// Init page directory and page directories by identity mapping
	for (uint32_t pde = 0; pde < 1024; pde++)
	{
		for (uint32_t pte = 0; pte < 1024; pte++)
		{
			uint32_t physical_addr = ((pde * 1024 + pte) * 0x1000);
			page_tables[pde][pte] = physical_addr | PAGE_PRESENT_RW;
		}
		page_directory[pde] = ((uint32_t)&page_tables[pde]) | PAGE_PRESENT_RW;
	}

	x86_load_page_directory(page_directory);
	x86_enable_paging();
}
