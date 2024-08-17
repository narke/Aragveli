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

	LIST_FOREACH(fs, &file_systems, next)
	{
		if (strncmp(fs_name, fs->name, strnlen(fs->name, FS_NAME_MAXLEN)+1) == 0)
		{
			return fs->mount(root_device, mount_point, mount_args,
					result_rootfs);
		}
	}

	return -KERNEL_NO_SUCH_DEVICE;
}

status_t
fs_register(struct file_system *fs)
{
	struct file_system *fs_item;

	LIST_FOREACH(fs_item, &file_systems, next)
	{
		if (!strncmp(fs->name, fs_item->name, strnlen(fs_item->name, FS_NAME_MAXLEN)+1))
			return -KERNEL_FILE_ALREADY_EXISTS;
	}

	LIST_INSERT_HEAD(&file_systems, fs, next);

	return KERNEL_OK;
}

status_t
fs_unregister(struct file_system *fs)
{
	struct file_system *fs_item;

	LIST_FOREACH(fs_item, &file_systems, next)
	{
		if (!strncmp(fs->name, fs_item->name, NAME_MAX))
		{
			LIST_REMOVE(fs, next);
			free(fs);
			return KERNEL_OK;
		}
	}

	return -KERNEL_INVALID_VALUE;
}
