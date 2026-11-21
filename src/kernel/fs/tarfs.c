/*
 * Copyright (c) 2015-2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <lib/c/stdio.h>
#include <lib/c/assert.h>
#include <lib/types.h>
#include <lib/queue.h>
#include <arch/x86/paging.h>
#include "vfs.h"
#include "tarfs.h"


static struct file_system tarfs;
static paddr_t initrd_start, initrd_end;
static struct node *tarfs_root;

struct path_node
{
	char name[NODE_NAME_LENGTH]; /* Flawfinder: ignore */
	STAILQ_ENTRY(path_node) next;
};

STAILQ_HEAD(, path_node) path_nodes;
static spinlock_t tarfs_ns_lock = SPINLOCK_INIT;

static status_t path_nodes_list_delete(void);

static struct node *
node_get(struct node *n)
{
	if (n)
	{
		atomic_inc(&n->refcount);
	}

	return n;
}

static void
node_put(struct node *n)
{
	if (n && atomic_dec_and_test(&n->refcount))
	{
		if (n->type == TMPFS_FILE && n->u.file.heap)
		{
			free(n->u.file.data);
		}

		free(n);
	}
}

static struct node *
folder_lookup(struct node *folder, const char *name, size_t length)
{
	struct node *child;
	uint32_t flags;

	if (folder->type != TMPFS_FOLDER)
	{
		return NULL;
	}

	flags = spinlock_lock_irqsave(&folder->lock);

	LIST_FOREACH(child, &folder->u.folder.nodes, next)
	{
		if (strncmp(child->name, name, length) == 0 && child->name[length] == '\0')
		{
			node_get(child);
			break;
		}
	}

	spinlock_unlock_irqrestore(&folder->lock, flags);
	return child;
}

/*
 * TAR file decoding functions.
 */

// Parse an octal number, ignoring leading and trailing nonsense.
static int
parseoct(const char *p, size_t n)
{
	int i = 0;

	while (*p < '0' || *p > '7')
	{
		++p;
		--n;
	}

	while (*p >= '0' && *p <= '7' && n > 0)
	{
		i *= 8;
		i += *p - '0';
		++p;
		--n;
	}
	return i;
}

/* Returns true if this is 512 zero bytes. */
static int
is_end_of_archive(const char *p)
{
	int n;
	for (n = 511; n >= 0; --n)
		if (p[n] != '\0')
			return 0;
	return 1;
}

/* Verify tar archive's checksum. */
static int
checksum(const char *p)
{
	int n, u = 0;
	for (n = 0; n < 512; ++n) {
		if (n < 148 || n > 155)
			/* Standard tar checksum adds unsigned bytes. */
			u += ((unsigned char *)p)[n]; /* Flawfinder: ignore */
		else
			u += 0x20;

	}
	return (u == parseoct(p + 148, 8));
}

/*
 * TARFS functions.
 */

static status_t
path_nodes_to_list(const char *path)
{
	size_t i = 0;
	char *name  = malloc(NODE_NAME_LENGTH);
	if (!name)
		return -KERNEL_NO_MEMORY;

	// Skip the first '/'
	if (path[0] == '/')
		path++;

	while (*path)
	{
		if (*path == '/')
		{
			name[i] = '\0'; // make a string

			struct path_node *pnode = malloc(sizeof(struct path_node));
			if (!pnode)
			{
				free(name);
				path_nodes_list_delete();
				return -KERNEL_NO_MEMORY;
			}

			strzcpy(pnode->name, name, sizeof(pnode->name));
			STAILQ_INSERT_TAIL(&path_nodes, pnode, next);

			free(name);
			name = malloc(NODE_NAME_LENGTH);
			if (!name)
			{
				path_nodes_list_delete();
				return -KERNEL_NO_MEMORY;
			}


			// Reinitialize
			i = 0;
			path++;
			continue;
		}

		/* Overlong components are truncated, as strzcpy would. */
		if (i < NODE_NAME_LENGTH - 1)
			name[i++] = *path;

		path++;
	}

	// There was a string after the last '/'
	if (i > 0)
	{
		name[i] = '\0';
		struct path_node *pnode = malloc(sizeof(struct path_node));
		if (!pnode)
		{
			free(name);
			path_nodes_list_delete();
			return -KERNEL_NO_MEMORY;
		}

		strzcpy(pnode->name, name, sizeof(pnode->name));

		STAILQ_INSERT_TAIL(&path_nodes, pnode, next);
	}

	free(name);

	return KERNEL_OK;
}

static status_t
path_nodes_list_delete(void)
{
	struct path_node *node;

	while (!STAILQ_EMPTY(&path_nodes))
	{
		node = STAILQ_FIRST(&path_nodes);
		STAILQ_REMOVE_HEAD(&path_nodes, next);
		free(node);
	}

	return KERNEL_OK;
}

static char *
tarfs_basename(const char *path)
{
	/* Holds a whole path, not just a name. */
	static char path_copy[PATH_MAX]; /* Flawfinder: ignore */
	char *result;

	strzcpy(path_copy, path, sizeof(path_copy));

	if (strnlen(path_copy, sizeof(path_copy)) == 1 && path_copy[0] == '/')
	{
		path_copy[0] = '/';
		path_copy[1] = '\0';
		return path_copy;
	}

	// Strip trailing '/' for no root folders
	if (strnlen(path_copy, sizeof(path_copy)) > 1 && path_copy[strnlen(path_copy, sizeof(path_copy)) - 1] == '/')
	{
		path_copy[strnlen(path_copy, sizeof(path_copy)) - 1] = '\0';
	}

	result = strrchr(path_copy, '/');
	return result ? result + 1 : (char *)path_copy;
}

#if 0
static char *
tarfs_dirname(const char *path)
{
	static char result[PATH_MAX]; /* Flawfinder: ignore */
	char path_copy[PATH_MAX]; /* Flawfinder: ignore */

	strzcpy(path_copy, path, sizeof(path_copy));

	if (strnlen(path_copy, sizeof(path_copy)) == 1 && path_copy[0] == '/')
	{
		result[0] = '/';
		result[1] = '\0';
		return result;
	}

	// Strip trailing '/' for no root folders
	if (strnlen(path_copy, sizeof(path_copy)) > 1 && path_copy[strnlen(path_copy, sizeof(path_copy)) - 1] == '/')
	{
		path_copy[strnlen(path_copy, sizeof(path_copy)) - 1] = '\0';
	}

	char *last_slash = strrchr(path_copy, '/');
	if (!last_slash)
		return NULL;

	int new_path_length = strnlen(path_copy, sizeof(path_copy)) - (strnlen(last_slash, sizeof(path_copy)) - 1);
	path_copy[new_path_length] = '\0';

	// Strip a remaining '/' after a filename was removed.
	if (new_path_length > 1 && path_copy[new_path_length - 1] == '/')
	{
		path_copy[new_path_length - 1] = '\0';
	}

	strzcpy(result, path_copy, sizeof(result));

	return result;
}
#endif

static status_t
add_node(const char *path, uint8_t type, size_t file_size, void *archive,
		struct node *root_node)
{
	// The root node was already added in init.
	if (strnlen(path, NODE_NAME_LENGTH) == 1 && path[0] == '/')
		return KERNEL_OK;

	char *filename = tarfs_basename(path);

	// Create a new node
	struct node *new_node = malloc(sizeof(struct node));
	if (!new_node)
		return -KERNEL_NO_MEMORY;

	memset(new_node, 0, sizeof(struct node));
	new_node->refcount = 1;
	strzcpy(new_node->name, filename, sizeof(new_node->name));
	new_node->name_length = strnlen(new_node->name, NODE_NAME_LENGTH) + 1;

	if (type == TMPFS_FOLDER)
	{
		new_node->type = TMPFS_FOLDER;
		LIST_INIT(&(new_node->u.folder.nodes));
	}
	else if (type == TMPFS_FILE)
	{
		new_node->type = TMPFS_FILE;
		new_node->u.file.size = file_size;
		/* Higher-half VA so access works under a process CR3. */
		new_node->u.file.data = PA2VA((uint32_t)(uintptr_t)archive);
	}

	// Create a list of node names separated by '/'
	status_t status = path_nodes_to_list(path);
	if (status != KERNEL_OK)
	{
		free(new_node);
		return status;
	}

	struct node *tmp_node, *iter_node;
	struct path_node *path_node_iter;

	tmp_node = root_node;

	bool inserted = false;

	STAILQ_FOREACH(path_node_iter, &path_nodes, next)
	{
		if (STAILQ_NEXT(path_node_iter, next) != NULL)
		{
			struct node *child = NULL;

			LIST_FOREACH(iter_node, &tmp_node->u.folder.nodes, next)
			{
				if (strncmp(path_node_iter->name, iter_node->name,
						strnlen(path_node_iter->name,
							NODE_NAME_LENGTH)+1) == 0)
				{
					if (iter_node->type == TMPFS_FOLDER)
						child = iter_node;
					break;
				}
			}

			if (!child)
			{
				path_nodes_list_delete();
				free(new_node);
				return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
			}

			tmp_node = child;
			continue;
		}

		LIST_FOREACH(iter_node, &tmp_node->u.folder.nodes, next)
		{
			if (strncmp(path_node_iter->name, iter_node->name,
					strnlen(path_node_iter->name,
						NODE_NAME_LENGTH)+1) == 0)
			{
				path_nodes_list_delete();
				free(new_node);
				return KERNEL_OK;
			}
		}

		LIST_INSERT_HEAD(&(tmp_node->u.folder.nodes), new_node, next);
		inserted = true;
	}

	path_nodes_list_delete();

	if (!inserted)
		free(new_node);

	return KERNEL_OK;
}

static void
untar(void *ramdisk_address, struct node *root_node)
{
	char buffer[512]; /* Flawfinder: ignore */
	size_t file_size;
	status_t status;

	while (1)
	{
		memcpy_s(buffer, 512, ramdisk_address, 512);
		ramdisk_address += 512;

		if (is_end_of_archive(buffer))
			return;

		if (!checksum(buffer))
		{
			kprintf("Checksum failure\n");
			return;
		}

		if (buffer[156] == '0' || buffer[156] == '7')
		{
			file_size = (size_t)parseoct(buffer + 124, 12);

			size_t data_blocks = (file_size + 511) / 512;   /* ceil(file_size / 512) */
			size_t padded_size = data_blocks * 512;
			void *data_start = ramdisk_address;
			ramdisk_address += padded_size;

			char *normalized_path = strchr(buffer, '/');

			status = add_node(normalized_path, TMPFS_FILE,
					file_size, data_start, root_node);
			assert(status == KERNEL_OK);
		}
		else if (buffer[156] == '5')
		{
			char *normalized_path = strchr(buffer, '/');

			status = add_node(normalized_path, TMPFS_FOLDER, 0,
					ramdisk_address, root_node);
			assert(status == KERNEL_OK);
		}
	}
}

static status_t
tarfs_build(void)
{
	void *ramdisk_address = (void *)initrd_start;

	struct node *root_node = malloc(sizeof(struct node));

	if (!root_node)
	{
		return -KERNEL_NO_MEMORY;
	}

	memset(root_node, 0, sizeof(*root_node));
	root_node->refcount = 1;
	root_node->type = TMPFS_FOLDER;
	root_node->name_length = strnlen("/", NODE_NAME_LENGTH)+1;
	strzcpy(root_node->name, "/", root_node->name_length);

	LIST_INIT(&(root_node->u.folder.nodes));
	STAILQ_INIT(&path_nodes);
	untar(ramdisk_address, root_node);

	tarfs_root = root_node;

	return KERNEL_OK;
}

static void
free_node_tree(struct node *node)
{
	if (!node)
	{
		return;
	}

	if (node->type == TMPFS_FOLDER)
	{
		struct node *child;

		while (!LIST_EMPTY(&node->u.folder.nodes))
		{
			child = LIST_FIRST(&node->u.folder.nodes);
			LIST_REMOVE(child, next);
			free_node_tree(child);
		}
	}

	if (node->type == TMPFS_FILE && node->u.file.heap)
	{
		free(node->u.file.data);
	}

	free(node);
}

static status_t
tarfs_vfs_umount(struct mountpoint *mp)
{
	(void)mp;

	if (tarfs_root)
	{
		free_node_tree(tarfs_root);
		tarfs_root = NULL;
	}

	return KERNEL_OK;
}

static const struct vnode_ops tarfs_vnode_ops;

static vfs_node_type_t
tarfs_vtype(const struct node *n)
{
	return n->type == TMPFS_FOLDER ? VFS_NODE_DIRECTORY
				       : VFS_NODE_REGULAR_FILE;
}

/* Consumes the caller's reference on n. */
static struct vnode *
tarfs_vnode_new(struct mountpoint *mp, struct node *n)
{
	struct vnode *vn = vnode_alloc(mp, tarfs_vtype(n),
			n->type == TMPFS_FILE ? n->u.file.size : 0,
			&tarfs_vnode_ops, n);

	if (!vn)
	{
		node_put(n);
	}

	return vn;
}

static bool
tarfs_name_exists(struct node *directory, const char *name)
{
	struct node *n;

	LIST_FOREACH(n, &directory->u.folder.nodes, next)
	{
		if (!strncmp(n->name, name, NODE_NAME_LENGTH))
			return true;
	}

	return false;
}

static status_t
tarfs_vop_lookup(struct vnode *directory, const char *name, struct vnode **result)
{
	size_t length = strnlen(name, NODE_NAME_LENGTH);
	struct node *child;

	if (length >= NODE_NAME_LENGTH)
	{
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	child = folder_lookup(directory->data, name, length);

	if (!child)
	{
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	*result = tarfs_vnode_new(directory->mp, child);
	return *result ? KERNEL_OK : -KERNEL_NO_MEMORY;
}

static ssize_t
tarfs_vop_read(struct vnode *vn, void *buffer, size_t length, loff_t offset) /* Flawfinder: ignore */
{
	struct node *n = vn->data;
	uint32_t flags = spinlock_lock_irqsave(&n->lock);
	size_t available;

	if ((uint64_t)offset >= n->u.file.size)
	{
		length = 0;
	}
	else
	{
		available = n->u.file.size - (size_t)offset;

		if (length > available)
		{
			length = available;
		}

		memcpy_s(buffer, length, n->u.file.data + (size_t)offset, length);
	}

	spinlock_unlock_irqrestore(&n->lock, flags);
	return (ssize_t)length;
}

static ssize_t
tarfs_vop_write(struct vnode *vn, const void *buf, size_t len, loff_t offset)
{
	struct node *n = vn->data;
	char *old = NULL;
	size_t end;
	uint32_t flags;

	if ((uint64_t)offset + len > 0xFFFFFFFFu)
	{
		return -KERNEL_INVALID_VALUE;
	}

	end = (size_t)offset + len;
	flags = spinlock_lock_irqsave(&n->lock);

	/* Initrd data is read-only: move the file to the heap on first write. */
	if (!n->u.file.heap || end > n->u.file.size)
	{
		size_t new_size = end > n->u.file.size ? end : n->u.file.size;
		char *data = malloc(new_size ? new_size : 1);

		if (!data)
		{
			spinlock_unlock_irqrestore(&n->lock, flags);
			return -KERNEL_NO_MEMORY;
		}

		memset(data, 0, new_size);

		if (n->u.file.size)
		{
			memcpy_s(data, new_size, n->u.file.data, n->u.file.size);
		}

		if (n->u.file.heap)
		{
			old = n->u.file.data;
		}

		n->u.file.data = data;
		n->u.file.size = (uint32_t)new_size;
		n->u.file.heap = true;
	}

	memcpy_s(n->u.file.data + (size_t)offset,
		 n->u.file.size - (size_t)offset, buf, len);
	vn->size = n->u.file.size;
	spinlock_unlock_irqrestore(&n->lock, flags);
	free(old);
	return (ssize_t)len;
}

static int
tarfs_vop_readdir(struct vnode *directory, uint32_t index, struct dirent *entry)
{
	struct node *folder = directory->data;
	struct node *child;
	uint32_t i = 0;
	uint32_t flags = spinlock_lock_irqsave(&folder->lock);

	LIST_FOREACH(child, &folder->u.folder.nodes, next)
	{
		if (i++ == index)
		{
			break;
		}
	}

	if (child)
	{
		entry->d_type = tarfs_vtype(child);
		entry->d_size = child->type == TMPFS_FILE ? child->u.file.size : 0;
		strzcpy(entry->d_name, child->name, sizeof(entry->d_name));
	}

	spinlock_unlock_irqrestore(&folder->lock, flags);
	return child ? 1 : 0;
}

static status_t
tarfs_vop_create(struct vnode *directory, const char *name, vfs_node_type_t type)
{
	struct node *parent = directory->data;
	struct node *new_node;
	status_t status;
	uint32_t flags;

	if (strnlen(name, NODE_NAME_LENGTH) >= NODE_NAME_LENGTH)
	{
		return -KERNEL_INVALID_VALUE;
	}

	new_node = malloc(sizeof(struct node));

	if (!new_node)
	{
		return -KERNEL_NO_MEMORY;
	}

	memset(new_node, 0, sizeof(struct node));
	strzcpy(new_node->name, name, sizeof(new_node->name));
	new_node->name_length = strnlen(new_node->name, NODE_NAME_LENGTH) + 1;
	new_node->type = type == VFS_NODE_DIRECTORY ? TMPFS_FOLDER : TMPFS_FILE;
	new_node->refcount = 1;

	if (new_node->type == TMPFS_FOLDER)
	{
		LIST_INIT(&new_node->u.folder.nodes);
	}

	flags = spinlock_lock_irqsave(&tarfs_ns_lock);

	if (parent->unlinked)
	{
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}
	else if (tarfs_name_exists(parent, name))
	{
		status = -KERNEL_FILE_ALREADY_EXISTS;
	}
	else
	{
		spinlock_lock(&parent->lock);
		LIST_INSERT_HEAD(&parent->u.folder.nodes, new_node, next);
		spinlock_unlock(&parent->lock);
		status = KERNEL_OK;
	}

	spinlock_unlock_irqrestore(&tarfs_ns_lock, flags);

	if (status != KERNEL_OK)
	{
		free(new_node);
	}

	return status;
}

static status_t
tarfs_vop_remove(struct vnode *directory, const char *name, vfs_node_type_t type)
{
	struct node *parent = directory->data;
	struct node *node;
	uint8_t tarfs_type = type == VFS_NODE_DIRECTORY ? TMPFS_FOLDER
							: TMPFS_FILE;
	status_t status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	uint32_t flags = spinlock_lock_irqsave(&tarfs_ns_lock);

	LIST_FOREACH(node, &parent->u.folder.nodes, next)
	{
		if (strncmp(node->name, name, NODE_NAME_LENGTH) != 0)
		{
			continue;
		}

		if (node->type != tarfs_type)
		{
			break;
		}

		if (tarfs_type == TMPFS_FOLDER && !LIST_EMPTY(&node->u.folder.nodes))
		{
			status = -KERNEL_BUSY;
			break;
		}

		node->unlinked = true;
		spinlock_lock(&parent->lock);
		LIST_REMOVE(node, next);
		spinlock_unlock(&parent->lock);
		status = KERNEL_OK;
		break;
	}

	spinlock_unlock_irqrestore(&tarfs_ns_lock, flags);

	if (status == KERNEL_OK)
	{
		node_put(node);		/* drop the folder link; freed when unused */
	}

	return status;
}

static status_t
tarfs_vop_rename(struct vnode *old_dir, const char *old_name,
		struct vnode *new_dir, const char *new_name)
{
	struct node *src_parent = old_dir->data;
	struct node *dst_parent = new_dir->data;
	struct node *src = NULL;
	struct node *n;
	status_t status = KERNEL_OK;
	uint32_t flags;

	if (strnlen(new_name, NODE_NAME_LENGTH) >= NODE_NAME_LENGTH)
	{
		return -KERNEL_INVALID_VALUE;
	}

	flags = spinlock_lock_irqsave(&tarfs_ns_lock);

	LIST_FOREACH(n, &src_parent->u.folder.nodes, next)
	{
		if (!strncmp(n->name, old_name, NODE_NAME_LENGTH))
		{
			src = n;
			break;
		}
	}

	if (!src || dst_parent->unlinked)
	{
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}
	else if (tarfs_name_exists(dst_parent, new_name))
	{
		status = -KERNEL_FILE_ALREADY_EXISTS;
	}
	else
	{
		spinlock_lock(&src_parent->lock);
		LIST_REMOVE(src, next);
		spinlock_unlock(&src_parent->lock);

		strzcpy(src->name, new_name, NODE_NAME_LENGTH);
		src->name_length = strnlen(src->name, NODE_NAME_LENGTH) + 1;

		spinlock_lock(&dst_parent->lock);
		LIST_INSERT_HEAD(&dst_parent->u.folder.nodes, src, next);
		spinlock_unlock(&dst_parent->lock);
	}

	spinlock_unlock_irqrestore(&tarfs_ns_lock, flags);
	return status;
}

static void
tarfs_vop_release(struct vnode *vn)
{
	node_put(vn->data);
}

static const struct vnode_ops tarfs_vnode_ops =
{
	.lookup  = tarfs_vop_lookup,
	.read    = tarfs_vop_read,
	.write   = tarfs_vop_write,
	.readdir = tarfs_vop_readdir,
	.create  = tarfs_vop_create,
	.remove  = tarfs_vop_remove,
	.rename  = tarfs_vop_rename,
	.release = tarfs_vop_release,
};

static status_t
tarfs_vfs_mount(struct mountpoint *mp, const char *source, const char *args)
{
	(void)source;
	(void)args;

	if (!tarfs_root)
	{
		status_t status = tarfs_build();

		if (status != KERNEL_OK)
		{
			return status;
		}
	}

	mp->root = tarfs_vnode_new(mp, node_get(tarfs_root));
	return mp->root ? KERNEL_OK : -KERNEL_NO_MEMORY;
}

status_t
tarfs_init(paddr_t start, paddr_t end)
{
	strzcpy(tarfs.name, "tarfs", sizeof(tarfs.name));
	tarfs.mount  = tarfs_vfs_mount;
	tarfs.umount = tarfs_vfs_umount;

	initrd_start = start;
	initrd_end   = end;

	return fs_register(&tarfs);
}

