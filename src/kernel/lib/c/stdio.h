/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#define EOF  (-1)

void __printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define printf(fmt, ...) \
	do { \
		(void)sizeof(char[1 - 2 * !__builtin_constant_p(fmt)]); \
		static const char __pf_fmt[] = fmt; \
		__printf(__pf_fmt, ##__VA_ARGS__); \
	} while (0)
