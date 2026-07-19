#pragma once

#include <lib/types.h>
#include <fs/tarfs.h>

int cmd_cat(struct node *root, struct node *cwd, const char *path);
int cmd_cd(struct node *root, struct node **cwd, const char *path);
int cmd_cp(struct node *root, struct node *cwd, const char *src_path,
		const char *dst_path);
int cmd_file(struct node *root, struct node *cwd, const char *path);
int cmd_ls(struct node *root, struct node *cwd, const char *path);
int cmd_mkdir(struct node *root, struct node *cwd, const char *path);
int cmd_mv(struct node *root, struct node *cwd, const char *src_path,
		const char *dst_path);
int cmd_pwd(struct node *cwd);
int cmd_rm(struct node *root, struct node *cwd, const char *path);
int cmd_rmdir(struct node *root, struct node *cwd, const char *path);
int cmd_touch(struct node *root, struct node *cwd, const char *path);
