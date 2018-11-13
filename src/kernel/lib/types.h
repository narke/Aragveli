/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/c/stdint.h>

typedef unsigned int		paddr_t;	// Physical address
typedef unsigned int		vaddr_t;	// Virtual address
typedef int			status_t;	// Return status
typedef uint32_t		size_t;		// Memory size of an object
typedef int32_t			ssize_t;	// Signed size of a memory object

typedef int32_t			atomic_count_t;
