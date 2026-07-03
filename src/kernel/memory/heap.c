/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/queue.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <lib/c/stdbool.h>
#include <memory/frame.h>
#include "heap.h"


#define VM_BASE 0x4000	// 16 Kb

uint32_t heap = 0;

SLIST_HEAD(, memory_range) free_ranges;
SLIST_HEAD(, memory_range) used_ranges;
SLIST_HEAD(, memory_range) metadata_pool;

static vaddr_t metadata_end;

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

	range = (struct memory_range *)heap;
	heap += sizeof(struct memory_range);
	assert(heap <= metadata_end);

	return range;
}

static void
range_metadata_free(struct memory_range *range)
{
	SLIST_INSERT_HEAD(&metadata_pool, range, next);
}

static void
coalesce_free_range(struct memory_range *range)
{
	struct memory_range *other;
	bool merged;

	do
	{
		merged = false;

		SLIST_FOREACH(other, &free_ranges, next)
		{
			if (other == range)
				continue;

			if (other->base_address + other->nb_pages * PAGE_SIZE
					== range->base_address)
			{
				other->nb_pages += range->nb_pages;
				SLIST_REMOVE(&free_ranges, range,
						memory_range, next);
				range_metadata_free(range);
				range = other;
				merged = true;
				break;
			}

			if (range->base_address + range->nb_pages * PAGE_SIZE
					== other->base_address)
			{
				range->nb_pages += other->nb_pages;
				SLIST_REMOVE(&free_ranges, other,
						memory_range, next);
				range_metadata_free(other);
				merged = true;
				break;
			}
		}
	} while (merged);
}

static void
create_range(bool is_free, vaddr_t start_address, vaddr_t end_address)
{
	assert(IS_PAGE_ALIGNED(start_address));
	assert(IS_PAGE_ALIGNED(end_address));

	if ((end_address - start_address) < PAGE_SIZE)
		return;

	struct memory_range *range = range_metadata_alloc();

	range->base_address = start_address;
	range->nb_pages = (end_address - start_address) / PAGE_SIZE;

	if (is_free)
		SLIST_INSERT_HEAD(&free_ranges, range, next);
	else
		SLIST_INSERT_HEAD(&used_ranges, range, next);
}

void
heap_setup(size_t ram_size,
		vaddr_t identity_mapping_start,
		vaddr_t identity_mapping_end,
		uint32_t framebuffer_start,
		uint32_t framebuffer_end)
{
	SLIST_INIT(&free_ranges);
	SLIST_INIT(&used_ranges);
	SLIST_INIT(&metadata_pool);

	heap = PAGE_ALIGN_UP(identity_mapping_end);
	metadata_end = PAGE_ALIGN_UP(identity_mapping_end) + PAGE_SIZE;

	// Free: 16 Kb - Framebuffer
	create_range(true,
			VM_BASE,
			PAGE_ALIGN_DOWN(framebuffer_start));

	// Used: Framebuffer zone
	create_range(false,
			PAGE_ALIGN_DOWN(framebuffer_start),
			PAGE_ALIGN_UP(framebuffer_end));

	// Free: Framebuffer end - Kernel start
	create_range(true,
			PAGE_ALIGN_UP(framebuffer_end),
			PAGE_ALIGN_DOWN(identity_mapping_start));

	// Used: Kernel
	create_range(false,
			PAGE_ALIGN_UP(identity_mapping_start),
			PAGE_ALIGN_DOWN(identity_mapping_end));

	// Used: Heap's allocation zone
	create_range(false,
			PAGE_ALIGN_UP(identity_mapping_end),
			PAGE_ALIGN_DOWN(identity_mapping_end + PAGE_SIZE));

	// Free: Heap's allocation zone - Kernel end
	create_range(true,
			PAGE_ALIGN_UP(identity_mapping_end + PAGE_SIZE),
			PAGE_ALIGN_DOWN(ram_size));
}

void *
heap_alloc(size_t size)
{
	struct memory_range *range;
	size_t nb_requested_pages;

	if (SLIST_EMPTY(&free_ranges))
		return NULL;

	nb_requested_pages = size == 0 ? 1 : (size + PAGE_SIZE - 1) / PAGE_SIZE;

	SLIST_FOREACH(range, &free_ranges, next)
	{
		if (range->nb_pages >= nb_requested_pages)
		{
			// Resize this range ?
			if (range->nb_pages > nb_requested_pages)
			{
				range->nb_pages -= nb_requested_pages;

				struct memory_range *new_range =
					range_metadata_alloc();

				new_range->base_address = range->base_address
					+ range->nb_pages * PAGE_SIZE;
				new_range->nb_pages = nb_requested_pages;

				SLIST_INSERT_HEAD(&used_ranges, new_range, next);
				return (void *)new_range->base_address;
			}
			else
			{
				SLIST_REMOVE(&free_ranges, range,
						memory_range, next);
				SLIST_INSERT_HEAD(&used_ranges, range, next);

				return (void *)range->base_address;
			}
		}
	}
	printf("alloc failed\n");

	return NULL;
}

void
heap_free(void *address)
{
	struct memory_range *range;

	if (!address)
		return;

	SLIST_FOREACH(range, &used_ranges, next)
	{
		if (range->base_address == (uint32_t)address)
		{
			SLIST_REMOVE(&used_ranges, range, memory_range, next);
			SLIST_INSERT_HEAD(&free_ranges, range, next);
			coalesce_free_range(range);
			return;
		}
	}
}
