/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

#define NULL ((void*)0)

void *memset(void *dst, int c, size_t length);
void *memcpy(void *dst, const void *src, size_t size);
void *memcpy_s(void *dst, size_t dst_size, const void *src, size_t src_size);
char *strzcpy(char *dst, const char *src, size_t len);
size_t strnlen(const char *s, size_t max_length);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
