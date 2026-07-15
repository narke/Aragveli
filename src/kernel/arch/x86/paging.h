#pragma once

#include <lib/types.h>

#define PAGE_FAULT 14
#define KERNEL_VIRTUAL_BASE 0xC0000000u

#define PA2VA(pa) ((void *)((uint32_t)(pa) + KERNEL_VIRTUAL_BASE))
#define VA2PA(va) ((uint32_t)(va) - KERNEL_VIRTUAL_BASE)

void x86_paging_setup(void);
