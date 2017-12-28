/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

typedef struct
{
	uint32_t 	nb_blocks;
	uint32_t 	block_size;
	uint32_t 	nb_free_blocks;
	uint32_t 	nb_initialized_blocks;
	unsigned char 	*mem_pool_start;
	unsigned char 	*next_mem_pool;
} pool_t;


void pool_create(pool_t *pool, uint32_t block_size, uint32_t nb_blocks);
void pool_destroy(pool_t *pool);
void *pool_allocate(pool_t *pool);
void pool_deallocate(pool_t *pool, void *ptr);
