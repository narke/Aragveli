/*
 * Copyright (c) 2015, 2017 Konstantin Tcholokachvili.
 * Copyright (c) 2009 Martin Decky.
 * Copyright (c) 2008 Jakub Jermar.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/queue.h>
#include <lib/types.h>
#include <lib/status.h>

/* Forward declarations */
struct file_system;
struct superblock;
struct vfs_node;
struct vfs_opened_file;
struct vfs_stat;
struct mountpoint;

#define FS_NAME_MAXLEN  20

#define PATH_MAX 1024
#define NAME_MAX 255

#define VNODE_HOLD(x) atomic_inc(&(x)->refcount)
#define VNODE_REL(x)  atomic_dec(&(x)->refcount)

typedef uint16_t dev_t;
typedef uint8_t  ino_t;
typedef uint16_t mode_t;
typedef uint8_t  nlink_t;
typedef int16_t  uid_t;
typedef int16_t  gid_t;
typedef int32_t  off_t;
typedef int64_t  loff_t;
typedef int32_t  block_size_t;
typedef int32_t  block_count_t;
typedef uint64_t time_t;
typedef int16_t  fs_handle_t;
typedef uint32_t fs_index_t;
typedef uint32_t block_device_id_t;

/*
 * Lookup flags.
 */

/**
 * No lookup flags used.
 */
#define L_NONE			0

/**
 * Lookup will succeed only if the object is a regular file.  If L_CREATE is
 * specified, an empty file will be created. This flag is mutually exclusive
 * with L_DIRECTORY.
 */
#define L_FILE			1

/**
 * Lookup will succeed only if the object is a folder. If L_CREATE is
 * specified, an empty folder will be created. This flag is mutually
 * exclusive with L_FILE.
 */
#define L_DIRECTORY		2

/**
 * Lookup will succeed only if the object is a root folder. The flag is
 * mutually exclusive with L_FILE and L_MP.
 */
#define L_ROOT			4

/**
 * Lookup will succeed only if the object is a mount point. The flag is mutually
 * exclusive with L_FILE and L_ROOT.
 */
#define L_MP			8


/**
 * When used with L_CREATE, L_EXCLUSIVE will cause the lookup to fail if the
 * object already exists. L_EXCLUSIVE is implied when L_DIRECTORY is used.
 */
#define L_EXCLUSIVE 		16

/**
 * L_CREATE is used for creating both regular files and directories.
 */
#define L_CREATE		32

/**
 * L_LINK is used for linking to an already existing nodes.
 */
#define L_LINK			64

/**
 * L_UNLINK is used to remove leaves from the file system namespace. This flag
 * cannot be passed directly by the client, but will be set by VFS during
 * VFS_UNLINK.
 */
#define L_UNLINK		128

/**
 * L_OPEN is used to indicate that the lookup operation is a part of VFS_IN_OPEN
 * call from the client. This means that the server might allocate some
 * resources for the opened file. This flag cannot be passed directly by the
 * client.
 */
#define L_OPEN			256

typedef struct
{
	bool          mp_active;
	fs_handle_t   fs_handle;
	block_device_id_t block_device;
} mp_data_t;

typedef struct
{
	mp_data_t  mp_data;
	void      *data;
} fs_node_t;

struct vfs_path
{
	char path[PATH_MAX];
	char name[NAME_MAX + 1];
};

struct dirent
{
	ino_t    d_ino;
	uint32_t d_off;
	uint32_t d_reclen;
	uint32_t d_name[NAME_MAX + 1];
};

struct folder
{
	struct node *parent;
	LIST_HEAD(, node) nodes;
};

struct file
{
	char *data;
	uint32_t size; // Limited to 4 GiB
};

struct node
{
	uint8_t type;
	uint8_t name_length;
	char name[50];
	union
	{
		struct folder folder;
		struct file   file;
	} u;
	LIST_ENTRY(node) next;
};

struct file_system_ops
{
	status_t (*mount)	(const char *, const char *, const char *,
					struct superblock **, fs_index_t *,
					uint64_t *, unsigned *);
	status_t (*umount)	(block_device_id_t);
	ino_t (*root)		(struct mountpoint *mp);
};

typedef struct {
	status_t (*mount)	(const char*, const char *, const char *,
					struct superblock **);
	status_t (*umount)	(void);
	int      (*read)	(block_device_id_t, fs_index_t,
					uint64_t, size_t, size_t *);
	int      (*write)	(block_device_id_t, fs_index_t, uint64_t,
					size_t, size_t *, uint64_t *);
	int 	 (*truncate)	(block_device_id_t, fs_index_t, uint64_t);
	int      (*close)	(block_device_id_t, fs_index_t);
	int      (*destroy)	(block_device_id_t, fs_index_t);
	int      (*sync)	(block_device_id_t, fs_index_t);
} vfs_ops_t;

typedef struct {
	/*
	 * The first set of methods are functions that return an integer error
	 * code. If some additional return value is to be returned, the first
	 * argument holds the output argument.
	 */
	int (*root_get)		(fs_node_t **, block_device_id_t);
	int (*match)		(fs_node_t **, fs_node_t *, const char *);
	int (*node_get)		(fs_node_t **, block_device_id_t, fs_index_t);
	int (*node_open)	(fs_node_t *);
	int (*node_put)		(fs_node_t *);
	int (*create)		(fs_node_t **, block_device_id_t, int);
	int (*destroy)		(fs_node_t *);
	int (*link)		(fs_node_t *, fs_node_t *, const char *);
	int (*unlink)		(fs_node_t *, fs_node_t *, const char *);
	int (*has_children)	(bool *, fs_node_t *);
	/*
	 * The second set of methods are usually mere getters that do not
	 * return an integer error code.
	 */
	fs_index_t        (*index_get)		(fs_node_t *);
	uint64_t          (*size_get)		(fs_node_t *);
	unsigned int      (*lnkcnt_get)		(fs_node_t *);
	bool              (*is_directory)	(fs_node_t *);
	bool              (*is_file)		(fs_node_t *);
	int               (*size_block)		(block_device_id_t, uint32_t *);
	int               (*total_block_count)	(block_device_id_t, uint64_t *);
	int               (*free_block_count)	(block_device_id_t, uint64_t *);
} node_ops_t;


struct file_system
{
	char name[FS_NAME_MAXLEN];
	status_t (*mount)(const char *root_device,
			const char *mount_point,
			const char *mount_args,
			struct superblock **result_rootfs);
	status_t (*umount)(void);
	atomic_count_t refcount;

	LIST_ENTRY(file_system)	next;
};

struct vfs_cache_node
{
	struct vfs_node	*node;
	atomic_count_t	ref_count;
	char 		*name;
};

struct superblock
{
	struct file_system	*filesystem;
	struct node		*root;

	LIST_ENTRY(superblock)	next;
};

struct mountpoint
{
	struct file_system *fs;
	uint32_t            flags;
	void               *data;

	struct vnode       *root_vnode;
	struct vnode       *parent_vnode;
	TAILQ_ENTRY(vnode)  vnodes;

	// TODO mutex

	TAILQ_ENTRY(mountpoint) next;
};

/* Virtual node */
struct vnode
{
	ino_t   ino;
	mode_t  mode;
	nlink_t nlink;
	loff_t  size;

	block_count_t blocks;
	block_size_t  block_size;

	struct mountpoint *mp;
	struct mountpoint *vfs_mount_here;

	void *data;

	struct vnode_ops *ops;

	atomic_count_t refcount;
	// TODO mutex
	// TODO pagecache
	TAILQ_ENTRY(vnode) next;
};

struct vnode_ops
{
	int     (*open)		(struct vnode *vnode);
	int     (*close)	(struct vnode *vnode);
	ino_t   (*lookup)	(struct vnode *vnode, char *name);
	ssize_t (*read)		(struct vnode *vnode, void *buf, size_t len,
					loff_t offset);
	ssize_t (*write)	(struct vnode *vnode, void *buf, size_t len,
					loff_t offset);
	int     (*mkdir)	(struct vnode *vnode, char *name, mode_t mode,
					uid_t uid, gid_t gid);
	int     (*creat)	(struct vnode *vnode, char *name, mode_t mode,
					uid_t uid, gid_t gid);
	int     (*symlink)	(struct vnode *vnode, char *name, char *path);
	int     (*readlink)	(struct vnode *vnode, char *buf, size_t len);
	int     (*getdents)	(struct vnode *vnode, struct dirent *dirp,
					size_t len, loff_t *offset);
	int     (*sync)		(struct vnode *vnode);
	int     (*mknod)	(struct vnode *vnode, char *name, mode_t mode,
					dev_t rdev, uid_t uid, gid_t gid);
	int    (*unlink)	(struct vnode *vnode, char *name);
};


struct vfs_node_ops_file
{
};

struct vfs_node_ops_symlink
{
};

struct vfs_node_ops_dir
{
};

typedef enum
{
	VFS_SEEK_SET = 42,
	VFS_SEEK_CUR = 24,
	VFS_SEEK_END = 84
} seek_whence_t;

struct vfs_ops_opened_file
{
};

struct vfs_ops_opened_chardev
{
};

struct vfs_dir_entry
{
};

struct vfs_ops_opened_dir
{
};

struct vfs_stat
{
};

#define VFS_PAIR \
	fs_handle_t fs_handle_t;

#define VFS_TRIPLET \
	VFS_PAIR; \
	fs_index_t fs_index;

typedef struct
{
	VFS_PAIR;
} vfs_pair_t;

typedef struct
{
	VFS_TRIPLET;
} vfs_triplet_t;

typedef enum
{
	VFS_NODE_REGULAR_FILE = 0x42,
	VFS_NODE_DIRECTORY    = 0x24,
	VFS_NODE_SYMLINK      = 0x84,
	VFS_NODE_DEVICE_CHAR  = 0x48,
} vfs_node_type_t;

typedef struct
{
	vfs_triplet_t   triplet;
	vfs_node_type_t type;
	uint64_t        size;
	unsigned int    link_count;
} vfs_lookup_result_t;

status_t vfs_init(const char       *root_device,
		const char         *fs_type,
		const char         *mount_point,
		const char         *mount_args,
		struct superblock **result_rootfs);

status_t vfs_list_init(void);


/*  ***************************************************************
 * The Following functions are relatively standard
 *
 * @see Unix manual pages for details
 */


/**
 * mount a file system
 *
 * @param actor struct task calling mount
 * @param _src_path(len) may be NULL (as for virtfs or /proc)
 * @fsname the name of the filesystem type to mount
 * @args any args passed to the file_system::mount method
 * @result_fs the resulting filesystem instance
 */
status_t vfs_mount(const char	   *src_path,
		size_t		    src_pathlen,
		const char	   *dst_path,
		size_t		    dst_pathlen,
		const char	   *fs_name,
		uint32_t	    mountflags,
		const char	   *args,
		struct superblock **result_fs);

/**
 * unmount the filesystem at the given location
 */
status_t vfs_umount(const char *mountpoint_path,
		unsigned int mountpoint_pathlen);

/**
 * Flush all the dirty nodes of all the FS to disk
 */
status_t vfs_sync_all_fs(void);


/**
 * Open flags
 */
#define VFS_OPEN_EXCL        (1 << 0)
#define VFS_OPEN_CREAT       (1 << 1)
#define VFS_OPEN_TRUNC       (1 << 2)
#define VFS_OPEN_NOFOLLOW    (1 << 3)
#define VFS_OPEN_DIRECTORY   (1 << 4) /* Incompatible with CREAT/TRUNC */
#define VFS_OPEN_SYNC        (1 << 5)
#define VFS_OPEN_CLOSEONEXEC (1 << 6) /* By default, files are kept
					 open upon an exec() */

#define VFS_OPEN_READ        (1 << 16)
#define VFS_OPEN_WRITE       (1 << 17)


/**
 * FS access rights
 */
#define VFS_S_IRUSR   00400
#define VFS_S_IWUSR   00200
#define VFS_S_IXUSR   00100

#define VFS_S_IRWXALL 07777 /* For symlinks */

/* ***************************************************************
 * Restricted functions reserved to FS code and block/char devices
 */

/**
 * Function to be called when proposing a new File system type
 */
status_t fs_register(struct file_system *fs);
status_t fs_unregister(struct file_system *fs);


/*
 * This is outside VFS
 */
status_t mount(const char *source, const char *target,
		const char *file_system_name, uint32_t mount_flags,
		const char *data);

status_t umount(const char *target);
