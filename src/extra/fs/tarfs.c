/*
 * Copyright (c) 2015, 2017 Konstantin Tcholokachvili.
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
#include "vfs.h"
#include "tarfs.h"

/* TMPFS node operations */
node_ops_t tarfs_node_ops =
{
	.root_get     = NULL,
	.match        = NULL,
	.node_get     = NULL,
	.node_open    = NULL,
	.node_put     = NULL,
	.create       = NULL,
	.destroy      = NULL,
	.link         = NULL,
	.unlink       = NULL,
	.has_children = NULL,
	.index_get    = NULL,
	.size_get     = NULL,
	.lnkcnt_get   = NULL,
	.is_directory = NULL,
	.is_file      = NULL
};

static struct file_system tarfs;
static paddr_t initrd_start, initrd_end;

struct path_node
{
	char name[NODE_NAME_LENGTH];
	STAILQ_ENTRY(path_node) next;
};

STAILQ_HEAD(, path_node) path_nodes;


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
			u += ((unsigned char *)p)[n];
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
	uint8_t i = 0;
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
				return -KERNEL_NO_MEMORY;

			strzcpy(pnode->name, name, strlen(name)+1);
			STAILQ_INSERT_TAIL(&path_nodes, pnode, next);

			name = malloc(NODE_NAME_LENGTH);
			if (!name)
				return -KERNEL_NO_MEMORY;


			// Reinitialize
			i = 0;
			path++;
			continue;
		}

		name[i] = *path;
		path++;
		i++;
	}

	// There was a string after the last '/'
	if (i > 0)
	{
		name[i] = '\0';
		struct path_node *pnode = malloc(sizeof(struct path_node));
		if (!pnode)
			return -KERNEL_NO_MEMORY;

		strzcpy(pnode->name, name, strlen(name)+1);

		STAILQ_INSERT_TAIL(&path_nodes, pnode, next);
	}

	return KERNEL_OK;
}

static status_t
path_nodes_list_delete(void)
{
	struct path_node *node = malloc(sizeof(struct path_node));
	if (!node)
		return -KERNEL_NO_MEMORY;

	while (!STAILQ_EMPTY(&path_nodes))
	{
		node = STAILQ_FIRST(&path_nodes);
		STAILQ_REMOVE(&path_nodes, node, path_node, next);
		free(node);
	}

	return KERNEL_OK;
}

struct node *
resolve_node(const char *path, struct node *root_node)
{
	struct node *tmp_node, *node;
	bool found = false;

	// Create a list of node names separated by '/'
	path_nodes_to_list(path);

	tmp_node = root_node;

	struct path_node *pn;
	STAILQ_FOREACH(pn, &path_nodes, next)
	{
		LIST_FOREACH(node, &tmp_node->u.folder.nodes, next)
		{
			if (strncmp(pn->name, node->name, strlen(pn->name)+1) == 0)
			{
				tmp_node = node;
				found = true;
			}
		}

		if (!found)
		{
			tmp_node = NULL;
			break;
		}
	}

	path_nodes_list_delete();

	return tmp_node;
}

static char *
tarfs_basename(const char *path)
{
	static char path_copy[NODE_NAME_LENGTH];
	char *result;

	strzcpy(path_copy, path, strlen(path)+1);

	if (strlen(path_copy) == 1 && path_copy[0] == '/')
	{
		path_copy[0] = '/';
		path_copy[1] = '\0';
		return path_copy;
	}

	// Strip trailing '/' for no root folders
	if (strlen(path_copy) > 1 && path_copy[strlen(path_copy) - 1] == '/')
	{
		path_copy[strlen(path_copy) - 1] = '\0';
	}

	result = strrchr(path_copy, '/');
	return result ? result + 1 : (char *)path_copy;
}

#if 0
static char *
tarfs_dirname(const char *path)
{
	static char result[NODE_NAME_LENGTH];
	char path_copy[NODE_NAME_LENGTH];

	strzcpy(path_copy, path, strlen(path)+1);

	if (strlen(path_copy) == 1 && path_copy[0] == '/')
	{
		result[0] = '/';
		result[1] = '\0';
		return result;
	}

	// Strip trailing '/' for no root folders
	if (strlen(path_copy) > 1 && path_copy[strlen(path_copy) - 1] == '/')
	{
		path_copy[strlen(path_copy) - 1] = '\0';
	}

	char *last_slash = strrchr(path_copy, '/');
	if (!last_slash)
		return NULL;

	int new_path_length = strlen(path_copy) - (strlen(last_slash) - 1);
	path_copy[new_path_length] = '\0';

	// Strip a remaining '/' after a filename was removed.
	if (new_path_length > 1 && path_copy[new_path_length - 1] == '/')
	{
		path_copy[new_path_length - 1] = '\0';
	}

	strzcpy(result, path_copy, new_path_length+1);

	return result;
}
#endif

static status_t
add_node(const char *path, uint8_t type, int file_size, void *archive,
		struct node *root_node)
{
	// The root node was already added in init.
	if (strlen(path) == 1 && path[0] == '/')
		return KERNEL_OK;

	char *filename = tarfs_basename(path);

	// Create a new node
	struct node *new_node = malloc(sizeof(struct node));
	if (!new_node)
		return -KERNEL_NO_MEMORY;

	memset(new_node, 0, sizeof(struct node));
	new_node->name_length = strlen(filename)+1;
	strzcpy(new_node->name, filename, new_node->name_length);

	if (type == TMPFS_FOLDER)
	{
		new_node->type = TMPFS_FOLDER;
	}
	else if (type == TMPFS_FILE)
	{
		new_node->type = TMPFS_FILE;
		new_node->u.file.size = file_size;
		new_node->u.file.data = archive - 512;
	}

	// Create a list of node names separated by '/'
	path_nodes_to_list(path);

	struct node *tmp_node, *iter_node;
	struct path_node *path_node_iter;

	tmp_node = root_node;

	STAILQ_FOREACH(path_node_iter, &path_nodes, next)
	{
		if (LIST_EMPTY(&tmp_node->u.folder.nodes))
		{
			LIST_INSERT_HEAD(&(tmp_node->u.folder.nodes), new_node, next);
		}
		else
		{

			LIST_FOREACH(iter_node, &tmp_node->u.folder.nodes, next)
			{
				if (strncmp(path_node_iter->name, iter_node->name,
							strlen(path_node_iter->name)+1) == 0)
				{
					if (iter_node->type == TMPFS_FOLDER)
					{
						tmp_node = iter_node;
						break;
					}
				}
				else
				{
					LIST_INSERT_HEAD(&(tmp_node->u.folder.nodes), new_node, next);
				}
			}
		}
	}

	path_nodes_list_delete();

	return KERNEL_OK;
}

static void
untar(void *ramdisk_address, struct node *root_node)
{
	char buffer[512];
	int file_size;
	status_t status;

	while (1)
	{
		memcpy(buffer, ramdisk_address, 512);
		ramdisk_address += 512;

		if (is_end_of_archive(buffer))
			return;

		if (!checksum(buffer))
		{
			printf("Checksum failure\n");
			return;
		}

		if (buffer[156] == '0' || buffer[156] == '7')
		{
			file_size = parseoct(buffer + 124, 12);

			if (file_size < 512)
				file_size = 512;

			ramdisk_address += file_size;

			char *normalized_path = strchr(buffer, '/');

			status = add_node(normalized_path, TMPFS_FILE,
					file_size, ramdisk_address, root_node);
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
tarfs_mount(const char *root_device, const char *mount_point,
		const char *mount_args, struct superblock **result_rootfs)
{
	void *ramdisk_address = (void *)initrd_start;
	(void)root_device;
	(void)mount_point;
	(void)mount_args;

	struct node *root_node = malloc(sizeof(struct node));

	if (!root_node)
		return -KERNEL_NO_MEMORY;

	root_node->type = TMPFS_FOLDER;
	root_node->name_length = strlen("/")+1;
	strzcpy(root_node->name, "/", root_node->name_length);

	LIST_INIT(&(root_node->u.folder.nodes));
	STAILQ_INIT(&path_nodes);
	untar(ramdisk_address, root_node);

	struct superblock *tarfs_superblock = malloc(sizeof(struct superblock));

	if (!tarfs_superblock)
		return -KERNEL_NO_MEMORY;

	tarfs_superblock->root = root_node;
	*result_rootfs = tarfs_superblock;

	return KERNEL_OK;
}

static status_t
tarfs_umount(void)
{
	return KERNEL_OK;
}

status_t
tarfs_init(paddr_t start, paddr_t end)
{
	strzcpy(tarfs.name, "tarfs", strlen("tarfs")+1);
	tarfs.mount  = tarfs_mount;
	tarfs.umount = tarfs_umount;

	initrd_start = start;
	initrd_end   = end;

	return fs_register(&tarfs);
}


vfs_ops_t
tarfs_ops = {
	.mount    = tarfs_mount,
	.umount   = tarfs_umount,
	.read     = NULL,
	.write    = NULL,
	.truncate = NULL,
	.close    = NULL,
	.destroy  = NULL,
	.sync     = NULL
};
