/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "stdint.h"
#include "stdbool.h"
#include "stdarg.h"

typedef uint32_t size_t;

/* Formats are compiler-checked at every call site: -Wformat -Wformat-security. */
int printf(const char *, ...) /* Flawfinder: ignore */
	__attribute__((__format__(__printf__, 1, 2)));
int vsnprintf(char *, size_t, const char *, va_list) /* Flawfinder: ignore */
	__attribute__((__format__(__printf__, 3, 0)));
int read(int fd, void *buf, size_t len); /* Flawfinder: ignore */
