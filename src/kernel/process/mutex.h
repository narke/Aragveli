/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/queue.h>
#include <lib/types.h>
#include <process/thread.h>
#include "semaphore.h"

typedef struct
{
	thread_t		*owner;
	TAILQ_HEAD(, thread)	waitqueue;
} mutex_t;

mutex_t *mutex_create(void);
void mutex_destroy(mutex_t *mtx);
status_t mutex_lock(mutex_t *mtx);
status_t mutex_unlock(mutex_t *mtx);
