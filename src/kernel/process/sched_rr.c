/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/types.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <lib/c/stdbool.h>
#include <arch/x86/paging.h>
#include <arch/x86/gdt.h>

#include "scheduler.h"
#include "process.h"


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

	/* New (zeroed), running, or blocked — never ready/zombie. */
	assert(t->state != THREAD_READY && t->state != THREAD_ZOMBIE);

	t->state = THREAD_READY;
	TAILQ_INSERT_TAIL(&ready_threads, t, sched_next);
}

void
scheduler_remove_thread(thread_t *t)
{
	TAILQ_REMOVE(&ready_threads, t, sched_next);
}

static void
switch_to(thread_t *next_thread, struct cpu_state **save_to)
{
	uint32_t next_pd = (next_thread->process
			&& next_thread->process->page_directory)
		? next_thread->process->page_directory
		: page_directory_kernel();

	page_directory_switch(next_pd);

	if (next_thread->process)
		set_kernel_stack(next_thread->kernel_stack_top);

	cpu_context_switch(save_to, next_thread->cpu_state);
}

static thread_t *
pick_next(void)
{
	thread_t *next = TAILQ_FIRST(&ready_threads);

	TAILQ_REMOVE(&ready_threads, next, sched_next);
	TAILQ_INSERT_TAIL(&ready_threads, next, sched_next);
	next->state = THREAD_READY;
	thread_set_current(next);

	return next;
}

void
scheduler_start(void)
{
	static struct cpu_state *discard;

	assert(TAILQ_EMPTY(&ready_threads) == false);

	thread_t *next_thread = pick_next();

	/* Discard the boot stack. Never returns. */
	switch_to(next_thread, &discard);

	for (;;)
		;
}

void
schedule(void)
{
	thread_t *current_thread = thread_get_current();

	assert(TAILQ_EMPTY(&ready_threads) == false);

	thread_t *next_thread = pick_next();

	// Avoid context switch if the context does not change
	if (current_thread != next_thread)
	{
		/* Preserve THREAD_BLOCKED (wait/sem); only demote a running thread. */
		if (current_thread->state == THREAD_RUNNING)
			current_thread->state = THREAD_READY;
		switch_to(next_thread, &current_thread->cpu_state);

		assert(current_thread == thread_get_current());
		assert(current_thread->state == THREAD_RUNNING);
	}
}

void
scheduler_switch_to_next(thread_t *dying)
{
	assert(TAILQ_EMPTY(&ready_threads) == false);   /* idle is always present */

	thread_t *next_thread = pick_next();

	switch_to(next_thread, &dying->cpu_state);
	__builtin_unreachable();
}
