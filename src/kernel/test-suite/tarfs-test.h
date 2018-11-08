#pragma once

#include <lib/types.h>
#include <fs/tarfs.h>

int cmd_ls(struct dentry *root, struct dentry *cwd, char *path);
int cmd_mkdir();
int cmd_rm();
int cmd_touch();
int cmd_cat();
int cmd_pwd(struct dentry *cwd);
int cmd_cd(struct dentry *, const char *);
int cmd_cp();
int cmd_mv();
void tarfs_test(struct dentry *root);
