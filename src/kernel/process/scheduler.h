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
thread_t *scheduler_elect_new_current_thread(void);
void schedule(void);
