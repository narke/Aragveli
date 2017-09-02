/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/status.h>

#define PAGE_SIZE 4096

#define PAGE_SHIFT 12

#define PAGE_MASK ((1<<12) - 1)

#define PAGE_ALIGN_DOWN(value) \
	(((unsigned)(value)) & (~((PAGE_SIZE)-1)))

#define PAGE_ALIGN_UP(value) \
	({ unsigned int __boundary = (PAGE_SIZE); \
	(((((unsigned)(value))-1) & (~(__boundary - 1))) + __boundary); })

#define IS_PAGE_ALIGNED(value) \
	(0 == (((unsigned)(value)) & ((PAGE_SIZE)-1)))

status_t physical_memory_setup(size_t ram_size);

paddr_t frame_alloc(void);

status_t frame_free(paddr_t frame_address);
