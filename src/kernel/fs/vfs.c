/*
 * Copyright (c) 2015-2026 Konstantin Tcholokachvili.
 * Copyright (c) 2009 Martin Decky.
 * Copyright (c) 2008 Jakub Jermar.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <fs/vfs.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <process/mutex.h>

LIST_HEAD(, file_system) file_systems;
static spinlock_t fs_list_lock = SPINLOCK_INIT;

static TAILQ_HEAD(, mountpoint) mountpoints =
	TAILQ_HEAD_INITIALIZER(mountpoints);
static mutex_t mount_lock;

status_t
vfs_list_init(void)
{
	LIST_INIT(&file_systems);
	mutex_init(&mount_lock);
	return KERNEL_OK;
}

/* Returns the file system with its refcount raised, or NULL. */
static struct file_system *
fs_find(const char *name)
{
	struct file_system *fs;
	uint32_t flags = spinlock_lock_irqsave(&fs_list_lock);

	LIST_FOREACH(fs, &file_systems, next)
	{
		if (strncmp(name, fs->name, FS_NAME_MAXLEN) == 0)
		{
			atomic_inc(&fs->refcount);
			break;
		}
	}

	spinlock_unlock_irqrestore(&fs_list_lock, flags);
	return fs;
}

/* fs_register() and fs_unregister(): keep your existing versions. */

/*
 * Vnodes
 */

struct vnode *
vnode_alloc(struct mountpoint *mp, vfs_node_type_t type, loff_t size,
		const struct vnode_ops *ops, void *data)
{
	struct vnode *vn = malloc(sizeof(struct vnode));

	if (!vn)
	{
		return NULL;
	}

	vn->type     = type;
	vn->size     = size;
	vn->mp       = mp;
	vn->ops      = ops;
	vn->data     = data;
	vn->refcount = 1;
	atomic_inc(&mp->vnode_count);

	return vn;
}

void
vnode_get(struct vnode *vn)
{
	if (vn)
	{
		atomic_inc(&vn->refcount);
	}
}

void
vnode_put(struct vnode *vn)
{
	if (vn && atomic_dec_and_test(&vn->refcount))
	{
		if (vn->ops->release)
		{
			vn->ops->release(vn);
		}

		atomic_dec(&vn->mp->vnode_count);
		free(vn);
	}
}

/*
 * Paths
 */

/* Builds an absolute path without ".", ".." or repeated '/'. */
status_t
vfs_normalize(const char *cwd, const char *path, char *out, size_t size)
{
	char joined[PATH_MAX]; /* Flawfinder: ignore */
	size_t len = 0;
	const char *p;

	if (!path || !out || size < 2)
	{
		return -KERNEL_INVALID_VALUE;
	}

	joined[0] = '\0';

	if (path[0] != '/')
	{
		if (!cwd || cwd[0] != '/')
		{
			return -KERNEL_INVALID_VALUE;
		}

		strzcpy(joined, cwd, sizeof(joined));
		len = strnlen(joined, sizeof(joined));
		joined[len++] = '/';
	}

	if (len + strnlen(path, PATH_MAX) >= sizeof(joined))
	{
		return -KERNEL_INVALID_VALUE;
	}

	strzcpy(joined + len, path, sizeof(joined) - len);

	out[0] = '/';
	out[1] = '\0';
	len = 1;

	for (p = joined; *p;)
	{
		const char *start;
		size_t clen;

		while (*p == '/')
		{
			p++;
		}

		if (*p == '\0')
		{
			break;
		}

		for (start = p; *p && *p != '/'; p++)
		{
			;
		}

		clen = (size_t)(p - start);

		if (clen == 1 && start[0] == '.')
		{
			continue;
		}

		if (clen == 2 && start[0] == '.' && start[1] == '.')
		{
			while (len > 1 && out[len - 1] != '/')
			{
				len--;
			}

			if (len > 1)
			{
				len--;
			}

			out[len] = '\0';
			continue;
		}

		if (clen > NAME_MAX || len + 1 + clen >= size)
		{
			return -KERNEL_INVALID_VALUE;
		}

		if (len > 1)
		{
			out[len++] = '/';
		}

		memcpy_s(out + len, size - len, start, clen);
		len += clen;
		out[len] = '\0';
	}

	return KERNEL_OK;
}

/* Caller holds mount_lock. */
static struct mountpoint *
mount_find_locked(const char *path, const char **rest)
{
	struct mountpoint *mp, *best = NULL;
	size_t best_len = 0;

	TAILQ_FOREACH(mp, &mountpoints, next)
	{
		size_t len = strnlen(mp->path, PATH_MAX);

		if (len > 1 && (strncmp(path, mp->path, len) != 0
				|| (path[len] != '\0' && path[len] != '/')))
		{
			continue;
		}

		if (!best || len > best_len)
		{
			best = mp;
			best_len = len;
		}
	}

	if (best && rest)
	{
		*rest = path + (best_len == 1 ? 0 : best_len);
	}

	return best;
}

static bool
mount_exists(const char *path)
{
	struct mountpoint *mp;
	bool found = false;

	mutex_lock(&mount_lock);

	TAILQ_FOREACH(mp, &mountpoints, next)
	{
		if (strncmp(mp->path, path, PATH_MAX) == 0)
		{
			found = true;
			break;
		}
	}

	mutex_unlock(&mount_lock);
	return found;
}

status_t
vfs_lookup(const char *cwd, const char *path, struct vnode **result)
{
	char abs_path[PATH_MAX]; /* Flawfinder: ignore */
	char name[NAME_MAX + 1]; /* Flawfinder: ignore */
	const char *p = NULL;
	struct mountpoint *mp;
	struct vnode *vn;
	status_t status;

	status = vfs_normalize(cwd, path, abs_path, sizeof(abs_path));

	if (status != KERNEL_OK)
	{
		return status;
	}

	mutex_lock(&mount_lock);
	mp = mount_find_locked(abs_path, &p);
	vn = mp ? mp->root : NULL;
	vnode_get(vn);
	mutex_unlock(&mount_lock);

	if (!vn)
	{
		return -KERNEL_NO_SUCH_DEVICE;
	}

	while (*p)
	{
		struct vnode *next;
		const char *start;
		size_t len;

		while (*p == '/')
		{
			p++;
		}

		if (*p == '\0')
		{
			break;
		}

		for (start = p; *p && *p != '/'; p++)
		{
			;
		}

		len = (size_t)(p - start);
		memcpy_s(name, sizeof(name), start, len);
		name[len] = '\0';

		if (vn->type != VFS_NODE_DIRECTORY || !vn->ops->lookup)
		{
			vnode_put(vn);
			return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
		}

		status = vn->ops->lookup(vn, name, &next);
		vnode_put(vn);

		if (status != KERNEL_OK)
		{
			return status;
		}

		vn = next;
	}

	*result = vn;
	return KERNEL_OK;
}

/* Resolves the parent folder of path and copies the last component to name. */
static status_t
vfs_lookup_parent(const char *cwd, const char *path, struct vnode **parent,
		char *name)
{
	char abs_path[PATH_MAX]; /* Flawfinder: ignore */
	char *slash;
	status_t status;

	status = vfs_normalize(cwd, path, abs_path, sizeof(abs_path));

	if (status != KERNEL_OK)
	{
		return status;
	}

	if (abs_path[1] == '\0' || mount_exists(abs_path))
	{
		return -KERNEL_BUSY;
	}

	slash = strrchr(abs_path, '/');
	strzcpy(name, slash + 1, NAME_MAX + 1);

	if (slash == abs_path)
	{
		slash[1] = '\0';
	}
	else
	{
		*slash = '\0';
	}

	status = vfs_lookup("/", abs_path, parent);

	if (status == KERNEL_OK && (*parent)->type != VFS_NODE_DIRECTORY)
	{
		vnode_put(*parent);
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	return status;
}

/*
 * Operations
 */

ssize_t
vfs_read(struct vnode *vn, void *buffer, size_t len, loff_t offset) /* Flawfinder: ignore */
{
	if (!vn || !buffer || offset < 0 || vn->type != VFS_NODE_REGULAR_FILE)
	{
		return -KERNEL_INVALID_VALUE;
	}

	if (!vn->ops->read)
	{
		return -KERNEL_OPERATION_NOT_SUPPORTED;
	}

	return vn->ops->read(vn, buffer, len, offset);
}

ssize_t
vfs_write(struct vnode *vn, const void *buffer, size_t len, loff_t offset)
{
	if (!vn || !buffer || offset < 0 || vn->type != VFS_NODE_REGULAR_FILE)
	{
		return -KERNEL_INVALID_VALUE;
	}

	if (!vn->ops->write)
	{
		return -KERNEL_OPERATION_NOT_SUPPORTED;
	}

	return vn->ops->write(vn, buffer, len, offset);
}

int
vfs_readdir(struct vnode *dir, uint32_t index, struct dirent *entry)
{
	if (!dir || !entry || dir->type != VFS_NODE_DIRECTORY)
	{
		return -KERNEL_INVALID_VALUE;
	}

	if (!dir->ops->readdir)
	{
		return -KERNEL_OPERATION_NOT_SUPPORTED;
	}

	return dir->ops->readdir(dir, index, entry);
}

status_t
vfs_create(const char *cwd, const char *path, vfs_node_type_t type)
{
	char name[NAME_MAX + 1]; /* Flawfinder: ignore */
	struct vnode *dir;
	status_t status = vfs_lookup_parent(cwd, path, &dir, name);

	if (status != KERNEL_OK)
	{
		return status;
	}

	status = dir->ops->create ? dir->ops->create(dir, name, type)
				  : -KERNEL_OPERATION_NOT_SUPPORTED;
	vnode_put(dir);
	return status;
}

status_t
vfs_remove(const char *cwd, const char *path, vfs_node_type_t type)
{
	char name[NAME_MAX + 1]; /* Flawfinder: ignore */
	struct vnode *dir;
	status_t status = vfs_lookup_parent(cwd, path, &dir, name);

	if (status != KERNEL_OK)
	{
		return status;
	}

	status = dir->ops->remove ? dir->ops->remove(dir, name, type)
				  : -KERNEL_OPERATION_NOT_SUPPORTED;
	vnode_put(dir);
	return status;
}

status_t
vfs_rename(const char *cwd, const char *old_path, const char *new_path)
{
	char old_name[NAME_MAX + 1]; /* Flawfinder: ignore */
	char new_name[NAME_MAX + 1]; /* Flawfinder: ignore */
	struct vnode *old_dir, *new_dir;
	status_t status;

	status = vfs_lookup_parent(cwd, old_path, &old_dir, old_name);

	if (status != KERNEL_OK)
	{
		return status;
	}

	status = vfs_lookup_parent(cwd, new_path, &new_dir, new_name);

	if (status != KERNEL_OK)
	{
		vnode_put(old_dir);
		return status;
	}

	if (old_dir->mp != new_dir->mp)
	{
		status = -KERNEL_OPERATION_NOT_SUPPORTED;
	}
	else if (!old_dir->ops->rename)
	{
		status = -KERNEL_OPERATION_NOT_SUPPORTED;
	}
	else
	{
		status = old_dir->ops->rename(old_dir, old_name,
				new_dir, new_name);
	}

	vnode_put(new_dir);
	vnode_put(old_dir);
	return status;
}

/*
 * Mounting
 */

status_t
vfs_mount(const char *source, const char *target, const char *fs_name,
		const char *args)
{
	struct mountpoint *mp;
	struct file_system *fs;
	status_t status;

	mp = malloc(sizeof(struct mountpoint));

	if (!mp)
	{
		return -KERNEL_NO_MEMORY;
	}

	memset(mp, 0, sizeof(struct mountpoint));

	status = vfs_normalize("/", target, mp->path, sizeof(mp->path));

	if (status != KERNEL_OK)
	{
		goto fail;
	}

	if (mp->path[1] != '\0')
	{
		struct vnode *dir;
		bool is_dir;

		status = vfs_lookup("/", mp->path, &dir);

		if (status != KERNEL_OK)
		{
			goto fail;
		}

		is_dir = dir->type == VFS_NODE_DIRECTORY;
		vnode_put(dir);

		if (!is_dir)
		{
			status = -KERNEL_INVALID_VALUE;
			goto fail;
		}
	}

	fs = fs_find(fs_name);

	if (!fs)
	{
		status = -KERNEL_NO_SUCH_DEVICE;
		goto fail;
	}

	mp->fs = fs;
	status = fs->mount(mp, source, args);

	if (status != KERNEL_OK)
	{
		atomic_dec(&fs->refcount);
		goto fail;
	}

	mutex_lock(&mount_lock);

	{
		const char *rest = NULL;
		struct mountpoint *cur = mount_find_locked(mp->path, &rest);

		if (cur && *rest == '\0')
		{
			status = -KERNEL_BUSY;
		}
		else
		{
			TAILQ_INSERT_TAIL(&mountpoints, mp, next);
		}
	}

	mutex_unlock(&mount_lock);

	if (status != KERNEL_OK)
	{
		vnode_put(mp->root);

		if (fs->umount)
		{
			fs->umount(mp);
		}

		atomic_dec(&fs->refcount);
		goto fail;
	}

	return KERNEL_OK;

fail:
	free(mp);
	return status;
}

static bool
mount_has_children_locked(const struct mountpoint *parent)
{
	struct mountpoint *mp;
	size_t len = strnlen(parent->path, PATH_MAX);

	TAILQ_FOREACH(mp, &mountpoints, next)
	{
		if (mp == parent)
		{
			continue;
		}

		if (len == 1 || (strncmp(mp->path, parent->path, len) == 0
				&& mp->path[len] == '/'))
		{
			return true;
		}
	}

	return false;
}

status_t
vfs_umount(const char *target)
{
	char path[PATH_MAX]; /* Flawfinder: ignore */
	struct mountpoint *mp;
	status_t status = vfs_normalize("/", target, path, sizeof(path));

	if (status != KERNEL_OK)
	{
		return status;
	}

	mutex_lock(&mount_lock);

	TAILQ_FOREACH(mp, &mountpoints, next)
	{
		if (strncmp(mp->path, path, PATH_MAX) == 0)
		{
			break;
		}
	}

	if (!mp)
	{
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}
	else if (atomic_read(&mp->vnode_count) != 1
		|| atomic_read(&mp->root->refcount) != 1
		|| mount_has_children_locked(mp))
	{
		status = -KERNEL_BUSY;
	}
	else
	{
		TAILQ_REMOVE(&mountpoints, mp, next);
	}

	mutex_unlock(&mount_lock);

	if (status != KERNEL_OK)
	{
		return status;
	}

	vnode_put(mp->root);

	if (mp->fs->umount)
	{
		status = mp->fs->umount(mp);
	}

	atomic_dec(&mp->fs->refcount);
	free(mp);
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
