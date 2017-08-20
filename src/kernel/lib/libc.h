/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "types.h"

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
	 		*((unsigned char*)0xB8000) = 'B'; \
	 		*((unsigned char*)0xB8001) = 0x1A; \
			/* Infinite loop and x86 processor halting */	\
			while (1) asm("hlt");				\
		}							\
	})

