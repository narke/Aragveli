/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <arch/x86-pc/vbe.h>

/**
 * Assert an expression
 *
 * @param expression The expression to evaluate.
 */
#define assert(expression)				\
	({						\
	 	int result = (int)(expression);		\
		if (!result)				\
		{					\
			/* Disable interrupts on x86 */	\
			asm("cli\n");			\
			vbe_set_color(NORMAL_RED);	\
			vbe_draw_string("Asserted!");	\
			/* Infinite loop and x86 processor halting */	\
			while (1) asm("hlt");				\
		}							\
	})

