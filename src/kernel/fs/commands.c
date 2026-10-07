#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <lib/c/stdbool.h>

#include "tarfs.h"
#include "commands.h"

#define FS_PATH_MAX 256

static int
is_directory(struct node *n)
{
	return n && n->type == TMPFS_FOLDER;
}

static struct node *
resolve_node_wrapper(const char *path, struct node *root, struct node *cwd)
{
	if (!path || path[0] == '\0' || (path[0] == '.' && path[1] == '\0'))
	{
		return node_get(cwd);
	}

	if (path[0] == '/')
	{
		if (!root)
			return NULL;
		return resolve_node(path, root);
	}

	if (!cwd)
	{
		return NULL;
	}

	return resolve_node(path, cwd);
}

/*
 * Split path into parent directory and basename.
 * "test.txt" -> ".", "test.txt"
 * "/file"    -> "/", "file"
 * "/docs/a"  -> "/docs", "a"
 * "foo/bar"  -> "foo", "bar"
 */
static int
fs_split_path(const char *path, char *dir, char *base, size_t n)
{
	/* Length-validated against n and FS_PATH_MAX before any copy. */
	char copy[FS_PATH_MAX]; /* Flawfinder: ignore */
	size_t len;
	char *slash;

	if (!path || !dir || !base || n == 0)
		return -1;

	len = strnlen(path, FS_PATH_MAX);

	if (len == 0 || len >= n || len >= FS_PATH_MAX)
		return -1;

	strzcpy(copy, path, len + 1);

	if (len > 1 && copy[len - 1] == '/')
	{
		copy[len - 1] = '\0';
		len--;
	}

	if (len == 1 && copy[0] == '/')
	{
		strzcpy(dir, "/", 2);
		strzcpy(base, "/", 2);
		return 0;
	}

	slash = strrchr(copy, '/');

	if (!slash)
	{
		strzcpy(dir, ".", 2);
		strzcpy(base, copy, n);
		return 0;
	}

	if (slash == copy)
	{
		strzcpy(dir, "/", 2);
		strzcpy(base, slash + 1, n);
		return 0;
	}

	*slash = '\0';
	strzcpy(dir, copy, n);
	strzcpy(base, slash + 1, n);
	return 0;
}

static int
name_exists(struct node *dir, const char *name)
{
	struct node *n;

	LIST_FOREACH(n, &dir->u.folder.nodes, next)
	{
		if (!strncmp(n->name, name, NODE_NAME_LENGTH))
			return 1;
	}

	return 0;
}

int
cmd_ls(struct node *root, struct node *cwd, const char *path)
{
	struct node *tmp_node;
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	uint32_t flags;

	if (!folder_node)
	{
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	if (is_directory(folder_node))
	{
		flags = spinlock_lock_irqsave(&folder_node->lock);

		LIST_FOREACH(tmp_node, &folder_node->u.folder.nodes, next)
		{
			kprintf("%s\n", tmp_node->name);
		}

		spinlock_unlock_irqrestore(&folder_node->lock, flags);
	}
	else
	{
		kprintf("%s\n", folder_node->name);
	}

	node_put(folder_node);

	return KERNEL_OK;
}

int
cmd_pwd(struct node *cwd)
{
	if (!cwd)
		return -KERNEL_INVALID_VALUE;

	kprintf("%s\n", cwd->name);
	return KERNEL_OK;
}

int
cmd_cd(struct node *root, struct node **cwd, const char *path)
{
	struct node *folder_node;

	if (!cwd || !*cwd)
		return -KERNEL_INVALID_VALUE;

	if (!path || path[0] == '\0')
		return KERNEL_OK;

	folder_node = resolve_node_wrapper(path, root, *cwd);

	if (!folder_node || !is_directory(folder_node))
	{
		node_put(folder_node);
		kprintf("cd: no such directory\n");
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	struct node *old = *cwd;
	*cwd = folder_node;
	node_put(old);
	return KERNEL_OK;
}

static int
cmd_create(struct node *root, struct node *cwd, const char *path,
		uint8_t type)
{
	/* Written only by fs_split_path(), bounded by the n argument. */
	char dir[FS_PATH_MAX]; /* Flawfinder: ignore */
	char base[FS_PATH_MAX]; /* Flawfinder: ignore */
	struct node *parent;
	struct node *new_node;

	if (!path || fs_split_path(path, dir, base, FS_PATH_MAX) != 0)
		return -KERNEL_INVALID_VALUE;

	parent = resolve_node_wrapper(dir, root, cwd);

	if (!is_directory(parent))
	{
		node_put(parent);
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	new_node = malloc(sizeof(struct node));

	if (!new_node)
	{
		node_put(parent);
		return -KERNEL_NO_MEMORY;
	}

	memset(new_node, 0, sizeof(struct node));
	strzcpy(new_node->name, base, sizeof(new_node->name));
	new_node->name_length = strnlen(new_node->name, NODE_NAME_LENGTH) + 1;
	new_node->type = type;
	new_node->refcount = 1;

	if (type == TMPFS_FOLDER)
		LIST_INIT(&new_node->u.folder.nodes);

	uint32_t flags = spinlock_lock_irqsave(&tarfs_ns_lock);
	int status;

	if (parent->unlinked)
	{
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}
	else if (name_exists(parent, base))
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

	node_put(parent);
	return status;
}

int
cmd_mkdir(struct node *root, struct node *cwd, const char *path)
{
	return cmd_create(root, cwd, path, TMPFS_FOLDER);
}

int
cmd_touch(struct node *root, struct node *cwd, const char *path)
{
	return cmd_create(root, cwd, path, TMPFS_FILE);
}

static int
folder_unlink(struct node *root, struct node *cwd, const char *path,
		uint8_t type)
{
	char dir[FS_PATH_MAX]; /* Flawfinder: ignore */
	char base[FS_PATH_MAX]; /* Flawfinder: ignore */
	struct node *parent;
	struct node *node;
	uint32_t flags;
	int status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (!path || fs_split_path(path, dir, base, FS_PATH_MAX) != 0)
	{
		return -KERNEL_INVALID_VALUE;
	}

	parent = resolve_node_wrapper(dir, root, cwd);

	if (!is_directory(parent))
	{
		node_put(parent);
		return status;
	}

	flags = spinlock_lock_irqsave(&tarfs_ns_lock);

	LIST_FOREACH(node, &parent->u.folder.nodes, next)
	{
		if (strncmp(node->name, base, NODE_NAME_LENGTH) != 0)
		{
			continue;
		}

		if (node->type != type)
		{
			break;
		}

		if (type == TMPFS_FOLDER && !LIST_EMPTY(&node->u.folder.nodes))
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

	node_put(parent);
	return status;
}

int
cmd_rm(struct node *root, struct node *cwd, const char *path)
{
	return folder_unlink(root, cwd, path, TMPFS_FILE);
}

int
cmd_rmdir(struct node *root, struct node *cwd, const char *path)
{
	return folder_unlink(root, cwd, path, TMPFS_FOLDER);
}

int
cmd_cat(struct node *root, struct node *cwd, const char *path)
{
	struct node *node;
	const uint8_t *data;
	uint64_t i;

	node = resolve_node_wrapper(path, root, cwd);

	if (!node || node->type != TMPFS_FILE)
	{
		node_put(node);
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	data = (const uint8_t *)node->u.file.data;

	for (i = 0; data && i < node->u.file.size; i++)
		kprintf("%c", data[i]);

	kprintf("\n");
	node_put(node);
	return KERNEL_OK;
}

int
cmd_file(struct node *root, struct node *cwd, const char *path)
{
	struct node *node;

	node = resolve_node_wrapper(path, root, cwd);

	if (!node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (node->type == TMPFS_FILE)
		kprintf("%s: file\n", node->name);
	else
		kprintf("%s: folder\n", node->name);

	node_put(node);
	return KERNEL_OK;
}

static int
mv_cp_internal(struct node *root, struct node *cwd,
		const char *src_path, const char *dst_path, bool is_mv)
{
	/* Written only by fs_split_path(), bounded by the n argument. */
	char src_dir[FS_PATH_MAX]; /* Flawfinder: ignore */
	char src_base[FS_PATH_MAX]; /* Flawfinder: ignore */
	char dir[FS_PATH_MAX]; /* Flawfinder: ignore */
	char base[FS_PATH_MAX]; /* Flawfinder: ignore */
	struct node *src_parent;
	struct node *src_node = NULL;
	struct node *dst_dir = NULL;
	struct node *new_node = NULL;
	const char *new_name;
	struct node *n;
	uint32_t flags;
	int status = KERNEL_OK;

	if (!src_path || !dst_path
		|| fs_split_path(src_path, src_dir, src_base, FS_PATH_MAX) != 0)
	{
		return -KERNEL_INVALID_VALUE;
	}

	if (!is_mv)
	{
		new_node = malloc(sizeof(struct node));
		if (!new_node)
			return -KERNEL_NO_MEMORY;
	}

	flags = spinlock_lock_irqsave(&tarfs_ns_lock);

	src_parent = resolve_node_wrapper(src_dir, root, cwd);

	if (is_directory(src_parent))
	{
		src_node = resolve_node(src_base, src_parent);
	}

	if (!src_node)
	{
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
		goto out;
	}

	/* Destination may be a directory (keep basename) or a new path. */
	dst_dir = resolve_node_wrapper(dst_path, root, cwd);

	if (is_directory(dst_dir))
	{
		new_name = src_node->name;
	}
	else
	{
		node_put(dst_dir);
		dst_dir = NULL;

		if (fs_split_path(dst_path, dir, base, FS_PATH_MAX) != 0)
		{
			status = -KERNEL_INVALID_VALUE;
			goto out;
		}

		dst_dir = resolve_node_wrapper(dir, root, cwd);

		if (!is_directory(dst_dir))
		{
			status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
			goto out;
		}

		new_name = base;
	}

	if (dst_dir->unlinked)
	{
		status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
		goto out;
	}

	LIST_FOREACH(n, &dst_dir->u.folder.nodes, next)
	{
		if (n != src_node
		    && !strncmp(n->name, new_name, NODE_NAME_LENGTH))
		{
			status = -KERNEL_FILE_ALREADY_EXISTS;
			goto out;
		}
	}

	if (!is_mv)
	{
		memset(new_node, 0, sizeof(struct node));
		new_node->refcount = 1;
		new_node->type = src_node->type;
		strzcpy(new_node->name, new_name, sizeof(new_node->name));
		new_node->name_length = strnlen(new_node->name, NODE_NAME_LENGTH) + 1;

		if (src_node->type == TMPFS_FILE)
		{
			new_node->u.file.size = src_node->u.file.size;
			new_node->u.file.data = src_node->u.file.data;
		}
		else
		{
			LIST_INIT(&new_node->u.folder.nodes);
		}

		spinlock_lock(&dst_dir->lock);
		LIST_INSERT_HEAD(&dst_dir->u.folder.nodes, new_node, next);
		spinlock_unlock(&dst_dir->lock);

		new_node = NULL;
		goto out;
	}

	spinlock_lock(&src_parent->lock);
	LIST_REMOVE(src_node, next);
	spinlock_unlock(&src_parent->lock);

	if (new_name != src_node->name)
	{
		strzcpy(src_node->name, new_name, NODE_NAME_LENGTH);
		src_node->name_length = strnlen(new_name, NODE_NAME_LENGTH) + 1;
	}

	spinlock_lock(&dst_dir->lock);
	LIST_INSERT_HEAD(&dst_dir->u.folder.nodes, src_node, next);
	spinlock_unlock(&dst_dir->lock);

out:
	spinlock_unlock_irqrestore(&tarfs_ns_lock, flags);

	node_put(dst_dir);
	node_put(src_node);
	node_put(src_parent);
	free(new_node);

	return status;
}

int
cmd_mv(struct node *root, struct node *cwd, const char *src_path,
		const char *dst_path)
{
	return mv_cp_internal(root, cwd, src_path, dst_path, true);
}

int
cmd_cp(struct node *root, struct node *cwd, const char *src_path,
		const char *dst_path)
{
	return mv_cp_internal(root, cwd, src_path, dst_path, false);
}
