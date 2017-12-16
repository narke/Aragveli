/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>
#include <lib/status.h>

struct cpu_state;

/*
 * The type of the functions passed as arguments to the Kernel thread
 * related functions.
 */
typedef void (cpu_kstate_function_arg1_t(uint32_t arg1));

void cpu_kstate_init(struct cpu_state **kctx,
		cpu_kstate_function_arg1_t *start_func,
		uint32_t start_arg,
		uint32_t stack_base,
		uint32_t stack_size,
		cpu_kstate_function_arg1_t *exit_func,
		uint32_t exit_arg);

void cpu_context_switch(struct cpu_state **from_ctx,
		struct cpu_state *to_ctx);
