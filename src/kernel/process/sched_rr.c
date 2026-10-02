/*
 * Copyright (c) 2017, 2026 Konstantin Tcholokachvili.
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
#include <arch/x86/per_cpu.h>

#include "scheduler.h"
#include "process.h"
#include "sched_rr.h"


TAILQ_HEAD(, thread) ready_threads;

void
scheduler_setup(void)
{
	TAILQ_INIT(&ready_threads);
}

void
scheduler_insert_thread(thread_t *t)
{
	/* Don't do anything for already ready or running threads */
	if (t->state == THREAD_READY || t->state == THREAD_RUNNING)
	{
		return;
	}

	/* New (zeroed) or blocked. */
	assert(t->state != THREAD_ZOMBIE);

	t->state = THREAD_READY;
	TAILQ_INSERT_TAIL(&ready_threads, t, sched_next);
}

void
sched_finish_switch(void)
{
	struct cpu *cpu = this_cpu();

	if (cpu->prev)
	{
		cpu->prev->on_cpu = 0;
		cpu->prev = NULL;
	}
}

static void
switch_to(thread_t *prev_thread, thread_t *next_thread, struct cpu_state **save_to)
{
	struct cpu *cpu = this_cpu();

	uint32_t next_pd = (next_thread->process
			&& next_thread->process->page_directory)
		? next_thread->process->page_directory
		: page_directory_kernel();

	page_directory_switch(next_pd);

	if (next_thread->process)
	{
		set_kernel_stack(next_thread->kernel_stack_top);
	}

	sched_finish_switch();

	assert(next_thread->on_cpu == 0);
	next_thread->on_cpu = 1;
	cpu->current        = next_thread;
	cpu->prev           = prev_thread;

	cpu_context_switch(save_to, next_thread->cpu_state);
}

static thread_t *
pick_next(void)
{
	thread_t *next = TAILQ_FIRST(&ready_threads);

	if (!next)
	{
		next = this_cpu()->idle;
	}
	else
	{
		TAILQ_REMOVE(&ready_threads, next, sched_next);
	}

	next->state = THREAD_RUNNING;

	return next;
}

void
scheduler_start(void)
{
	static struct cpu_state *discard;
	struct cpu *cpu = this_cpu();
	thread_t *idle = cpu->current;

	idle->state  = THREAD_READY;
	idle->on_cpu = 0;
	cpu->prev    = NULL;

	thread_t *next_thread = pick_next();

	/* Discard the boot stack. Never returns. */
	switch_to(NULL, next_thread, &discard);

	for (;;)
	{
		;
	}
}

void
schedule(void)
{
	struct cpu *cpu = this_cpu();
	thread_t *current_thread = cpu->current;

	if (current_thread->state == THREAD_RUNNING)
	{
		current_thread->state = THREAD_READY;

		if (current_thread != cpu->idle)
		{
			TAILQ_INSERT_TAIL(&ready_threads, current_thread, sched_next);
		}
	}

	thread_t *next_thread = pick_next();
	
	// Avoid context switch if the context does not change
	if (current_thread == next_thread)
	{
		return;
	}
	
	switch_to(current_thread, next_thread, &current_thread->cpu_state);

	sched_finish_switch();
	
	assert(current_thread == thread_get_current());
	assert(current_thread->state == THREAD_RUNNING);
}

void
scheduler_switch_to_next(thread_t *dying)
{
	thread_t *next_thread = pick_next();

	switch_to(dying, next_thread, &dying->cpu_state);
	__builtin_unreachable();
}
