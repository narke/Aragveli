/*
 * Copyright (c) 2017, 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/queue.h>
#include <lib/types.h>
#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <arch/x86/irq.h>
#include <process/thread.h>
#include <process/scheduler.h>

#include "mutex.h"

void
mutex_init(mutex_t *mtx)
{
	mtx->owner = NULL;
	mtx->count = 0;
	spinlock_init(&mtx->lock);
	TAILQ_INIT(&mtx->waitqueue);
}

mutex_t *
mutex_create(void)
{
	mutex_t *mtx = malloc(sizeof(mutex_t));

	if (mtx)
	{
		mutex_init(mtx);
	}

	return mtx;
}

void
mutex_destroy(mutex_t *mtx)
{
	assert(mtx->owner == NULL);
	assert(TAILQ_EMPTY(&mtx->waitqueue));
	free(mtx);
}

status_t
mutex_lock(mutex_t *mtx)
{
	thread_t *current_thread = thread_get_current();
	uint32_t flags = spinlock_lock_irqsave(&mtx->lock);

	if (mtx->owner == current_thread)
	{
		spinlock_unlock_irqrestore(&mtx->lock, flags);
		return -KERNEL_BUSY;
	}

	if (mtx->owner == NULL)
	{
		mtx->owner = current_thread;
		spinlock_unlock_irqrestore(&mtx->lock, flags);
		return KERNEL_OK;
	}

	TAILQ_INSERT_TAIL(&mtx->waitqueue, current_thread, next);
	sched_sleep(&mtx->lock);
	X86_IRQs_ENABLE(flags);

	return KERNEL_OK;
}

status_t
mutex_unlock(mutex_t *mtx)
{
	thread_t *next_owner;
	uint32_t flags = spinlock_lock_irqsave(&mtx->lock);

	if (mtx->owner != thread_get_current())
	{
		spinlock_unlock_irqrestore(&mtx->lock, flags);
		return -KERNEL_PERMISSION_ERROR;
	}

	next_owner = TAILQ_FIRST(&mtx->waitqueue);

	if (next_owner)
	{
		TAILQ_REMOVE(&mtx->waitqueue, next_owner, next);
	}

	mtx->owner = next_owner;

	spinlock_unlock_irqrestore(&mtx->lock, flags);

	if (next_owner)
	{
		scheduler_insert_thread(next_owner);
	}

	return KERNEL_OK;
}
