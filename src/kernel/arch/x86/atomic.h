/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

static inline void
atomic_inc(atomic_count_t count)
{
	asm volatile("\n\tlock incl %0" : "+m"(count));
}

static inline void
atomic_dec(atomic_count_t count)
{
	asm volatile("\n\tlock decl %0" : "+m"(count));
}

