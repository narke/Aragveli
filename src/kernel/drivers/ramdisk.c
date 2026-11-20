#include <arch/x86/paging.h>
#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include "blockdev.h"
#include "ramdisk.h"

#define RAMDISK_SECTOR_SIZE 512

static bool
ramdisk_range_ok(struct block_device *dev, uint32_t lba, uint32_t count)
{
	return lba < dev->sector_count && count <= dev->sector_count - lba;
}

static status_t
ramdisk_read(struct block_device *dev, uint32_t lba, void *buf, uint32_t count)
{
	size_t len = count * dev->sector_size;

	if (!ramdisk_range_ok(dev, lba, count))
		return -KERNEL_INVALID_VALUE;

	memcpy_s(buf, len, (uint8_t *)dev->data + lba * dev->sector_size, len);
	return KERNEL_OK;
}

static status_t
ramdisk_write(struct block_device *dev, uint32_t lba, const void *buf,
		uint32_t count)
{
	size_t len = count * dev->sector_size;

	if (!ramdisk_range_ok(dev, lba, count))
		return -KERNEL_INVALID_VALUE;

	memcpy_s((uint8_t *)dev->data + lba * dev->sector_size, len, buf, len);
	return KERNEL_OK;
}

status_t
ramdisk_create(const char *name, paddr_t start, paddr_t end)
{
	struct block_device *dev = malloc(sizeof(struct block_device));

	if (!dev)
		return -KERNEL_NO_MEMORY;

	memset(dev, 0, sizeof(*dev));
	strzcpy(dev->name, name, sizeof(dev->name));
	dev->sector_size  = RAMDISK_SECTOR_SIZE;
	dev->sector_count = (end - start) / RAMDISK_SECTOR_SIZE;
	dev->read  = ramdisk_read;
	dev->write = ramdisk_write;
	/* Higher-half VA so access works under a process CR3. */
	dev->data  = PA2VA(start);

	return blockdev_register(dev);
}
