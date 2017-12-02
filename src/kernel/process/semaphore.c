/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/stdlib.h>
#include <arch/x86/irq.h>
#include <process/scheduler.h>

#include "semaphore.h"

semaphore_t *
semaphore_create(int32_t value)
{
	semaphore_t *semaphore = malloc(sizeof(semaphore_t));

	if (!semaphore)
		return NULL;

	semaphore->value = value;
	TAILQ_INIT(&semaphore->waitqueue);

	return semaphore;
}

void
semaphore_destroy(semaphore_t *semaphore)
{
	thread_t *item;

	while ((item = TAILQ_FIRST(&semaphore->waitqueue)))
	{
		TAILQ_REMOVE(&semaphore->waitqueue, item, next);
	}

	free(semaphore);
}

void
semaphore_up(semaphore_t *semaphore)
{
	uint32_t flags;

	X86_IRQs_DISABLE(flags);

	semaphore->value++;

	if (semaphore->value >= 0)
	{
		// Awake a blocked thread
		thread_t *unblocked_thread = TAILQ_FIRST(&semaphore->waitqueue);
		TAILQ_REMOVE(&semaphore->waitqueue, unblocked_thread, next);
		unblocked_thread->state = THREAD_READY;
		scheduler_set_ready(unblocked_thread);
	}

	X86_IRQs_ENABLE(flags);
}

void
semaphore_down(semaphore_t *semaphore)
{
	uint32_t flags;

	X86_IRQs_DISABLE(flags);

	if (semaphore->value >= 0)
		semaphore->value--;

	if (semaphore->value < 0)
	{
		thread_t *current_thread = thread_get_current();
		current_thread->state = THREAD_BLOCKED;
		TAILQ_INSERT_TAIL(&semaphore->waitqueue, current_thread, next);
		schedule();
	}

	X86_IRQs_ENABLE(flags);
}
