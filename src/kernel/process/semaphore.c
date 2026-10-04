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
	spinlock_init(&semaphore->lock);
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
	thread_t *t;
	uint32_t flags = spinlock_lock_irqsave(&semaphore->lock);

	t = TAILQ_FIRST(&semaphore->waitqueue);

	if (t)
	{
		TAILQ_REMOVE(&semaphore->waitqueue, t, next);
	}
	else
	{
		semaphore->count++;
	}

	spinlock_unlock_irqrestore(&semaphore->lock, flags);

	// Awake a blocked thread
	if (t)
	{
		scheduler_insert_thread(t);
	}
}

void
semaphore_down(semaphore_t *semaphore)
{
	uint32_t flags = spinlock_lock_irqsave(&semaphore->lock);

	if (semaphore->count > 0)
	{
		semaphore->count--;
		spinlock_unlock_irqrestore(&semaphore->lock, flags);
		return;
	}

	thread_t *current_thread = thread_get_current();
	TAILQ_INSERT_TAIL(&semaphore->waitqueue, current_thread, next);
	sched_sleep(&semaphore->lock);
	X86_IRQs_ENABLE(flags);
}
