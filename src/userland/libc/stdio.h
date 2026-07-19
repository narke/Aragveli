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
#include "string.h"

int printf(const char *fmt, ...)
	__attribute__((format(__printf__, 1, 2)));
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
	__attribute__((format(__printf__, 3, 0)));
int snprintf(char *buf, size_t size, const char *fmt, ...)
	__attribute__((format(__printf__, 3, 4)));

int read(int fd, void *buf, size_t bufsize);
