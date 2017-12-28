/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include "pool.h"


// See a paper called: Fast Efficient Fixed-Size Memory Pool

void
pool_create(pool_t *pool, uint32_t block_size, uint32_t nb_blocks)
{
	pool->nb_blocks		= nb_blocks;
	pool->block_size	= block_size;
	pool->mem_pool_start	= malloc(nb_blocks * block_size);
	pool->nb_free_blocks 	= nb_blocks;
	pool->next_mem_pool 	= pool->mem_pool_start;
}

void
pool_destroy(pool_t *pool)
{
	free(pool->mem_pool_start);
	pool->mem_pool_start = NULL;
}

static unsigned char *
address_from_index(pool_t *pool, unsigned int i)
{
	return pool->mem_pool_start + (i * pool->block_size);
}

static unsigned int
index_from_address(pool_t *pool, const unsigned char *p)
{
	return (((unsigned int)(p - pool->mem_pool_start)) / pool->block_size);
}

void *
pool_allocate(pool_t *pool)
{
	if (pool->nb_initialized_blocks < pool->nb_blocks)
	{
		uint32_t *p = (uint32_t *)address_from_index(pool, pool->nb_initialized_blocks);
		*p = pool->nb_initialized_blocks + 1;
		pool->nb_initialized_blocks++;
	}

	void *ret = NULL;

	if (pool->nb_free_blocks > 0)
	{
		ret = (void *)pool->next_mem_pool;
		--pool->nb_free_blocks;

		if (pool->nb_free_blocks != 0)
			pool->next_mem_pool = address_from_index(pool, *((uint32_t *)pool->next_mem_pool));
		else
			pool->next_mem_pool = NULL;
	}

	return ret;
}

void
pool_deallocate(pool_t *pool, void *ptr)
{
	if (pool->next_mem_pool != NULL)
	{
		(*(uint32_t *)ptr) = index_from_address(pool, pool->next_mem_pool);
		pool->next_mem_pool = (unsigned char *)ptr;
	}
	else
	{
		(*(uint32_t *)ptr) = pool->nb_blocks;
		pool->next_mem_pool = (unsigned char *)ptr;
	}

	++pool->nb_free_blocks;
}
