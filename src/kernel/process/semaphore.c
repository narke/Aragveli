/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <arch/x86/irq.h>
#include <process/scheduler.h>

#include "semaphore.h"

semaphore_t *
semaphore_create(int32_t value)
{
	semaphore_t *semaphore = malloc(sizeof(semaphore_t));

	if (!semaphore)
		return NULL;

	semaphore->count = value;
	TAILQ_INIT(&semaphore->waitqueue);

	return semaphore;
}

void
semaphore_destroy(semaphore_t *semaphore)
{
	assert(TAILQ_EMPTY(&semaphore->waitqueue));
	free(semaphore);
}

void
semaphore_up(semaphore_t *semaphore)
{
	uint32_t flags;
	thread_t *t;

	X86_IRQs_DISABLE(flags);

	t = TAILQ_FIRST(&semaphore->waitqueue);

	if (t)
	{
		// Awake a blocked thread
		TAILQ_REMOVE(&semaphore->waitqueue, t, next);
		scheduler_insert_thread(t);
	}
	else
	{
		semaphore->count++;
	}

	X86_IRQs_ENABLE(flags);
}

void
semaphore_down(semaphore_t *semaphore)
{
	uint32_t flags;

	X86_IRQs_DISABLE(flags);

	if (semaphore->count > 0)
	{
		semaphore->count--;
	}
	else
	{
		thread_t *current_thread = thread_get_current();

		current_thread->state = THREAD_BLOCKED;
		scheduler_remove_thread(current_thread);
		TAILQ_INSERT_TAIL(&semaphore->waitqueue, current_thread, next);
		schedule();
	}

	X86_IRQs_ENABLE(flags);
}
