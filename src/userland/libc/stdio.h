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

int printf(const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);

/* buf must be at least len bytes; len is both capacity and max to read. */
int read(int fd, void *buf, size_t len)
	__attribute__((access(write_only, 2, 3)));
