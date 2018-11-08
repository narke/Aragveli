#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include "tarfs-test.h"

int
cmd_ls(struct node *root, struct node *cwd, char *path)
{
	struct node *tmp_node = NULL, *start_root;

	if (!cwd && !root)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (strlen(path) > 0 && path[0] == '/')
		start_root = resolve_node(path, root);
	else
		start_root = resolve_node(path, cwd);

	if (!start_root)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (start_root->type == TARFS_DIRECTORY)
	{
		LIST_FOREACH(tmp_node, &start_root->u.folder.nodes, next)
		{
			printf("%s\n", tmp_node->name);
		}
	}
	else if (start_root->type == TARFS_FILE)
		printf("%s\n", tmp_node->name);
	else
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	return KERNEL_OK;
}

int
cmd_pwd(struct node *cwd)
{
	printf("%s\n", cwd->name);

	return KERNEL_OK;
}

int
cmd_cd(struct node *cwd, const char *dst_folder_name)
{
	struct node *iter = NULL;

	if (cwd->type == TARFS_FILE)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	LIST_FOREACH(iter, &cwd->u.folder.nodes, next)
	{
		if (strncmp(iter->name, dst_folder_name, strlen(dst_folder_name)))
		{
			printf("FIXME: cd matched!\n");
			return KERNEL_OK;
		}
	}

	return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
}

void
tarfs_test(struct node *root)
{
	cmd_ls(root, root, "/");
	cmd_pwd(root);
	cmd_cd(root, "docs");
}
