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
#include <arch/x86/paging.h>
#include <arch/x86/gdt.h>
#include <arch/x86/syscall.h>
#include "thread.h"
#include "scheduler.h"
#include "process.h"

extern void enter_user_mode(uint32_t, uint32_t);

static TAILQ_HEAD(, thread) zombie_threads;

static volatile thread_t *g_current_thread = NULL;

static void
thread_reap(void)
{
	uint32_t flags;
	thread_t *z;

	X86_IRQs_DISABLE(flags);

	while ((z = TAILQ_FIRST(&zombie_threads)) != NULL)
	{
		TAILQ_REMOVE(&zombie_threads, z, zombie);
		free((void *)z->stack_base_address);
		free(z);
	}

	X86_IRQs_ENABLE(flags);
}

static void
idle_thread(uint32_t arg)
{
	(void)arg;

	for (;;)
	{
		thread_reap();
		asm volatile("sti; hlt" ::: "memory");
	}
}

static void
user_thread_entry(uint32_t arg)
{
	process_t *p = (process_t *)arg;

	page_directory_switch(p->page_directory);
	set_kernel_stack(p->thread->kernel_stack_top);
	enter_user_mode(p->entry, p->user_stack_top);
}

static thread_t *
thread_alloc_stack(const char *name)
{
	thread_t *t;

	t = malloc(sizeof(thread_t));
	if (!t)
		return NULL;

	memset(t, 0, sizeof(*t));
	strzcpy(t->name, ((name) ? name : "[NONAME]"), THREAD_MAX_NAMELEN);

	t->stack_base_address = (uint32_t)malloc(THREAD_KERNEL_STACK_SIZE);
	t->stack_size = THREAD_KERNEL_STACK_SIZE;

	if (!t->stack_base_address)
	{
		free(t);
		return NULL;
	}

	/* Needed before the first switch_to(): TSS.esp0 must be valid. */
	t->kernel_stack_top = t->stack_base_address + t->stack_size;
	return t;
}

static thread_t *
thread_alloc(const char *name,
		kernel_thread_start_routine_t start_func,
		void *start_arg)
{
	thread_t *new_thread;

	if (!start_func)
		return NULL;

	new_thread = thread_alloc_stack(name);
	if (!new_thread)
		return NULL;

	cpu_kstate_init(&new_thread->cpu_state,
			(cpu_kstate_function_arg1_t *)start_func,
			(uint32_t)start_arg,
			new_thread->stack_base_address,
			new_thread->stack_size,
			(cpu_kstate_function_arg1_t *)thread_exit,
			(uint32_t)NULL);

	return new_thread;
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
	/*
	 * BLOCKED is allowed: wait/sem set that state before schedule(),
	 * which still needs to identify the outgoing thread.
	 */
	assert(g_current_thread != NULL);
	assert(g_current_thread->state == THREAD_RUNNING
	    || g_current_thread->state == THREAD_BLOCKED);
	return (thread_t *)g_current_thread;
}

void
threading_setup(void)
{
	TAILQ_INIT(&zombie_threads);

	thread_t *idle = thread_kernel_create("idle", idle_thread, NULL);
	assert(idle != NULL);

	thread_set_current(idle);
}

thread_t *
thread_kernel_create(const char *name,
		kernel_thread_start_routine_t start_func,
		void *start_arg)
{
	thread_t *new_thread = thread_alloc(name, start_func, start_arg);

	if (!new_thread)
		return NULL;

	scheduler_insert_thread(new_thread);

	return new_thread;
}

thread_t *
thread_user_create(const char *name, struct process *process)
{
	thread_t *t;

	if (!process)
		return NULL;

	t = thread_alloc(name, user_thread_entry, process);
	if (!t)
		return NULL;

	t->process = process;
	process->thread = t;
	scheduler_insert_thread(t);

	return t;
}

static void
forkret(uint32_t frame_addr)
{
	syscall_fork_return((struct syscall_frame *)frame_addr);
}

thread_t *
thread_fork_create(const char *name, struct process *process,
		   struct syscall_frame *parent_frame)
{
	thread_t *t;
	struct syscall_frame *cframe;

	if (!process || !parent_frame)
		return NULL;

	t = thread_alloc_stack(name ? name : "[fork]");
	if (!t)
		return NULL;

	/* Frame at stack top; kstate uses the remainder below it. */
	cframe = (struct syscall_frame *)(t->kernel_stack_top
					  - sizeof(struct syscall_frame));
	memcpy(cframe, parent_frame, sizeof(*cframe));
	cframe->eax = 0;

	cpu_kstate_init(&t->cpu_state,
			(cpu_kstate_function_arg1_t *)forkret,
			(uint32_t)cframe,
			t->stack_base_address,
			t->stack_size - sizeof(struct syscall_frame),
			(cpu_kstate_function_arg1_t *)thread_exit,
			(uint32_t)NULL);

	t->process = process;
	process->thread = t;
	scheduler_insert_thread(t);

	return t;
}

void
thread_exit(void)
{
	uint32_t flags;

	X86_IRQs_DISABLE(flags);

	thread_t *self = (thread_t *)g_current_thread;

	if (self->process)
	{
		self->process->thread = NULL;
		self->process = NULL;
	}

	scheduler_remove_thread(self);

	self->state = THREAD_ZOMBIE;
	TAILQ_INSERT_TAIL(&zombie_threads, self, zombie);

	/* Never returns: switches to the next ready thread. */
	scheduler_switch_to_next(self);
}
