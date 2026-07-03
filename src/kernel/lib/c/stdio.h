/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#define EOF  (-1)

void vbe_console_format(const char *fmt, ...)
	__attribute__((format(__printf__, 1, 2)));

#define kprintf(fmt, ...) \
	do { \
		(void)sizeof(char[1 - 2 * !__builtin_constant_p(fmt)]); \
		vbe_console_format("" fmt "", ##__VA_ARGS__); \
	} while (0)
