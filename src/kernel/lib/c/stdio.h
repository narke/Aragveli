/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#define EOF  (-1)

void __printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define __printf_first(first, ...) first
#define printf(...) \
	((void)sizeof(char[1 - 2 * !__builtin_constant_p(__printf_first(__VA_ARGS__, ""))]), \
	 __printf(__VA_ARGS__))
