#pragma once

#include <lib/types.h>
#include <fs/tarfs.h>

int cmd_ls(struct node *root, struct node *cwd, char *path);
int cmd_mkdir();
int cmd_rm();
int cmd_touch();
int cmd_cat();
int cmd_pwd(struct node *cwd);
int cmd_cd(struct node *, const char *);
int cmd_cp();
int cmd_mv();
void tarfs_test(struct node *root);
