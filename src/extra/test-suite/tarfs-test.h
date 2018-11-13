#pragma once

#include <lib/types.h>
#include <fs/tarfs.h>

int cmd_cat(struct node *root, struct node *cwd, const char *path, const char *filename);
int cmd_cd(struct node *, const char *);
int cmd_cp(struct node *root, struct node *cwd, const char *src_path, const char *src_node_name,
		const char *dst_path, const char *dst_node_name);
int cmd_du();
int cmd_df();
int cmd_file(struct node *root, struct node *cwd, const char *path, const char *nodename);
int cmd_ln();
int cmd_ls(struct node *root, struct node *cwd, const char *path);
int cmd_mkdir(struct node *root, struct node *cwd, const char *path, const char *folder_name);
int cmd_mv(struct node *root, struct node *cwd, const char *src_path, const char *src_node_name,
		const char *dst_path, const char *dst_node_name);
int cmd_pwd(struct node *cwd);
int cmd_rm(struct node *root, struct node *cwd, const char *path, const char *filename);
int cmd_rmdir(struct node *root, struct node *cwd, const char *path, const char *folder_name);
int cmd_touch(struct node *root, struct node *cwd, const char *path, const char *filename);
void tarfs_test(struct node *root);
