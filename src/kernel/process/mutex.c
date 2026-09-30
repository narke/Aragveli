/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
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

mutex_t *
mutex_create(void)
{
	mutex_t *mtx = malloc(sizeof(mutex_t));

	if (!mtx)
		return NULL;

	mtx->owner = NULL;
	mtx->count = 0;
	TAILQ_INIT(&mtx->waitqueue);

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
	uint32_t flags;
	thread_t *current_thread = thread_get_current();

	X86_IRQs_DISABLE(flags);

	if (mtx->owner == current_thread)
	{
		X86_IRQs_DISABLE(flags);
		return -KERNEL_BUSY;
	}

	if (mtx->owner == NULL)
	{
		mtx->owner = current_thread;
	}
	else
	{
		current_thread->state = THREAD_BLOCKED;
		scheduler_remove_thread(current_thread);
		TAILQ_INSERT_TAIL(&mtx->waitqueue, current_thread, next);
		schedule();
	}

	X86_IRQs_DISABLE(flags);
	return KERNEL_OK;
}

status_t
mutex_unlock(mutex_t *mtx)
{
	uint32_t flags;
	thread_t *next_owner;

	X86_IRQs_DISABLE(flags);

	if (mtx->owner != thread_get_current())
	{
		X86_IRQs_ENABLE(flags);
		return -KERNEL_PERMISSION_ERROR;
	}

	next_owner = TAILQ_FIRST(&mtx->waitqueue);

	if (next_owner)
	{
		TAILQ_REMOVE(&mtx->waitqueue, next_owner, next);
		scheduler_insert_thread(next_owner);
	}

	mtx->owner = next_owner;

	X86_IRQs_ENABLE(flags);
	return KERNEL_OK;
}
