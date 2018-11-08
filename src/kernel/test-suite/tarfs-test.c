#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include "tarfs-test.h"

int
cmd_ls(struct dentry *root, struct dentry *cwd, char *path)
{
	struct dentry *tmp_node = NULL, *start_root;

	if (!cwd && !root)
		return -KERNEL_NO_SUCH_FILE_OR_DIRECTORY;

	if (strlen(path) > 0 && path[0] == '/')
		start_root = resolve_node(path, root);
	else
		start_root = resolve_node(path, cwd);

	if (!start_root)
		return -KERNEL_NO_SUCH_FILE_OR_DIRECTORY;

	if (start_root->type == TARFS_DIRECTORY)
	{
		LIST_FOREACH(tmp_node, &start_root->u.dir.nodes, next)
		{
			printf("%s\n", tmp_node->name);
		}
	}
	else if (start_root->type == TARFS_FILE)
		printf("%s\n", tmp_node->name);
	else
		return -KERNEL_NO_SUCH_FILE_OR_DIRECTORY;

	return KERNEL_OK;
}

int
cmd_pwd(struct dentry *cwd)
{
	printf("%s\n", cwd->name);

	return KERNEL_OK;
}

int
cmd_cd(struct dentry *cwd, const char *dst_direcrory_name)
{
	struct dentry *iter = NULL;

	if (cwd->type == TARFS_FILE)
		return -KERNEL_NO_SUCH_FILE_OR_DIRECTORY;

	LIST_FOREACH(iter, &cwd->u.dir.nodes, next)
	{
		if (strncmp(iter->name, dst_direcrory_name, strlen(dst_direcrory_name)))
		{
			printf("FIXME: cd matched!\n");
			return KERNEL_OK;
		}
	}

	return -KERNEL_NO_SUCH_FILE_OR_DIRECTORY;
}

void
tarfs_test(struct dentry *root)
{
	cmd_ls(root, root, "/");
	cmd_pwd(root);
	cmd_cd(root, "docs");
}
