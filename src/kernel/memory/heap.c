/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/queue.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <lib/c/stdio.h>
#include <memory/frame.h>
#include <arch/x86/paging.h>
#include "heap.h"

static uint32_t metadata_heap;
static vaddr_t metadata_end;

SLIST_HEAD(, memory_range) used_ranges;
SLIST_HEAD(, memory_range) metadata_pool;

struct memory_range
{
	uint32_t base_address;
	uint32_t nb_pages;
	SLIST_ENTRY(memory_range) next;
};

static struct memory_range *
range_metadata_alloc(void)
{
	struct memory_range *range;

	if (!SLIST_EMPTY(&metadata_pool))
	{
		range = SLIST_FIRST(&metadata_pool);
		SLIST_REMOVE_HEAD(&metadata_pool, next);
		return range;
	}

	range = (struct memory_range *)metadata_heap;
	metadata_heap += sizeof(struct memory_range);
	assert(metadata_heap <= metadata_end);

	return range;
}

static void
range_metadata_free(struct memory_range *range)
{
	SLIST_INSERT_HEAD(&metadata_pool, range, next);
}

void
heap_setup(size_t ram_size,
		vaddr_t identity_mapping_start,
		vaddr_t identity_mapping_end,
		uint32_t framebuffer_start,
		uint32_t framebuffer_end)
{
	(void)ram_size;
	(void)identity_mapping_start;
	(void)framebuffer_start;
	(void)framebuffer_end;

	SLIST_INIT(&used_ranges);
	SLIST_INIT(&metadata_pool);

	/* Last page of the kernel-reserved identity region (see frame_setup). */
	metadata_end = PAGE_ALIGN_UP(identity_mapping_end);
	metadata_heap = metadata_end - PAGE_SIZE;
}

void *
heap_alloc(size_t size)
{
	size_t nb_pages = size == 0 ? 1 : (size + PAGE_SIZE - 1) / PAGE_SIZE;
	paddr_t base = frame_alloc_contiguous(nb_pages);
	struct memory_range *range;

	if (!base)
	{
		kprintf("alloc failed!\n");
		return NULL;
	}

	range = range_metadata_alloc();
	range->base_address = base;
	range->nb_pages = nb_pages;
	SLIST_INSERT_HEAD(&used_ranges, range, next);

	return PA2VA(base);
}

void
heap_free(void *address)
{
	struct memory_range *range;

	if (!address)
		return;

	uint32_t physical_address = VA2PA(address);

	SLIST_FOREACH(range, &used_ranges, next)
	{
		if (range->base_address == physical_address)
		{
			frame_free_contiguous(range->base_address, range->nb_pages);
			SLIST_REMOVE(&used_ranges, range, memory_range, next);
			range_metadata_free(range);
			return;
		}
	}

	kprintf("heap_free: invalid pointer %p\n", address);
	assert(0);
}
