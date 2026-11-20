#pragma once

#include <lib/types.h>
#include <lib/queue.h>
#include <lib/status.h>

#define BLOCKDEV_NAME_MAX 16

struct block_device
{
	char     name[BLOCKDEV_NAME_MAX]; /* Flawfinder: ignore */
	uint32_t sector_size;
	uint32_t sector_count;
	status_t (*read)(struct block_device *dev, uint32_t lba,
			void *buf, uint32_t count);
	status_t (*write)(struct block_device *dev, uint32_t lba,
			const void *buf, uint32_t count);
	void    *data;
	LIST_ENTRY(block_device) next;
};

status_t blockdev_register(struct block_device *dev);
struct block_device *blockdev_find(const char *name);
