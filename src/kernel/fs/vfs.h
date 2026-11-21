/*
 * Copyright (c) 2015-2026 Konstantin Tcholokachvili.
 * Copyright (c) 2009 Martin Decky.
 * Copyright (c) 2008 Jakub Jermar.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <arch/x86/atomic.h>
#include <arch/x86/spinlock.h>
#include <lib/c/stdbool.h>
#include <lib/queue.h>
#include <lib/types.h>
#include <lib/status.h>

#define FS_NAME_MAXLEN  20

#define PATH_MAX 1024
#define NAME_MAX 255
#define NODE_NAME_LENGTH 50

typedef uint16_t dev_t;
typedef uint32_t ino_t;
typedef uint16_t mode_t;
typedef uint8_t  nlink_t;
typedef int16_t  uid_t;
typedef int16_t  gid_t;
typedef int32_t  off_t;
typedef int64_t  loff_t;
typedef uint64_t time_t;

/* tarfs in-memory tree */
struct folder
{
	struct node *parent;
	LIST_HEAD(, node) nodes;
};

struct file
{
	char *data;
	uint32_t size; // Limited to 4 GiB
	bool heap;     // data was malloc'ed, not mapped from the initrd
};

struct node
{
	uint8_t type;
	size_t name_length;
	char name[NODE_NAME_LENGTH]; /* Flawfinder: ignore */
	union
	{
		struct folder folder;
		struct file   file;
	} u;
	atomic_count_t	refcount;	/* +1 for the link in the parent folder */
	spinlock_t	lock;		/* folders: u.folder.nodes against lookups */
	bool		unlinked;	/* protected by tarfs_ns_lock */
	LIST_ENTRY(node) next;
};

typedef enum
{
	VFS_NODE_REGULAR_FILE = 0x42,
	VFS_NODE_DIRECTORY    = 0x24,
	VFS_NODE_SYMLINK      = 0x84,
	VFS_NODE_DEVICE_CHAR  = 0x48,
} vfs_node_type_t;

struct dirent
{
	vfs_node_type_t d_type;
	uint64_t        d_size;
	char            d_name[NAME_MAX + 1]; /* Flawfinder: ignore */
};

struct vnode;
struct mountpoint;

struct vnode_ops
{
	status_t (*lookup)	(struct vnode *dir, const char *name,
					struct vnode **result);
	ssize_t  (*read)	(struct vnode *vn, void *buf, size_t len, /* Flawfinder: ignore */
					loff_t offset);
	ssize_t  (*write)	(struct vnode *vn, const void *buf, size_t len,
					loff_t offset);
	/* Returns 1 and fills *entry, 0 past the last entry, < 0 on error. */
	int      (*readdir)	(struct vnode *dir, uint32_t index,
					struct dirent *entry);
	status_t (*create)	(struct vnode *dir, const char *name,
					vfs_node_type_t type);
	status_t (*remove)	(struct vnode *dir, const char *name,
					vfs_node_type_t type);
	status_t (*rename)	(struct vnode *old_dir, const char *old_name,
					struct vnode *new_dir, const char *new_name);
	status_t (*truncate)	(struct vnode *vn, loff_t size);
	status_t (*sync)	(struct vnode *vn);
	/* Frees filesystem-private data; the VFS frees the vnode itself. */
	void     (*release)	(struct vnode *vn);
};

struct vnode
{
	vfs_node_type_t         type;
	loff_t                  size;
	struct mountpoint      *mp;
	const struct vnode_ops *ops;
	void                   *data;
	atomic_count_t          refcount;
};

struct mountpoint
{
	char                path[PATH_MAX]; /* Flawfinder: ignore */
	struct file_system *fs;
	struct vnode       *root;
	void               *data;
	atomic_count_t      vnode_count;

	TAILQ_ENTRY(mountpoint) next;
};

struct file_system
{
	char name[FS_NAME_MAXLEN]; /* Flawfinder: ignore */
	/* Must set mp->root (holding one reference); may set mp->data. */
	status_t (*mount)(struct mountpoint *mp, const char *source,
			const char *args);
	status_t (*umount)(struct mountpoint *mp);
	atomic_count_t refcount;

	LIST_ENTRY(file_system)	next;
};

status_t vfs_list_init(void);
status_t fs_register(struct file_system *fs);
status_t fs_unregister(struct file_system *fs);

struct vnode *vnode_alloc(struct mountpoint *mp, vfs_node_type_t type,
		loff_t size, const struct vnode_ops *ops, void *data);
void vnode_get(struct vnode *vn);
void vnode_put(struct vnode *vn);

status_t vfs_mount(const char *source, const char *target,
		const char *fs_name, const char *args);
status_t vfs_umount(const char *target);

status_t vfs_normalize(const char *cwd, const char *path, char *out,
		size_t size);
/* Returns a referenced vnode; the caller must vnode_put() it. */
status_t vfs_lookup(const char *cwd, const char *path, struct vnode **result);
ssize_t  vfs_read(struct vnode *vn, void *buf, size_t len, loff_t offset); /* Flawfinder: ignore */
ssize_t  vfs_write(struct vnode *vn, const void *buf, size_t len,
		loff_t offset);
int      vfs_readdir(struct vnode *dir, uint32_t index, struct dirent *entry);
status_t vfs_create(const char *cwd, const char *path, vfs_node_type_t type);
status_t vfs_remove(const char *cwd, const char *path, vfs_node_type_t type);
status_t vfs_rename(const char *cwd, const char *old_path,
		const char *new_path);

/* Open flags */
#define VFS_OPEN_EXCL        (1 << 0)
#define VFS_OPEN_CREAT       (1 << 1)
#define VFS_OPEN_TRUNC       (1 << 2)
#define VFS_OPEN_NOFOLLOW    (1 << 3)
#define VFS_OPEN_DIRECTORY   (1 << 4)
#define VFS_OPEN_SYNC        (1 << 5)
#define VFS_OPEN_CLOSEONEXEC (1 << 6)
#define VFS_OPEN_READ        (1 << 16)
#define VFS_OPEN_WRITE       (1 << 17)
