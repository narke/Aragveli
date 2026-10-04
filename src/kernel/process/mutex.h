/*
 * Copyright (c) 2017, 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/queue.h>
#include <lib/types.h>
#include <process/thread.h>
#include <arch/x86/atomic.h>
#include <arch/x86/spinlock.h>
#include "semaphore.h"

typedef struct
{
	thread_t		*owner;
	volatile atomic_count_t	count;
	spinlock_t		lock;
	TAILQ_HEAD(, thread)	waitqueue;
} mutex_t;

void mutex_init(mutex_t *mtx);
mutex_t *mutex_create(void);
void mutex_destroy(mutex_t *mtx);
status_t mutex_lock(mutex_t *mtx);
status_t mutex_unlock(mutex_t *mtx);
