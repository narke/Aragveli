#include <arch/x86/spinlock.h>
#include <lib/c/string.h>
#include "blockdev.h"

static LIST_HEAD(, block_device) block_devices =
	LIST_HEAD_INITIALIZER(block_devices);
static spinlock_t blockdev_lock = SPINLOCK_INIT;

status_t
blockdev_register(struct block_device *dev)
{
	uint32_t flags = spinlock_lock_irqsave(&blockdev_lock);

	LIST_INSERT_HEAD(&block_devices, dev, next);
	spinlock_unlock_irqrestore(&blockdev_lock, flags);
	return KERNEL_OK;
}

struct block_device *
blockdev_find(const char *name)
{
	struct block_device *dev;
	uint32_t flags = spinlock_lock_irqsave(&blockdev_lock);

	LIST_FOREACH(dev, &block_devices, next)
	{
		if (strncmp(dev->name, name, BLOCKDEV_NAME_MAX) == 0)
		{
			break;
		}
	}

	spinlock_unlock_irqrestore(&blockdev_lock, flags);
	return dev;
}
