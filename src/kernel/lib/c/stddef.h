/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>	/* size_t */

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __WCHAR_TYPE__   wchar_t;

typedef struct
{
	long long   ll __attribute__((aligned(__alignof__(long long))));
	long double ld __attribute__((aligned(__alignof__(long double))));
} max_align_t;

#define offsetof(type, member) __builtin_offsetof(type, member)
