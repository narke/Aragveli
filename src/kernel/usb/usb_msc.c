#include <lib/c/stdio.h>
#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include <drivers/blockdev.h>
#include <fs/vfs.h>
#include <process/mutex.h>
#include <process/semaphore.h>
#include <process/thread.h>
#include "tusb.h"
#include "usb.h"

#define MSC_SECTOR_SIZE   512
#define MSC_BOUNCE_SECTORS 8

struct msc_request
{
	uint8_t       dev_addr;
	uint32_t      lba;
	uint16_t      count;
	bool          write;
	volatile bool ok;
};

static struct block_device msc_dev;
static bool msc_registered;
static volatile uint8_t msc_addr;
static uint8_t *bounce;
static mutex_t *msc_lock;
static semaphore_t *msc_done;
static struct msc_request *volatile pending;

static bool
msc_complete(uint8_t dev_addr, const tuh_msc_complete_data_t *cb_data)
{
	struct msc_request *req = (struct msc_request *)cb_data->user_arg;

	(void)dev_addr;

	req->ok = cb_data->csw->status == MSC_CSW_STATUS_PASSED;
	semaphore_up(msc_done);

	return true;
}

bool
usb_msc_pending(void)
{
	return pending != NULL;
}

/* Runs in the usb thread only, so TinyUSB is never entered concurrently */
void
usb_msc_service(void)
{
	struct msc_request *req = pending;
	bool ok;

	if (req == NULL || !tuh_msc_ready(req->dev_addr))
	{
		return;
	}

	pending = NULL;

	if (req->write)
	{
		ok = tuh_msc_write10(req->dev_addr, 0, bounce, req->lba,
				req->count, msc_complete, (uintptr_t)req);
	}
	else
	{
		ok = tuh_msc_read10(req->dev_addr, 0, bounce, req->lba,
				req->count, msc_complete, (uintptr_t)req);
	}

	if (!ok)
	{
		req->ok = false;
		semaphore_up(msc_done);
	}
}

static status_t
msc_transfer(uint32_t lba, uint8_t *dst, const uint8_t *src, uint32_t count,
		bool write)
{
	status_t status = KERNEL_OK;

	if (lba >= msc_dev.sector_count || count > msc_dev.sector_count - lba)
	{
		return -KERNEL_INVALID_VALUE;
	}

	mutex_lock(msc_lock);

	while (count > 0)
	{
		uint16_t n = (uint16_t)(count < MSC_BOUNCE_SECTORS ? count : MSC_BOUNCE_SECTORS);
		size_t len = n * MSC_SECTOR_SIZE;
		struct msc_request req = { msc_addr, lba, n, write, false };

		if (req.dev_addr == 0)
		{
			status = -KERNEL_NO_SUCH_DEVICE;
			break;
		}

		if (write)
		{
			memcpy(bounce, src, len);
		}

		pending = &req;
		usb_wakeup();
		semaphore_down(msc_done);

		if (!req.ok)
		{
			status = -KERNEL_INTERNAL_FATAL_ERROR;
			break;
		}

		if (write)
		{
			src += len;
		}
		else
		{
			memcpy(dst, bounce, len);
			dst += len;
		}

		lba += n;
		count -= n;
	}

	mutex_unlock(msc_lock);

	return status;
}

static status_t
msc_read(struct block_device *dev, uint32_t lba, void *buf, uint32_t count)
{
	(void)dev;
	return msc_transfer(lba, buf, NULL, count, false);
}

static status_t
msc_write(struct block_device *dev, uint32_t lba, const void *buf,
		uint32_t count)
{
	(void)dev;
	return msc_transfer(lba, NULL, buf, count, true);
}

static void
msc_mount_thread(uint32_t arg)
{
	(void)arg;

	status_t status = vfs_create("/", "/mnt/usb", VFS_NODE_DIRECTORY);

	if (status == KERNEL_OK || status == -KERNEL_FILE_ALREADY_EXISTS)
	{
		status = vfs_mount("usb0", "/mnt/usb", "fatfs", NULL);
	}

	if (status == KERNEL_OK)
	{
		kprintf("usb0 mounted on /mnt/usb\n");
	}
	else
	{
		kprintf("mount usb0 on /mnt/usb failed: %d\n", status);
	}

	thread_exit();
}

void
tuh_msc_mount_cb(uint8_t dev_addr)
{
	uint32_t block_size = tuh_msc_get_block_size(dev_addr, 0);

	if (block_size != MSC_SECTOR_SIZE)
	{
		kprintf("USB storage: unsupported block size %lu\n", block_size);
		return;
	}

	if (!msc_registered)
	{
		bounce   = malloc(MSC_BOUNCE_SECTORS * MSC_SECTOR_SIZE);
		msc_lock = mutex_create();
		msc_done = semaphore_create(0);

		if (!bounce || !msc_lock || !msc_done)
		{
			return;
		}

		strzcpy(msc_dev.name, "usb0", sizeof(msc_dev.name));
		msc_dev.sector_size = MSC_SECTOR_SIZE;
		msc_dev.read  = msc_read;
		msc_dev.write = msc_write;
		blockdev_register(&msc_dev);
		msc_registered = true;
	}

	msc_dev.sector_count = tuh_msc_get_block_count(dev_addr, 0);
	msc_addr = dev_addr;

	kprintf("USB storage: %lu sectors\n", msc_dev.sector_count);

	// vfs_mount() reads sectors, which needs this (usb) thread to be free
	thread_kernel_create("usb-mount", msc_mount_thread, NULL);
}

void
tuh_msc_umount_cb(uint8_t dev_addr)
{
	(void)dev_addr;

	msc_addr = 0;
	kprintf("USB storage removed\n");
}
