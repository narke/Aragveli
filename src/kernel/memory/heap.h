/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>
#include <drivers/vbe.h>

void heap_setup(size_t ram_size,
		vaddr_t identity_mapping_start,
		vaddr_t identity_mapping_end,
		uint32_t framebuffer_start,
		uint32_t framebuffer_end);

void *heap_alloc(size_t size);
void heap_free(void *ptr);
