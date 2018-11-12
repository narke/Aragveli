/*
 * Copyright (c) 2015, 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>
#include <fs/vfs.h>

#define TMPFS_NODE(node) ((node) ? (tarfs_node_t *)(node)->data : NULL)
#define FS_NODE(node)    ((node) ? (node)->bp : NULL)

#define TARFS_FILE	1
#define TARFS_DIRECTORY	2

typedef enum
{
	TMPFS_NONE,
	TMPFS_FILE,
	TMPFS_FOLDER
} tarfs_node_type_t;

/* forward declaration */
struct tarfs_node;

status_t tarfs_init(vaddr_t initrd_start, vaddr_t initrd_end);
struct node *resolve_node(const char *path, struct node *root_node);
