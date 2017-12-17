/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/assert.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <lib/status.h>
#include <arch/x86/irq.h>
#include "thread.h"
#include "scheduler.h"

TAILQ_HEAD(, thread) kernel_threads;

static volatile thread_t *g_current_thread = NULL;

static void
idle_thread()
{
	while (1)
		asm("hlt\n");
}

inline void
thread_set_current(thread_t *current_thread)
{
	assert(current_thread->state == THREAD_READY);

	g_current_thread        = current_thread;
	g_current_thread->state = THREAD_RUNNING;
}

thread_t *
thread_get_current(void)
{
	assert(g_current_thread->state == THREAD_RUNNING);
	return (thread_t *)g_current_thread;
}

void
threading_setup(void)
{
	TAILQ_INIT(&kernel_threads);

	thread_t *idle = thread_create("idle", idle_thread, NULL, 0);
	assert(idle != NULL);

	thread_set_current(idle);
}

thread_t *
thread_create(const char *name,
		kernel_thread_start_routine_t start_func,
		void *start_arg,
		uint8_t priority)
{
	uint32_t flags;
	thread_t *new_thread;

	if (!start_func)
		return NULL;

	// Allocate a new thread structure for the current running thread
	new_thread = malloc(sizeof(thread_t));

	if (!new_thread)
		return NULL;

	// Initialize the thread attributes
	strzcpy(new_thread->name, ((name)?name:"[NONAME]"), THREAD_MAX_NAMELEN);
	new_thread->state = THREAD_CREATED;

	// Allocate the stack for the new thread
	new_thread->stack_base_address	= (uint32_t)malloc(THREAD_KERNEL_STACK_SIZE);
	new_thread->stack_size		= THREAD_KERNEL_STACK_SIZE;
	new_thread->priority		= priority;

	if (!new_thread->stack_base_address)
	{
		free(new_thread);
		return NULL;
	}

	// Initialize the CPU context of the new thread
	cpu_kstate_init(&new_thread->cpu_state,
			(cpu_kstate_function_arg1_t *)start_func,
			(uint32_t)start_arg,
			new_thread->stack_base_address,
			new_thread->stack_size,
			(cpu_kstate_function_arg1_t *)thread_exit,
			(uint32_t)NULL);

	// Add the thread in the global list
	X86_IRQs_DISABLE(flags);
	TAILQ_INSERT_TAIL(&kernel_threads, new_thread, next);
	X86_IRQs_ENABLE(flags);

	// Mark the thread as ready
	scheduler_insert_thread(new_thread);

	return new_thread;
}

void
thread_exit(void)
{
	uint32_t flags;

	X86_IRQs_DISABLE(flags);

	TAILQ_REMOVE(&kernel_threads, g_current_thread, next);

	free((void *)g_current_thread->stack_base_address);
	kmemset((void *)g_current_thread, 0, sizeof(thread_t));
	free((void *)g_current_thread);

	scheduler_remove_thread((thread_t *)g_current_thread);
	thread_t *new_current_thread = scheduler_elect_new_current_thread();
	thread_set_current(new_current_thread);

	X86_IRQs_ENABLE(flags);

	schedule();
}
