/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once


#define NULL ((void*)0)

typedef enum {false=0, true=1} bool;

typedef unsigned long long int	uint64_t;
typedef unsigned long int	uint32_t;
typedef unsigned short int	uint16_t;
typedef unsigned char		uint8_t;

typedef signed long long int    int64_t;
typedef signed long int         int32_t;
typedef signed short int        int16_t;
typedef signed char             int8_t;


typedef unsigned int		paddr_t;	// Physical address
typedef unsigned int		vaddr_t;	// Virtual address
typedef int			status_t;	// Return status
typedef uint32_t		size_t;		// Memory size of an object
typedef int32_t			ssize_t;	// Signed size of a memory object

typedef uint32_t		sysarg_t;
typedef int32_t			atomic_count_t;
