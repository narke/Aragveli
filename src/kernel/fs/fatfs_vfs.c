/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86/spinlock.h>
#include <drivers/blockdev.h>
#include <lib/c/stdio.h>
#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include "fatfs/ff.h"
#include "fatfs/fatfs_port.h"
#include "vfs.h"
#include "fatfs_vfs.h"

struct fatfs_mount
{
	FATFS fs;
	BYTE  pdrv;		// Physical drive number
	char  drive[3];		// "N:" */ /* Flawfinder: ignore
};

struct fatfs_vnode
{
	char path[PATH_MAX];	/* "N:/dir/file" */ /* Flawfinder: ignore */
};

static struct file_system fatfs_fs;
static const struct vnode_ops fatfs_vnode_ops;

static struct block_device *pdrv_owner[FF_VOLUMES];
static spinlock_t pdrv_lock = SPINLOCK_INIT;

static status_t
fatfs_status(FRESULT res)
{
	switch (res)
	{
		case FR_OK:
			return KERNEL_OK;
		case FR_NO_FILE:
		case FR_NO_PATH:
			return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
		case FR_EXIST:
			return -KERNEL_FILE_ALREADY_EXISTS;
		case FR_DENIED:
		case FR_WRITE_PROTECTED:
			return -KERNEL_PERMISSION_ERROR;
		case FR_LOCKED:
		case FR_TIMEOUT:
		case FR_TOO_MANY_OPEN_FILES:
			return -KERNEL_BUSY;
		case FR_NOT_ENOUGH_CORE:
			return -KERNEL_NO_MEMORY;
		case FR_NOT_READY:
		case FR_NO_FILESYSTEM:
		case FR_INVALID_DRIVE:
		case FR_NOT_ENABLED:
			return -KERNEL_NO_SUCH_DEVICE;
		case FR_INVALID_NAME:
		case FR_INVALID_OBJECT:
		case FR_INVALID_PARAMETER:
			return -KERNEL_INVALID_VALUE;
		default:
			return -KERNEL_INTERNAL_FATAL_ERROR;
	}
}

/* Returns a free FatFs drive number bound to dev, or -1. */
static int
pdrv_claim(struct block_device *dev)
{
	int pdrv = -1;
	uint32_t flags = spinlock_lock_irqsave(&pdrv_lock);

	for (int i = 0; i < FF_VOLUMES; i++)
	{
		if (pdrv_owner[i] == dev)
		{
			pdrv = -1;
			break;
		}

		if (!pdrv_owner[i] && pdrv < 0)
		{
			pdrv = i;
		}
	}

	if (pdrv >= 0)
	{
		pdrv_owner[pdrv] = dev;
	}

	spinlock_unlock_irqrestore(&pdrv_lock, flags);
	return pdrv;
}

static void
pdrv_release(BYTE pdrv)
{
	uint32_t flags = spinlock_lock_irqsave(&pdrv_lock);

	pdrv_owner[pdrv] = NULL;
	spinlock_unlock_irqrestore(&pdrv_lock, flags);
}

static status_t
fatfs_child_path(const char *dir, const char *name, char *out, size_t size)
{
	size_t len = strnlen(dir, size);
	size_t name_len = strnlen(name, NAME_MAX + 1);
	size_t sep = dir[len - 1] == '/' ? 0 : 1;

	if (name_len > NAME_MAX || len + sep + name_len >= size)
	{
		return -KERNEL_INVALID_VALUE;
	}

	strzcpy(out, dir, size);

	if (sep)
	{
		out[len++] = '/';
	}

	strzcpy(out + len, name, size - len);
	return KERNEL_OK;
}

static struct vnode *
fatfs_vnode_new(struct mountpoint *mp, const char *path,
		vfs_node_type_t type, loff_t size)
{
	struct fatfs_vnode *fv = malloc(sizeof(struct fatfs_vnode));
	struct vnode *vn;

	if (!fv)
	{
		return NULL;
	}

	strzcpy(fv->path, path, sizeof(fv->path));
	vn = vnode_alloc(mp, type, size, &fatfs_vnode_ops, fv);

	if (!vn)
	{
		free(fv);
	}

	return vn;
}

/*
 * Vnode operations
 */

static status_t
fatfs_vop_lookup(struct vnode *dir, const char *name, struct vnode **result)
{
	struct fatfs_vnode *parent = dir->data;
	struct fatfs_vnode *fv = malloc(sizeof(struct fatfs_vnode));
	FILINFO *fno = malloc(sizeof(FILINFO));
	status_t status;

	if (!fv || !fno)
	{
		status = -KERNEL_NO_MEMORY;
		goto out;
	}

	status = fatfs_child_path(parent->path, name, fv->path, sizeof(fv->path));

	if (status != KERNEL_OK)
	{
		goto out;
	}

	status = fatfs_status(f_stat(fv->path, fno));

	if (status != KERNEL_OK)
	{
		goto out;
	}

	*result = vnode_alloc(dir->mp,
			(fno->fattrib & AM_DIR) ? VFS_NODE_DIRECTORY
						: VFS_NODE_REGULAR_FILE,
			(loff_t)fno->fsize, &fatfs_vnode_ops, fv);

	if (*result)
	{
		fv = NULL;	/* now owned by the vnode */
	}
	else
	{
		status = -KERNEL_NO_MEMORY;
	}

out:
	free(fno);
	free(fv);
	return status;
}

static ssize_t
fatfs_vop_read(struct vnode *vn, void *buf, size_t len, loff_t offset) /* Flawfinder: ignore */
{
	struct fatfs_vnode *fv = vn->data;
	FIL *fil = malloc(sizeof(FIL));
	UINT done = 0;
	FRESULT res;

	if (!fil)
	{
		return -KERNEL_NO_MEMORY;
	}

	res = f_open(fil, fv->path, FA_READ);

	if (res == FR_OK)
	{
		res = f_lseek(fil, (FSIZE_t)offset);

		if (res == FR_OK)
		{
			res = f_read(fil, buf, (UINT)len, &done);
		}

		f_close(fil);
	}

	free(fil);
	return res == FR_OK ? (ssize_t)done : fatfs_status(res);
}

static ssize_t
fatfs_vop_write(struct vnode *vn, const void *buf, size_t len, loff_t offset)
{
	struct fatfs_vnode *fv = vn->data;
	FIL *fil = malloc(sizeof(FIL));
	UINT done = 0;
	FRESULT res;

	if (!fil)
	{
		return -KERNEL_NO_MEMORY;
	}

	res = f_open(fil, fv->path, FA_WRITE | FA_OPEN_EXISTING);

	if (res == FR_OK)
	{
		res = f_lseek(fil, (FSIZE_t)offset);

		if (res == FR_OK)
		{
			res = f_write(fil, buf, (UINT)len, &done);
		}

		vn->size = (loff_t)f_size(fil);

		FRESULT close_res = f_close(fil);

		if (res == FR_OK)
		{
			res = close_res;
		}
	}

	free(fil);

	/* A short count with FR_OK means the volume is full. */
	return res == FR_OK ? (ssize_t)done : fatfs_status(res);
}

static int
fatfs_vop_readdir(struct vnode *dir, uint32_t index, struct dirent *entry)
{
	struct fatfs_vnode *fv = dir->data;
	DIR *dp = malloc(sizeof(DIR));
	FILINFO *fno = malloc(sizeof(FILINFO));
	FRESULT res = FR_NOT_ENOUGH_CORE;
	int ret;

	if (dp && fno)
	{
		res = f_opendir(dp, fv->path);

		if (res == FR_OK)
		{
			for (uint32_t i = 0; ; i++)
			{
				res = f_readdir(dp, fno);

				if (res != FR_OK || fno->fname[0] == '\0'
					|| i == index)
					break;
			}

			f_closedir(dp);
		}
	}

	if (res != FR_OK)
	{
		ret = fatfs_status(res);
	}
	else if (fno->fname[0] == '\0')
	{
		ret = 0;
	}
	else
	{
		entry->d_type = (fno->fattrib & AM_DIR) ? VFS_NODE_DIRECTORY
							: VFS_NODE_REGULAR_FILE;
		entry->d_size = fno->fsize;

		if (fno->fdate != 0)
		{
			entry->d_mtime.year   = (uint16_t)((fno->fdate >> 9) + 1980);
			entry->d_mtime.month  = (uint8_t)((fno->fdate >> 5) & 0x0F);
			entry->d_mtime.day    = (uint8_t)(fno->fdate & 0x1F);
			entry->d_mtime.hour   = (uint8_t)(fno->ftime >> 11);
			entry->d_mtime.minute = (uint8_t)((fno->ftime >> 5) & 0x3F);
			entry->d_mtime.second = (uint8_t)((fno->ftime & 0x1F) * 2);
		}

		strzcpy(entry->d_name, fno->fname, sizeof(entry->d_name));
		ret = 1;
	}

	free(fno);
	free(dp);
	return ret;
}

static status_t
fatfs_vop_create(struct vnode *dir, const char *name, vfs_node_type_t type)
{
	struct fatfs_vnode *parent = dir->data;
	char *path = malloc(PATH_MAX);
	FIL *fil = NULL;
	status_t status;
	FRESULT res;

	if (!path)
	{
		return -KERNEL_NO_MEMORY;
	}

	status = fatfs_child_path(parent->path, name, path, PATH_MAX);

	if (status != KERNEL_OK)
	{
		goto out;
	}

	if (type == VFS_NODE_DIRECTORY)
	{
		res = f_mkdir(path);
	}
	else
	{
		fil = malloc(sizeof(FIL));
		res = fil ? f_open(fil, path, FA_CREATE_NEW | FA_WRITE)
			  : FR_NOT_ENOUGH_CORE;

		if (res == FR_OK)
		{
			res = f_close(fil);
		}
	}

	status = fatfs_status(res);

out:
	free(fil);
	free(path);
	return status;
}

static status_t
fatfs_vop_remove(struct vnode *dir, const char *name, vfs_node_type_t type)
{
	struct fatfs_vnode *parent = dir->data;
	char *path = malloc(PATH_MAX);
	FILINFO *fno = malloc(sizeof(FILINFO));
	status_t status;
	FRESULT res;

	if (!path || !fno)
	{
		status = -KERNEL_NO_MEMORY;
		goto out;
	}

	status = fatfs_child_path(parent->path, name, path, PATH_MAX);

	if (status != KERNEL_OK)
	{
		goto out;
	}

	status = fatfs_status(f_stat(path, fno));

	if (status != KERNEL_OK)
	{
		goto out;
	}

	if (((fno->fattrib & AM_DIR) != 0) != (type == VFS_NODE_DIRECTORY))
	{
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
		goto out;
	}

	res = f_unlink(path);

	/* f_unlink refuses non-empty directories with FR_DENIED. */
	if (res == FR_DENIED && type == VFS_NODE_DIRECTORY)
	{
		status = -KERNEL_BUSY;
	}
	else
	{
		status = fatfs_status(res);
	}

out:
	free(fno);
	free(path);
	return status;
}

static status_t
fatfs_vop_rename(struct vnode *old_dir, const char *old_name,
		struct vnode *new_dir, const char *new_name)
{
	struct fatfs_vnode *src_parent = old_dir->data;
	struct fatfs_vnode *dst_parent = new_dir->data;
	char *old_path = malloc(PATH_MAX);
	char *new_path = malloc(PATH_MAX);
	status_t status = -KERNEL_NO_MEMORY;

	if (old_path && new_path)
	{
		status = fatfs_child_path(src_parent->path, old_name,
				old_path, PATH_MAX);

		if (status == KERNEL_OK)
		{
			status = fatfs_child_path(dst_parent->path, new_name,
					new_path, PATH_MAX);
		}

		if (status == KERNEL_OK)
		{
			status = fatfs_status(f_rename(old_path, new_path));
		}
	}

	free(new_path);
	free(old_path);
	return status;
}

static status_t
fatfs_vop_truncate(struct vnode *vn, loff_t size)
{
	struct fatfs_vnode *fv = vn->data;
	FIL *fil = malloc(sizeof(FIL));
	FRESULT res;

	if (!fil)
	{
		return -KERNEL_NO_MEMORY;
	}

	res = f_open(fil, fv->path, FA_WRITE | FA_OPEN_EXISTING);

	if (res == FR_OK)
	{
		res = f_lseek(fil, (FSIZE_t)size);

		if (res == FR_OK)
			res = f_truncate(fil);

		vn->size = (loff_t)f_size(fil);

		FRESULT close_res = f_close(fil);
		if (res == FR_OK)
			res = close_res;
	}

	free(fil);
	return fatfs_status(res);
}

static status_t
fatfs_vop_sync(struct vnode *vn)
{
	/* Every operation closes its FIL, which flushes it. */
	(void)vn;
	return KERNEL_OK;
}

static void
fatfs_vop_release(struct vnode *vn)
{
	free(vn->data);
}

static const struct vnode_ops fatfs_vnode_ops =
{
	.lookup   = fatfs_vop_lookup,
	.read     = fatfs_vop_read,
	.write    = fatfs_vop_write,
	.readdir  = fatfs_vop_readdir,
	.create   = fatfs_vop_create,
	.remove   = fatfs_vop_remove,
	.rename   = fatfs_vop_rename,
	.truncate = fatfs_vop_truncate,
	.sync     = fatfs_vop_sync,
	.release  = fatfs_vop_release,
};

/*
 * Mounting
 */

static const char *
fatfs_type_name(BYTE fs_type)
{
	switch (fs_type)
	{
		case FS_FAT12: return "FAT12";
		case FS_FAT16: return "FAT16";
		case FS_FAT32: return "FAT32";
		case FS_EXFAT: return "exFAT";
		default:       return "unknown";
	}
}

static status_t
fatfs_vfs_mount(struct mountpoint *mp, const char *source, const char *args)
{
	struct block_device *dev = blockdev_find(source);
	struct fatfs_mount *fm;
	char root[4]; /* Flawfinder: ignore */
	FRESULT res;
	int pdrv;

	(void)args;

	if (!dev)
	{
		return -KERNEL_NO_SUCH_DEVICE;
	}

	fm = malloc(sizeof(struct fatfs_mount));

	if (!fm)
	{
		return -KERNEL_NO_MEMORY;
	}

	pdrv = pdrv_claim(dev);
	if (pdrv < 0)
	{
		free(fm);
		return -KERNEL_BUSY;
	}

	fm->pdrv     = (BYTE)pdrv;
	fm->drive[0] = (char)('0' + pdrv);
	fm->drive[1] = ':';
	fm->drive[2] = '\0';
	diskio_bind(fm->pdrv, dev);

	/* Forced mount: detects FAT12/16/32/exFAT and reports errors now. */
	res = f_mount(&fm->fs, fm->drive, 1);

	if (res == FR_OK)
	{
		root[0] = fm->drive[0];
		root[1] = ':';
		root[2] = '/';
		root[3] = '\0';

		mp->root = fatfs_vnode_new(mp, root, VFS_NODE_DIRECTORY, 0);

		if (!mp->root)
		{
			f_mount(NULL, fm->drive, 0);
			res = FR_NOT_ENOUGH_CORE;
		}
	}

	if (res != FR_OK)
	{
		diskio_bind(fm->pdrv, NULL);
		pdrv_release(fm->pdrv);
		free(fm);
		return fatfs_status(res);
	}

	mp->data = fm;
	kprintf("fatfs: %s (%s) mounted on %s\n", source,
			fatfs_type_name(fm->fs.fs_type), mp->path);
	return KERNEL_OK;
}

static status_t
fatfs_vfs_umount(struct mountpoint *mp)
{
	struct fatfs_mount *fm = mp->data;
	FRESULT res = f_mount(NULL, fm->drive, 0);

	diskio_bind(fm->pdrv, NULL);
	pdrv_release(fm->pdrv);
	free(fm);
	return fatfs_status(res);
}

status_t
fatfs_init(void)
{
	strzcpy(fatfs_fs.name, "fatfs", sizeof(fatfs_fs.name));
	fatfs_fs.mount  = fatfs_vfs_mount;
	fatfs_fs.umount = fatfs_vfs_umount;

	return fs_register(&fatfs_fs);
}
