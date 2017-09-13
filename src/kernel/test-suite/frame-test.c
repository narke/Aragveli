/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/types.h>
#include <lib/c/assert.h>
#include <lib/c/printf.h>
#include <lib/queue.h>
#include <arch/x86-pc/vbe.h>
#include <memory/frame.h>

#include "frame-test.h"

#define MAGIC 511

typedef struct physical_page
{
	uint32_t before[MAGIC];
	SLIST_ENTRY(physical_page) next;
	uint32_t after[MAGIC];
} physical_page_t;


void test_frame_allocator(void)
{
	SLIST_HEAD(, physical_page) physical_pages;

	physical_page_t *physical_page;

	uint32_t nb_allocated_physical_pages = 0;
	uint32_t nb_free_physical_pages = 0;

	SLIST_INIT(&physical_pages);

	vbe_set_position(5, 0);
	printf("++ Physical memory allocaion/deallocation test! ++\n");

	// Test the allocation
	while ((physical_page = (physical_page_t *)frame_alloc()) != NULL)
	{
		int i;
		nb_allocated_physical_pages++;

		for (i = 0 ; i < MAGIC ; i++)
		{
			physical_page->before[i] = (uint32_t)physical_page;
			physical_page->after[i]  = (uint32_t)physical_page;
		}

		SLIST_INSERT_HEAD(&physical_pages, physical_page, next);
	}

	// Test the deallocation
	SLIST_FOREACH(physical_page, &physical_pages, next)
	{
		int i;

		for (i = 0 ; i < MAGIC ; i++)
		{
			if ((physical_page->before[i] !=  (uint32_t)physical_page)
				|| (physical_page->after[i] !=  (uint32_t)physical_page))
			{
				printf("Page overwritten\n");
				return;
			}
		}

		if (frame_free((uint32_t)physical_page) < 0)
		{
			printf("Cannot dealloc page\n");
			return;
		}

		nb_free_physical_pages++;

		vbe_set_position(30, 7);
	}

	assert(nb_allocated_physical_pages == nb_free_physical_pages);

	printf("Can allocate %d bytes and free %d bytes \n",
		nb_allocated_physical_pages << PAGE_SHIFT,
		nb_free_physical_pages << PAGE_SHIFT);
}
