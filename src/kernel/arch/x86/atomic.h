/*
 * Copyright (c) 2017, 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>
#include <lib/c/stdbool.h>

static inline atomic_count_t
atomic_read(const volatile atomic_count_t *v)
{
	return *v;
}

static inline void
atomic_set(volatile atomic_count_t *v, atomic_count_t i)
{
	*v = i;
}

static inline void
atomic_inc(volatile atomic_count_t *v)
{
	asm volatile("lock incl %0" : "+m"(*v) :: "memory", "cc");
}

static inline void
atomic_dec(volatile atomic_count_t *v)
{
	asm volatile("lock decl %0" : "+m"(*v) :: "memory", "cc");
}

static inline atomic_count_t
atomic_fetch_add(volatile atomic_count_t *v, atomic_count_t i)
{
	asm volatile("lock xaddl %0, %1"
		: "+r"(i), "+m"(*v) :: "memory", "cc");
	return i;
}

static inline bool
atomic_dec_and_test(volatile atomic_count_t *v)
{
	uint8_t zero;

	asm volatile("lock decl %0; sete %1"
		: "+m"(*v), "=qm"(zero) :: "memory", "cc");
	return zero;
}

static inline atomic_count_t
atomic_cmpxchg(volatile atomic_count_t *v,
	atomic_count_t old,
	atomic_count_t new)
{
	asm volatile("lock cmpxchgl %2, %1"
		: "+a"(old), "+m"(*v)
		: "r"(new)
		: "memory", "cc");
	return old;
}

static inline void
atomic_set_bit(volatile uint32_t *v, uint32_t bit)
{
	asm volatile("lock btsl %1, %0"
		: "+m"(*v) : "Ir"(bit) : "memory", "cc");
}

static inline void
atomic_clear_bit(volatile uint32_t *v, uint32_t bit)
{
	asm volatile("lock btrl %1, %0"
		: "+m"(*v) : "Ir"(bit) : "memory", "cc");
}
