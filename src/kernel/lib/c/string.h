/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

void *kmemset(void *dst, int c, size_t length);
void *kmemcpy(void *dst, const void *src, size_t size);
char *strzcpy(char *dst, const char *src, size_t len);
size_t kstrlen(const char *s);
int kstrncmp(const char *s1, const char *s2, size_t n);
char *kstrchr(const char *s, int c);
char *kstrrchr(const char *s, int c);
