/*
 * Copyright (c) 2017, 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once


#include <lib/queue.h>
#include <lib/types.h>
#include <arch/x86/atomic.h>
#include <arch/x86/spinlock.h>
#include <process/thread.h>

typedef struct
{
	volatile atomic_count_t count;
	spinlock_t lock;
	TAILQ_HEAD(, thread) waitqueue;
} semaphore_t;

semaphore_t *semaphore_create(int32_t count);
void semaphore_destroy(semaphore_t *semaphore);
void semaphore_up(semaphore_t *semaphore);
void semaphore_down(semaphore_t *semaphore);
