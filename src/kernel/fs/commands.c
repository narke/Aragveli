#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <lib/c/stdbool.h>
#include <fs/vfs.h>

#include "commands.h"

#define CAT_CHUNK 512
#define CP_CHUNK  4096

static const char *
path_or_dot(const char *path)
{
	return (path && path[0]) ? path : ".";
}

static const char *
path_basename(const char *path)
{
	const char *slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}

int
cmd_ls(const char *cwd, const char *path)
{
	struct dirent *entry;
	struct vnode *vn;
	int ret = 0;
	int status = vfs_lookup(cwd, path_or_dot(path), &vn);

	if (status != KERNEL_OK)
	{
		return status;
	}

	if (vn->type != VFS_NODE_DIRECTORY)
	{
		kprintf("%s\n", path);
		vnode_put(vn);
		return KERNEL_OK;
	}

	entry = malloc(sizeof(struct dirent));

	if (!entry)
	{
		vnode_put(vn);
		return -KERNEL_NO_MEMORY;
	}

	for (uint32_t i = 0; (ret = vfs_readdir(vn, i, entry)) == 1; i++)
	{
		kprintf("%s%s\n", entry->d_name,
			entry->d_type == VFS_NODE_DIRECTORY ? "/" : "");
	}

	free(entry);
	vnode_put(vn);
	return ret < 0 ? ret : KERNEL_OK;
}

int
cmd_pwd(const char *cwd)
{
	if (!cwd)
	{
		return -KERNEL_INVALID_VALUE;
	}

	kprintf("%s\n", cwd);
	return KERNEL_OK;
}

int
cmd_cd(char *cwd, size_t size, const char *path)
{
	struct vnode *vn;
	char *abs_path;
	int status;

	if (!cwd)
	{
		return -KERNEL_INVALID_VALUE;
	}

	if (!path || path[0] == '\0')
	{
		return KERNEL_OK;
	}

	abs_path = malloc(PATH_MAX);

	if (!abs_path)
	{
		return -KERNEL_NO_MEMORY;
	}

	status = vfs_normalize(cwd, path, abs_path, PATH_MAX);

	if (status == KERNEL_OK)
	{
		status = vfs_lookup("/", abs_path, &vn);
	}

	if (status == KERNEL_OK)
	{
		if (vn->type == VFS_NODE_DIRECTORY)
		{
			strzcpy(cwd, abs_path, size);
		}
		else
		{
			status = -KERNEL_NO_SUCH_FILE_OR_FOLDER;
		}

		vnode_put(vn);
	}

	if (status != KERNEL_OK)
	{
		kprintf("cd: no such directory\n");
	}

	free(abs_path);
	return status;
}

int
cmd_mkdir(const char *cwd, const char *path)
{
	return vfs_create(cwd, path, VFS_NODE_DIRECTORY);
}

int
cmd_touch(const char *cwd, const char *path)
{
	return vfs_create(cwd, path, VFS_NODE_REGULAR_FILE);
}

int
cmd_rm(const char *cwd, const char *path)
{
	return vfs_remove(cwd, path, VFS_NODE_REGULAR_FILE);
}

int
cmd_rmdir(const char *cwd, const char *path)
{
	return vfs_remove(cwd, path, VFS_NODE_DIRECTORY);
}

int
cmd_cat(const char *cwd, const char *path)
{
	struct vnode *vn;
	char *buf;
	loff_t offset = 0;
	ssize_t n = 0;
	int status = vfs_lookup(cwd, path, &vn);

	if (status != KERNEL_OK)
	{
		return status;
	}

	if (vn->type != VFS_NODE_REGULAR_FILE)
	{
		vnode_put(vn);
		return -KERNEL_NO_SUCH_FILE_OR_FOLDER;
	}

	buf = malloc(CAT_CHUNK);

	if (!buf)
	{
		vnode_put(vn);
		return -KERNEL_NO_MEMORY;
	}

	while ((n = vfs_read(vn, buf, CAT_CHUNK, offset)) > 0)
	{
		for (ssize_t i = 0; i < n; i++)
		{
			kprintf("%c", buf[i]);
		}

		offset += n;
	}

	kprintf("\n");
	free(buf);
	vnode_put(vn);
	return n < 0 ? (int)n : KERNEL_OK;
}

int
cmd_file(const char *cwd, const char *path)
{
	struct vnode *vn;
	int status = vfs_lookup(cwd, path, &vn);

	if (status != KERNEL_OK)
	{
		return status;
	}

	kprintf("%s: %s\n", path_basename(path),
		vn->type == VFS_NODE_DIRECTORY ? "folder" : "file");
	vnode_put(vn);
	return KERNEL_OK;
}

/* Absolute target of mv/cp: dst itself, or dst/basename(src) if dst is a folder. */
static int
target_path(const char *cwd, const char *src, const char *dst,
		char *out, size_t size)
{
	struct vnode *vn;
	char *src_abs = malloc(PATH_MAX);
	int status;

	if (!src_abs)
	{
		return -KERNEL_NO_MEMORY;
	}

	status = vfs_normalize(cwd, src, src_abs, PATH_MAX);

	if (status == KERNEL_OK)
	{
		status = vfs_normalize(cwd, dst, out, size);
	}

	if (status == KERNEL_OK && vfs_lookup("/", out, &vn) == KERNEL_OK)
	{
		if (vn->type == VFS_NODE_DIRECTORY)
		{
			const char *base = path_basename(src_abs);
			size_t len = strnlen(out, size);
			size_t base_len = strnlen(base, NAME_MAX + 1);
			size_t sep = len > 1 ? 1 : 0;

			if (len + sep + base_len >= size)
			{
				status = -KERNEL_INVALID_VALUE;
			}
			else
			{
				if (sep)
				{
					out[len++] = '/';
				}

				strzcpy(out + len, base, size - len);
			}
		}

		vnode_put(vn);
	}

	free(src_abs);
	return status;
}

static int
copy_file(struct vnode *src, const char *dst_path)
{
	struct vnode *dst;
	char *buf;
	loff_t offset = 0;
	ssize_t n = 0;
	int status = vfs_create("/", dst_path, VFS_NODE_REGULAR_FILE);

	if (status != KERNEL_OK)
	{
		return status;
	}

	status = vfs_lookup("/", dst_path, &dst);

	if (status != KERNEL_OK)
	{
		return status;
	}

	buf = malloc(CP_CHUNK);

	if (!buf)
	{
		status = -KERNEL_NO_MEMORY;
	}

	while (status == KERNEL_OK
		&& (n = vfs_read(src, buf, CP_CHUNK, offset)) > 0)
	{
		ssize_t written = vfs_write(dst, buf, (size_t)n, offset);

		/* A short write means the destination volume is full. */
		if (written != n)
		{
			status = written < 0 ? (int)written : -KERNEL_NO_MEMORY;
		}

		offset += n;
	}

	if (status == KERNEL_OK && n < 0)
	{
		status = (int)n;
	}

	free(buf);
	vnode_put(dst);

	if (status != KERNEL_OK)
	{
		vfs_remove("/", dst_path, VFS_NODE_REGULAR_FILE);
	}

	return status;
}

int
cmd_cp(const char *cwd, const char *src_path, const char *dst_path)
{
	struct vnode *src;
	char *dst;
	int status;

	if (!src_path || !dst_path)
	{
		return -KERNEL_INVALID_VALUE;
	}

	dst = malloc(PATH_MAX);

	if (!dst)
	{
		return -KERNEL_NO_MEMORY;
	}

	status = target_path(cwd, src_path, dst_path, dst, PATH_MAX);

	if (status == KERNEL_OK)
	{
		status = vfs_lookup(cwd, src_path, &src);
	}

	if (status == KERNEL_OK)
	{
		if (src->type == VFS_NODE_DIRECTORY)
		{
			status = vfs_create("/", dst, VFS_NODE_DIRECTORY);
		}
		else
		{
			status = copy_file(src, dst);
		}

		vnode_put(src);
	}

	free(dst);
	return status;
}

int
cmd_mv(const char *cwd, const char *src_path, const char *dst_path)
{
	struct vnode *src;
	char *dst;
	int status;

	if (!src_path || !dst_path)
	{
		return -KERNEL_INVALID_VALUE;
	}

	dst = malloc(PATH_MAX);

	if (!dst)
	{
		return -KERNEL_NO_MEMORY;
	}

	status = target_path(cwd, src_path, dst_path, dst, PATH_MAX);

	if (status == KERNEL_OK)
	{
		status = vfs_rename(cwd, src_path, dst);
	}

	/* Across mounts: copy, then delete the source (files only). */
	if (status == -KERNEL_OPERATION_NOT_SUPPORTED
		&& vfs_lookup(cwd, src_path, &src) == KERNEL_OK)
	{
		if (src->type == VFS_NODE_REGULAR_FILE)
		{
			status = copy_file(src, dst);
		}

		vnode_put(src);

		if (status == KERNEL_OK)
		{
			status = vfs_remove(cwd, src_path, VFS_NODE_REGULAR_FILE);
		}
	}

	free(dst);
	return status;
}
