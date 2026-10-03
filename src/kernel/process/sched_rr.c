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
#include <arch/x86/spinlock.h>
#include <arch/x86/atomic.h>
#include <arch/x86/lapic.h>
#include <arch/x86/smp.h>

#include "scheduler.h"
#include "process.h"
#include "sched_rr.h"


static spinlock_t sched_lock = SPINLOCK_INIT;
TAILQ_HEAD(, thread) ready_threads;

void
scheduler_setup(void)
{
	TAILQ_INIT(&ready_threads);
}

static void
kick_idle_cpu(void)
{
	uint32_t self = this_cpu()->id;
	for (uint32_t i = 0; i < MAX_CPU_COUNT; i++)
	{
		if (i == self || !(g_online_cpus & (1u << i)))
		{
			continue;
		}

		if (g_cpus[i].current == g_cpus[i].idle)
		{
			LocalApicSendIpi(g_cpus[i].apic_id, RESCHED_VECTOR);
			return;
		}
	}
}

void
scheduler_insert_thread(thread_t *t)
{
	uint32_t flags = spinlock_lock_irqsave(&sched_lock);

	if (t->state != THREAD_READY && t->state != THREAD_RUNNING)
	{
		assert(t->state != THREAD_ZOMBIE);

		t->state = THREAD_READY;
		TAILQ_INSERT_TAIL(&ready_threads, t, sched_next);
		kick_idle_cpu();
	}

	spinlock_unlock_irqrestore(&sched_lock, flags);
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
	process_t *prev_process = prev_thread ? prev_thread->process : NULL;
	process_t *next_process = next_thread->process;

	uint32_t next_pd = (next_process && next_process->page_directory)
		? next_process->page_directory
		: page_directory_kernel();

	if (next_process)
	{
		atomic_set_bit(&next_process->vm_cpus, cpu->id);
	}

	page_directory_switch(next_pd);

	if (prev_process && prev_process != next_process)
	{
		atomic_clear_bit(&prev_process->vm_cpus, cpu->id);
	}

	if (next_process)
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
pick_next_locked(void)
{
	thread_t *next_thread = TAILQ_FIRST(&ready_threads);

	if (!next_thread)
	{
		next_thread = this_cpu()->idle;
	}
	else
	{
		TAILQ_REMOVE(&ready_threads, next_thread, sched_next);
	}

	next_thread->state = THREAD_RUNNING;
	return next_thread;
}

static void
schedule_locked(void)
{
	struct cpu *cpu = this_cpu();
	thread_t *current_thread = cpu->current;
	thread_t *next_thread;

	if (current_thread->state == THREAD_RUNNING)
	{
		current_thread->state = THREAD_READY;

		if (current_thread != cpu->idle)
		{
			TAILQ_INSERT_TAIL(&ready_threads, current_thread, sched_next);
		}
	}

	next_thread = pick_next_locked();

	if (current_thread == next_thread)
	{
		return;
	}

	switch_to(current_thread, next_thread, &current_thread->cpu_state);

	sched_finish_switch();

	assert(current_thread == this_cpu_current());
	assert(current_thread->state == THREAD_RUNNING);
}

void
schedule(void)
{
	uint32_t flags = spinlock_lock_irqsave(&sched_lock);

	schedule_locked();

	spinlock_unlock_irqrestore(&sched_lock, flags);
}

void
sched_sleep(spinlock_t *wq_lock)
{
	thread_t *self = this_cpu_current();

	spinlock_lock(&sched_lock);
	spinlock_unlock(wq_lock);

	self->state = THREAD_BLOCKED;

	schedule_locked();
	spinlock_unlock(&sched_lock);
}

void
sched_thread_entry(void)
{
	sched_finish_switch();
	spinlock_unlock(&sched_lock);
	asm volatile("sti" ::: "memory");
}

void
scheduler_start(void)
{
	static struct cpu_state *discard[MAX_CPU_COUNT];
	struct cpu *cpu = this_cpu();
	thread_t *idle = cpu->current;
	thread_t *next_thread;

	spinlock_lock(&sched_lock);	/* IRQs are still off on every CPU here */

	idle->state  = THREAD_READY;
	idle->on_cpu = 0;
	cpu->prev    = NULL;

	next_thread = pick_next_locked();

	/* Discard the boot stack. Never returns. */
	switch_to(NULL, next_thread, &discard[cpu->id]);

	for (;;)
	{
		;
	}
}

void
scheduler_switch_to_next(thread_t *dying)
{
	spinlock_lock(&sched_lock);

	thread_t *next_thread = pick_next_locked();

	switch_to(dying, next_thread, &dying->cpu_state);
	__builtin_unreachable();
}
