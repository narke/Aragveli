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
#define THREAD_KERNEL_STACK_SIZE PAGE_SIZE

#define THREAD_MAX_NAMELEN 32

/* Forward declaration */
struct thread;


/*
 * The possible states of a valid thread
 */
typedef enum
{
	THREAD_CREATED,
	THREAD_READY,
	THREAD_RUNNING,
	THREAD_BLOCKED,
	THREAD_ZOMBIE,
} thread_state;

/*
 * Definition of the function executed by a kernel thread
 */
typedef void (*kernel_thread_start_routine_t)(void *arg);

struct thread
{
	/* Kernel stack parameters */
	uint32_t	stack_base_address;
	uint32_t	stack_size;
	char		name[THREAD_MAX_NAMELEN];
	thread_state	state;
	struct cpu_state *cpu_state;
	TAILQ_ENTRY(thread) next;
};


/*
 * Initialize the subsystem responsible for thread management
 *
 * Initialize the primary kernel thread so that it can be handled
 * the same way as an ordinary thread created by thread_create().
 */
void threading_setup(void);
struct thread *thread_create(const char *name,
		kernel_thread_start_routine_t start_func,
		void *start_arg);
inline void thread_set_current(struct thread *current_thread);
struct thread *thread_get_current(void);
