#pragma once

#include <lib/types.h>

#define PAGE_FAULT 14
#define KERNEL_VIRTUAL_BASE 0xC0000000u

#define PA2VA(pa) ((void *)((uint32_t)(pa) + KERNEL_VIRTUAL_BASE))
#define VA2PA(va) ((uint32_t)(va) - KERNEL_VIRTUAL_BASE)

#define PAGE_PRESENT	0x1
#define PAGE_RW		0x2
#define PAGE_USER	(1 << 2)			// U/S bit
#define PAGE_PS		0x80				// 4MB page

void x86_paging_setup(void);
uint32_t page_directory_create(void);
status_t page_map_user(uint32_t pd_phys, uint32_t vaddr, uint32_t paddr, uint32_t flags);
void page_directory_switch(uint32_t pd_phys);
void page_directory_clear_user(uint32_t pd_phys);
void page_directory_destroy(uint32_t pd_phys);
uint32_t page_lookup(uint32_t pd_phys, uint32_t vaddr);
uint32_t page_directory_kernel(void);
