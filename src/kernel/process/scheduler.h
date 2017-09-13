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
void scheduler_set_ready(thread_t * thr);
void schedule(void);
