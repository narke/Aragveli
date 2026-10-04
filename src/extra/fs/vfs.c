/*
 * Copyright (c) 2015, 2017 Konstantin Tcholokachvili.
 * Copyright (c) 2009 Martin Decky.
 * Copyright (c) 2008 Jakub Jermar.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */


#include <fs/vfs.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>

LIST_HEAD(, file_system) file_systems;
static spinlock_t fs_list_lock = SPINLOCK_INIT;

status_t
vfs_list_init(void)
{
	LIST_INIT(&file_systems);
	return KERNEL_OK;
}

status_t
vfs_init(const char        *root_device,
	const char         *fs_name,
	const char         *mount_point,
	const char         *mount_args,
	struct superblock **result_rootfs)
{
	struct file_system *fs;
	status_t status;
	uint32_t flags = spinlock_lock_irqsave(&fs_list_lock);

	LIST_FOREACH(fs, &file_systems, next)
	{
		if (strncmp(fs_name, fs->name, strnlen(fs->name, FS_NAME_MAXLEN)+1) == 0)
		{
			atomic_inc(&fs->refcount);
			break;
		}
	}

	spinlock_unlock_irqrestore(&fs_list_lock, flags);

	if (!fs)
	{
		return -KERNEL_NO_SUCH_DEVICE;
	}

	status = fs->mount(root_device, mount_point, mount_args, result_rootfs);

	if (status != KERNEL_OK)
	{
		atomic_dec(&fs->refcount);
	}

	return status;
}

status_t
fs_register(struct file_system *fs)
{
	struct file_system *fs_item;
	status_t status = KERNEL_OK;
	uint32_t flags = spinlock_lock_irqsave(&fs_list_lock);

	LIST_FOREACH(fs_item, &file_systems, next)
	{
		if (!strncmp(fs->name, fs_item->name, strnlen(fs_item->name, FS_NAME_MAXLEN)+1))
		{
			status = -KERNEL_FILE_ALREADY_EXISTS;
			break;
		}
	}

	if (status == KERNEL_OK)
	{
		LIST_INSERT_HEAD(&file_systems, fs, next);
	}

	spinlock_unlock_irqrestore(&fs_list_lock, flags);
	return status;
}

status_t
fs_unregister(struct file_system *fs)
{
	struct file_system *fs_item;
	status_t status = -KERNEL_INVALID_VALUE;
	uint32_t flags = spinlock_lock_irqsave(&fs_list_lock);

	LIST_FOREACH(fs_item, &file_systems, next)
	{
		if (!strncmp(fs->name, fs_item->name, NAME_MAX))
		{
			if (atomic_read(&fs->refcount) != 0)
			{
				status = -KERNEL_BUSY;
				break;
			}

			LIST_REMOVE(fs, next);
			status = KERNEL_OK;
			break;
		}
	}

	spinlock_unlock_irqrestore(&fs_list_lock, flags);

	if (status == KERNEL_OK)
	{
		free(fs);
	}

	return status;
}
