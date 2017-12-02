/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/queue.h>
#include <lib/types.h>
#include <lib/c/stdlib.h>
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
	TAILQ_INIT(&mtx->waitqueue);

	return mtx;
}

void
mutex_destroy(mutex_t *mtx)
{
	thread_t *item;

	while ((item = TAILQ_FIRST(&mtx->waitqueue)))
	{
		TAILQ_REMOVE(&mtx->waitqueue, item, next);
	}

	free(mtx);
}

status_t
mutex_lock(mutex_t *mtx)
{
	uint32_t flags;
	status_t status = KERNEL_OK;

	X86_IRQs_DISABLE(flags);

	// Mutex already owned?
	if (mtx->owner != NULL)
	{
		thread_t *current_thread = thread_get_current();

		if (mtx->owner == current_thread)
			status = -KERNEL_BUSY;
		else
			TAILQ_INSERT_TAIL(&mtx->waitqueue, current_thread, next);
	}

	X86_IRQs_ENABLE(flags);

	return status;
}

status_t
mutex_unlock(mutex_t *mtx)
{
	uint32_t flags;
	status_t status;

	X86_IRQs_DISABLE(flags);

	if (mtx->owner != thread_get_current())
	{
		status = -KERNEL_PERMISSION_ERROR;
	}
	else if (TAILQ_EMPTY(&mtx->waitqueue))
	{
		mtx->owner = NULL;
		status = KERNEL_OK;
	}
	else
	{
		thread_t *blocked_thread = TAILQ_FIRST(&mtx->waitqueue);
		TAILQ_REMOVE(&mtx->waitqueue, blocked_thread, next);

		if (blocked_thread->state != THREAD_RUNNING)
			scheduler_set_ready(blocked_thread);

		status = KERNEL_OK;
	}

	X86_IRQs_ENABLE(flags);

	return status;
}
