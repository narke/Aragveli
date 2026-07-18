/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/queue.h>
#include <lib/types.h>
#include <arch/x86/cpu-context.h>
#include <memory/frame.h>


/*
 * The size of the stack of a kernel thread
 */
#define THREAD_KERNEL_STACK_SIZE (4 * PAGE_SIZE)

#define THREAD_MAX_NAMELEN 32

/*
 * The possible states of a valid thread
 */
typedef enum
{
	/* 0 = new thread after memset, not yet on the ready queue */
	THREAD_READY = 1,
	THREAD_RUNNING,
	THREAD_BLOCKED,
	THREAD_ZOMBIE,
} thread_state;

/*
 * Definition of the function executed by a kernel thread
 */
typedef void (*kernel_thread_start_routine_t)(uint32_t arg);

typedef struct thread
{
	/* Kernel stack parameters */
	paddr_t		stack_base_address;
	uint32_t	stack_size;
	char		name[THREAD_MAX_NAMELEN];
	thread_state	state;
	struct cpu_state *cpu_state;
	struct process  *process;
	uint32_t        kernel_stack_top;
	TAILQ_ENTRY(thread) next;		/* mutex/sem waitqueue */
	TAILQ_ENTRY(thread) sched_next;		/* ready queue */
	TAILQ_ENTRY(thread) zombie;
} thread_t;


void threading_setup(void);
thread_t *thread_kernel_create(const char *name,
		kernel_thread_start_routine_t start_func,
		void *start_arg);
thread_t *thread_user_create(const char *name, struct process *process);
void thread_exit(void) __attribute__((noreturn));
inline void thread_set_current(thread_t *current_thread);
thread_t *thread_get_current(void);
