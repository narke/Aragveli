#pragma once

#include <lib/types.h>
#include <arch/x86/paging.h>
#include <arch/x86-pc/vbe.h>

#define VM_BASE 0x4000			// 16 Kb
#define VM_TOP  PAGING_MIRROR_VADDR	// 1GB - 4MB

void heap_setup(vaddr_t identity_mapping_start,
		vaddr_t identity_mapping_end,
		uint32_t framebuffer_start,
		uint32_t framebuffer_end);

void *heap_alloc(size_t size);
void heap_free(void *ptr);

void map_pages(vaddr_t base_address, vaddr_t top_address, bool is_user);
void unmap_pages(vaddr_t base_address, vaddr_t top_address);
