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

static void
add_in_ready_queue(struct thread *t, bool insert_at_tail)
{
	assert((THREAD_CREATED == t->state)
		|| (THREAD_RUNNING == t->state)
		|| (THREAD_BLOCKED == t->state));

	t->state = THREAD_READY;

	if (insert_at_tail)
		TAILQ_INSERT_TAIL(&ready_threads, t, next);
	else
		TAILQ_INSERT_HEAD(&ready_threads, t, next);
}

void
scheduler_set_ready(struct thread *t)
{
	/* Don't do anything for already ready threads */
	if (THREAD_READY == t->state)
		return;

	/* Schedule it for the present turn */
	add_in_ready_queue(t, true);
}

void
schedule(void)
{
	struct thread *current_thread = thread_get_current();

	assert(TAILQ_EMPTY(&ready_threads) == false);

	struct thread *next_thread = TAILQ_FIRST(&ready_threads);
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
