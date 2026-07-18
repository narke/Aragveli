/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>
#include "thread.h"

void scheduler_setup(void);
void scheduler_insert_thread(thread_t * t);
void scheduler_remove_thread(thread_t *t);
void scheduler_start(void) __attribute__((noreturn));
void schedule(void);
void scheduler_switch_to_next(thread_t *dying) __attribute__((noreturn));
