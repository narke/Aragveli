#pragma once

#include <lib/types.h>

int cmd_cat(const char *cwd, const char *path);
int cmd_cd(char *cwd, size_t size, const char *path);
int cmd_cp(const char *cwd, const char *src_path, const char *dst_path);
int cmd_file(const char *cwd, const char *path);
int cmd_ls(const char *cwd, const char *path);
int cmd_mkdir(const char *cwd, const char *path);
int cmd_mv(const char *cwd, const char *src_path, const char *dst_path);
int cmd_pwd(const char *cwd);
int cmd_rm(const char *cwd, const char *path);
int cmd_rmdir(const char *cwd, const char *path);
int cmd_touch(const char *cwd, const char *path);
