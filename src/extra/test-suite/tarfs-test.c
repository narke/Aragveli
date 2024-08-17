#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include "tarfs-test.h"

static struct node *
resolve_node_wrapper(const char *path, struct node *root, struct node *cwd)
{
	struct node *folder_node;

	if (strnlen(path, NODE_NAME_LENGTH) > 0 && path[0] == '/')
	{
		if (!root)
			return NULL;

		folder_node = resolve_node(path, root);
	}
	else
	{
		if (!cwd)
			return NULL;

		folder_node = resolve_node(path, cwd);
	}

	if (!folder_node)
		return NULL;

	return folder_node;
}

int
cmd_ls(struct node *root, struct node *cwd, const char *path)
{
	struct node *tmp_node = NULL;
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (folder_node->type == TARFS_DIRECTORY)
	{
		LIST_FOREACH(tmp_node, &folder_node->u.folder.nodes, next)
		{
			printf("%s\n", tmp_node->name);
		}
	}
	else
		printf("%s\n", tmp_node->name);

	return KERNEL_OK;
}

int
cmd_pwd(struct node *cwd)
{
	printf("%s\n", cwd->name);

	return KERNEL_OK;
}

int
cmd_cd(struct node *cwd, const char *path)
{
	if (cwd->type == TARFS_FILE)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	struct node *folder_node = resolve_node(path, cwd);
	if (!folder_node)
	{
		printf("cd error\n");
	}
	printf("cd: %s\n", folder_node->name);
	// TODO update cwd

	return KERNEL_OK;
}

int
cmd_mkdir(struct node *root, struct node *cwd, const char *path, const char *folder_name)
{
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	// Create a new node
	struct node *new_node = malloc(sizeof(struct node));
	if (!new_node)
		return -KERNEL_NO_MEMORY;

	memset(new_node, 0, sizeof(struct node));
	new_node->name_length = strnlen(folder_name, NODE_NAME_LENGTH)+1;
	strzcpy(new_node->name, folder_name, new_node->name_length);
	new_node->type = TMPFS_FOLDER;

	LIST_INSERT_HEAD(&(folder_node->u.folder.nodes), new_node, next);

	return KERNEL_OK;
}

int
cmd_touch(struct node *root, struct node *cwd, const char *path, const char *filename)
{
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	// Create a new node
	struct node *new_node = malloc(sizeof(struct node));
	if (!new_node)
		return -KERNEL_NO_MEMORY;

	memset(new_node, 0, sizeof(struct node));
	new_node->name_length = strnlen(filename, NODE_NAME_LENGTH)+1;
	strzcpy(new_node->name, filename, new_node->name_length);
	new_node->type = TMPFS_FILE;
	new_node->u.file.size = 0;
	//new_node->u.file.data = 0;

	LIST_INSERT_HEAD(&(folder_node->u.folder.nodes), new_node, next);

	return KERNEL_OK;
}

int
cmd_rm(struct node *root, struct node *cwd, const char *path, const char *filename)
{
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	struct node* tmp_node;
	LIST_FOREACH(tmp_node, &(folder_node->u.folder.nodes), next)
	{
		if (!strncmp(tmp_node->name, filename, strnlen(filename, NODE_NAME_LENGTH))
				&& tmp_node->type == TMPFS_FILE)
		{
			LIST_REMOVE(tmp_node, next);
			free(tmp_node);
			return KERNEL_OK;
		}
	}

	return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
}

int
cmd_rmdir(struct node *root, struct node *cwd, const char *path, const char *folder_name)
{
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	struct node* tmp_node;
	LIST_FOREACH(tmp_node, &(folder_node->u.folder.nodes), next)
	{
		if (!strncmp(tmp_node->name, folder_name, strnlen(folder_name, NODE_NAME_LENGTH))
				&& tmp_node->type == TMPFS_FOLDER)
		{
			LIST_REMOVE(tmp_node, next);
			free(tmp_node);
			return KERNEL_OK;
		}
	}

	return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

}

int
cmd_cat(struct node *root, struct node *cwd, const char *path, const char *filename)
{
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	struct node* tmp_node;
	LIST_FOREACH(tmp_node, &(folder_node->u.folder.nodes), next)
	{
		if (!strncmp(tmp_node->name, filename, strnlen(filename, NODE_NAME_LENGTH))
				&& tmp_node->type == TMPFS_FILE)
		{
			for (uint64_t i = 0; i < tmp_node->u.file.size; i++)
				printf("%c", tmp_node->u.file.data[i]);
			printf("\n");
			return KERNEL_OK;
		}
	}

	return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
}

int
cmd_file(struct node *root, struct node *cwd, const char *path, const char *nodename)
{
	struct node *folder_node = resolve_node_wrapper(path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	struct node* tmp_node;
	LIST_FOREACH(tmp_node, &(folder_node->u.folder.nodes), next)
	{
		if (!strncmp(tmp_node->name, nodename, strnlen(nodename, NODE_NAME_LENGTH)))
		{
			if (tmp_node->type == TMPFS_FILE)
				printf("%s: file\n", tmp_node->name);
			else
				printf("%s: folder\n", tmp_node->name);
			return KERNEL_OK;
		}
	}

	return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
}

static int
mv_cp_internal(struct node *root, struct node *cwd,
		const char *src_path,
		const char *src_node_name,
		const char *dst_path,
		const char *dst_node_name,
		bool is_mv)
{
	bool src_node_found = false;

	// Source
	struct node *folder_node = resolve_node_wrapper(src_path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	struct node* src_node;
	LIST_FOREACH(src_node, &(folder_node->u.folder.nodes), next)
	{
		if (!strncmp(src_node->name, src_node_name, strnlen(src_node_name, NODE_NAME_LENGTH)))
		{
			src_node_found = true;
			break;
		}
	}

	if (!src_node_found)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	// New node
	struct node *new_node = malloc(sizeof(struct node));
	if (!new_node)
		return -KERNEL_NO_MEMORY;

	if (!is_mv)
	{
		memset(new_node, 0, sizeof(struct node));
		new_node->type = src_node->type;

		if (dst_node_name)
		{
			new_node->name_length = strnlen(dst_node_name, NODE_NAME_LENGTH)+1;
			strzcpy(new_node->name, dst_node_name, strnlen(dst_node_name, NODE_NAME_LENGTH)+1);
		}
		else
		{
			new_node->name_length = strnlen(src_node_name, NODE_NAME_LENGTH)+1;
			strzcpy(new_node->name, src_node_name, strnlen(src_node_name, NODE_NAME_LENGTH)+1);
		}
	}

	// Destination
	folder_node = resolve_node_wrapper(dst_path, root, cwd);
	if (!folder_node)
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

	if (dst_node_name)
	{
		struct node* dst_node;
		LIST_FOREACH(dst_node, &(folder_node->u.folder.nodes), next)
		{
			if (!strncmp(dst_node->name, dst_node_name, strnlen(dst_node_name, NODE_NAME_LENGTH)))
			{
				return -KERNEL_FILE_ALREADY_EXISTS;
			}
		}
	}

	if (is_mv && dst_node_name)
		strzcpy(src_node->name, dst_node_name, strnlen(dst_node_name, NODE_NAME_LENGTH)+1);

	if (is_mv)
	{
		// Remove the source node
		LIST_REMOVE(src_node, next);
		// And insert into the destination node
		LIST_INSERT_HEAD(&(folder_node->u.folder.nodes), src_node, next);
	}
	else
	{
		LIST_INSERT_HEAD(&(folder_node->u.folder.nodes), new_node, next);
	}

	return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
}

int
cmd_mv(struct node *root, struct node *cwd,
		const char *src_path,
		const char *src_node_name,
		const char *dst_path,
		const char *dst_node_name)
{
	return mv_cp_internal(root, cwd, src_path, src_node_name,
			dst_path, dst_node_name, true);
}

int
cmd_cp(struct node *root, struct node *cwd,
		const char *src_path,
		const char *src_node_name,
		const char *dst_path,
		const char *dst_node_name)
{
	return mv_cp_internal(root, cwd, src_path, src_node_name,
			dst_path, dst_node_name, false);
}

void
tarfs_test(struct node *root)
{
	//cmd_ls(root, root, "/");
	//cmd_pwd(root);
	//cmd_cd(root, "docs");
	//cmd_mkdir(root, root, "/", "newfolder");
	//cmd_touch(root, root, "/", "newfile");
	//cmd_rm(root, root, "/", "newfile");
	//cmd_rmdir(root, root, "/", "newfolder");
	cmd_cat(root, root, "/", "test.txt");
	//cmd_file(root, root, "/", "test.txt");
	//cmd_mv(root, root, "/", "test.txt", "/", "newmv.txt");
	//cmd_cp(root, root, "/", "test.txt", "/", "newcp.txt");
	//cmd_ls(root, root, "/");

	/*cmd_mv(root, root, "/", "test.txt", "/docs", NULL);
	cmd_ls(root, root, "/docs");
	cmd_ls(root, root, "/");*/
}
