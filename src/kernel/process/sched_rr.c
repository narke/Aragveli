/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/types.h>
#include <lib/c/assert.h>

#include "scheduler.h"


TAILQ_HEAD(, thread) ready_threads;

void
scheduler_setup(void)
{
	TAILQ_INIT(&ready_threads);
}

void
scheduler_insert_thread(thread_t *t)
{
	/* Don't do anything for already ready threads */
	if (t->state == THREAD_READY)
		return;

	/* Schedule it for the present turn */
	assert((t->state == THREAD_CREATED)
		|| (t->state == THREAD_RUNNING)
		|| (t->state == THREAD_BLOCKED));

	t->state = THREAD_READY;

	thread_t *thread;

	TAILQ_FOREACH(thread, &ready_threads, next)
	{
		if (t->priority > thread->priority)
			break;
	}

	TAILQ_INSERT_TAIL(&ready_threads, t, next);
}

void
scheduler_remove_thread(thread_t *t)
{
	TAILQ_REMOVE(&ready_threads, t,next);
}

void
scheduler_elect_new_current_thread(void)
{
	thread_t *t = TAILQ_FIRST(&ready_threads);
	thread_set_current(t);
	schedule();
}

void
schedule(void)
{
	thread_t *current_thread = thread_get_current();

	assert(TAILQ_EMPTY(&ready_threads) == false);

	thread_t *next_thread = TAILQ_FIRST(&ready_threads);
	TAILQ_REMOVE(&ready_threads, next_thread, next);
	TAILQ_INSERT_TAIL(&ready_threads, next_thread, next);

	next_thread->state = THREAD_READY;

	thread_set_current(next_thread);

	// Avoid context switch if the context does not change
	if (current_thread != next_thread)
	{
		cpu_context_switch(&current_thread->cpu_state,
			next_thread->cpu_state);

		assert(current_thread == thread_get_current());
		assert(current_thread->state == THREAD_RUNNING);
	}
}
