#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <lib/c/stdbool.h>
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
		return cwd;

	if (path[0] == '/')
	{
		if (!root)
			return NULL;
		return resolve_node(path, root);
	}

	if (!cwd)
		return NULL;

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
	char copy[FS_PATH_MAX];
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

	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (is_directory(folder_node))
	{
		LIST_FOREACH(tmp_node, &folder_node->u.folder.nodes, next)
			kprintf("%s\n", tmp_node->name);
	}
	else
	{
		kprintf("%s\n", folder_node->name);
	}

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
		kprintf("cd: no such directory\n");
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	*cwd = folder_node;
	return KERNEL_OK;
}

static int
cmd_create(struct node *root, struct node *cwd, const char *path,
		uint8_t type)
{
	char dir[FS_PATH_MAX];
	char base[FS_PATH_MAX];
	struct node *parent;
	struct node *new_node;

	if (!path || fs_split_path(path, dir, base, FS_PATH_MAX) != 0)
		return -KERNEL_INVALID_VALUE;

	parent = resolve_node_wrapper(dir, root, cwd);

	if (!parent || !is_directory(parent))
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (name_exists(parent, base))
		return -KERNEL_FILE_ALREADY_EXISTS;

	new_node = malloc(sizeof(struct node));

	if (!new_node)
		return -KERNEL_NO_MEMORY;

	memset(new_node, 0, sizeof(struct node));
	new_node->name_length = strnlen(base, NODE_NAME_LENGTH) + 1;
	strzcpy(new_node->name, base, new_node->name_length);
	new_node->type = type;

	if (type == TMPFS_FOLDER)
		LIST_INIT(&new_node->u.folder.nodes);
	else
		new_node->u.file.size = 0;

	LIST_INSERT_HEAD(&parent->u.folder.nodes, new_node, next);
	return KERNEL_OK;
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

int
cmd_rm(struct node *root, struct node *cwd, const char *path)
{
	struct node *node;

	node = resolve_node_wrapper(path, root, cwd);

	if (!node || node->type != TMPFS_FILE)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	LIST_REMOVE(node, next);
	free(node);
	return KERNEL_OK;
}

int
cmd_rmdir(struct node *root, struct node *cwd, const char *path)
{
	struct node *node;

	node = resolve_node_wrapper(path, root, cwd);

	if (!node || !is_directory(node))
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	LIST_REMOVE(node, next);
	free(node);
	return KERNEL_OK;
}

int
cmd_cat(struct node *root, struct node *cwd, const char *path)
{
	struct node *node;
	const uint8_t *data;
	uint64_t i;

	node = resolve_node_wrapper(path, root, cwd);

	if (!node || node->type != TMPFS_FILE)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	data = (const uint8_t *)node->u.file.data;

	for (i = 0; data && i < node->u.file.size; i++)
		kprintf("%c", data[i]);

	kprintf("\n");
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

	return KERNEL_OK;
}

static int
mv_cp_internal(struct node *root, struct node *cwd,
		const char *src_path, const char *dst_path, bool is_mv)
{
	struct node *src_node;
	struct node *dst_dir;
	struct node *new_node = NULL;
	char dir[FS_PATH_MAX];
	char base[FS_PATH_MAX];
	const char *new_name;

	src_node = resolve_node_wrapper(src_path, root, cwd);

	if (!src_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	/* Destination may be a directory (keep basename) or a new path. */
	dst_dir = resolve_node_wrapper(dst_path, root, cwd);

	if (dst_dir && is_directory(dst_dir))
	{
		new_name = src_node->name;
	}
	else
	{
		if (fs_split_path(dst_path, dir, base, FS_PATH_MAX) != 0)
			return -KERNEL_INVALID_VALUE;

		dst_dir = resolve_node_wrapper(dir, root, cwd);
		if (!dst_dir || !is_directory(dst_dir))
			return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

		new_name = base;
	}

	struct node *n;

	LIST_FOREACH(n, &dst_dir->u.folder.nodes, next)
	{
		if (n != src_node
		    && !strncmp(n->name, new_name, NODE_NAME_LENGTH))
			return -KERNEL_FILE_ALREADY_EXISTS;
	}

	if (!is_mv)
	{
		new_node = malloc(sizeof(struct node));

		if (!new_node)
			return -KERNEL_NO_MEMORY;

		memset(new_node, 0, sizeof(struct node));
		new_node->type = src_node->type;
		new_node->name_length = strnlen(new_name, NODE_NAME_LENGTH) + 1;
		strzcpy(new_node->name, new_name, new_node->name_length);

		if (src_node->type == TMPFS_FILE)
		{
			new_node->u.file.size = src_node->u.file.size;
			new_node->u.file.data = src_node->u.file.data;
		}
		else
		{
			LIST_INIT(&new_node->u.folder.nodes);
		}

		LIST_INSERT_HEAD(&dst_dir->u.folder.nodes, new_node, next);
		return KERNEL_OK;
	}

	LIST_REMOVE(src_node, next);
	strzcpy(src_node->name, new_name, NODE_NAME_LENGTH);
	src_node->name_length = strnlen(new_name, NODE_NAME_LENGTH) + 1;
	LIST_INSERT_HEAD(&dst_dir->u.folder.nodes, src_node, next);
	return KERNEL_OK;
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
