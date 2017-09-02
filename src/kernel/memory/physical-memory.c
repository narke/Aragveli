/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86-pc/bootstrap/multiboot.h>
#include <lib/queue.h>
#include <lib/types.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include "physical-memory.h"

typedef struct frame
{
	uint32_t address;
	uint32_t ref_count;
	SLIST_ENTRY(frame) next;
} frame_t;

SLIST_HEAD(, frame) free_frames;
SLIST_HEAD(, frame) used_frames;

extern char __kernel_start, __kernel_end;

static uint32_t physical_memory_start;
static uint32_t physical_memory_end;
static frame_t *frames_array;

#define FRAMES_ARRAY_ADDRSS PAGE_ALIGN_UP((paddr_t)(&__kernel_end))

status_t
physical_memory_setup(size_t ram_size)
{
	frame_t *frame;
	paddr_t kernel_base, kernel_top, frame_address;

	SLIST_INIT(&free_frames);
	SLIST_INIT(&used_frames);

	ram_size = PAGE_ALIGN_DOWN(ram_size);

	// Update the addresses managed by the physical memory allocator
	kernel_base = PAGE_ALIGN_DOWN((paddr_t)(&__kernel_start));
	kernel_top   = FRAMES_ARRAY_ADDRSS
		+ PAGE_ALIGN_UP((ram_size >> PAGE_SHIFT) * sizeof(frame_t));

	// Is there enough memory to fit the kenel?
	if (kernel_top > ram_size)
		return -KERNEL_NO_MEMORY;

	/* The first page can't be allocated because it serves
	 * to indicate un unavailable page by return address 0 */
	physical_memory_start = PAGE_SIZE;
	physical_memory_end   = ram_size;

	frames_array = (frame_t *)FRAMES_ARRAY_ADDRSS;

	enum {RESERVED, FREE, KERNEL} action;

	for (frame_address = 0, frame = frames_array;
		frame_address < physical_memory_end;
		frame_address += PAGE_SIZE, frame++)
	{
		memset(frame, 0, sizeof(frame_t));

		frame->address = frame_address;

		if (frame_address < kernel_base)
		{
			action = RESERVED;
		}
		else if ((frame_address >= kernel_base)
			&& (frame_address < (paddr_t)&__kernel_start))
		{
			action = FREE;
		}
		else if ((frame_address >= kernel_base)
			&& (frame_address < kernel_top))
		{
			action = KERNEL;
		}
		else
		{
			action = FREE;
		}

		switch (action)
		{
			case FREE:
				frame->ref_count = 0;
				SLIST_INSERT_HEAD(&free_frames, frame, next);
				break;

			case KERNEL:
				frame->ref_count = 1;
				SLIST_INSERT_HEAD(&used_frames, frame, next);
				break;

			default:
				// Reserved frame: NOP
				break;
		}
	}

	return KERNEL_OK;
}

paddr_t
frame_alloc(void)
{
	frame_t *frame;

	if (SLIST_EMPTY(&free_frames))
		return (paddr_t)NULL;

	frame = SLIST_FIRST(&free_frames);
	SLIST_REMOVE(&free_frames, frame, frame, next);

	assert(frame->ref_count == 0);

	frame->ref_count++;

	SLIST_INSERT_HEAD(&used_frames, frame, next);

	return frame->address;
}

inline static frame_t *
frame_at_address(paddr_t frame_address)
{
	if (frame_address & PAGE_MASK)
		return NULL;

	if ((frame_address < physical_memory_start)
		|| (frame_address >= physical_memory_end))
		return NULL;

	return frames_array + (frame_address >> PAGE_SHIFT);
}

status_t
frame_free(paddr_t frame_address)
{
	status_t status = !KERNEL_OK;

	frame_t *frame = frame_at_address(frame_address);

	if (!frame)
		return -KERNEL_INVALID_VALUE;

	frame->ref_count--;

	if (frame->ref_count == 0)
	{
		SLIST_REMOVE(&used_frames, frame, frame, next);
		SLIST_INSERT_HEAD(&free_frames, frame, next);
		status = KERNEL_OK;
	}

	return status;
}
