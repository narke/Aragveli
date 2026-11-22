/*
 * Copyright (c) 2018-2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

typedef unsigned long long int	uint64_t;
typedef unsigned long int	uint32_t;
typedef unsigned short int	uint16_t;
typedef unsigned char		uint8_t;

typedef signed long long int    int64_t;
typedef signed long int         int32_t;
typedef signed short int        int16_t;
typedef signed char             int8_t;

typedef unsigned long		uintptr_t;

typedef signed long		intptr_t;

#define INT8_MIN	(-128)
#define INT8_MAX	127
#define UINT8_MAX	255

#define INT16_MIN	(-32767 - 1)
#define INT16_MAX	32767
#define UINT16_MAX	65535

/* 32-bit types are 'long' here, hence the L suffixes */
#define INT32_MIN	(-2147483647L - 1)
#define INT32_MAX	2147483647L
#define UINT32_MAX	4294967295UL

#define INT64_MIN	(-9223372036854775807LL - 1)
#define INT64_MAX	9223372036854775807LL
#define UINT64_MAX	18446744073709551615ULL

#define INTPTR_MIN	INT32_MIN
#define INTPTR_MAX	INT32_MAX
#define UINTPTR_MAX	UINT32_MAX
#define SIZE_MAX	UINT32_MAX

#define INT8_C(c)	c
#define INT16_C(c)	c
#define INT32_C(c)	c ## L
#define INT64_C(c)	c ## LL
#define UINT8_C(c)	c
#define UINT16_C(c)	c
#define UINT32_C(c)	c ## UL
#define UINT64_C(c)	c ## ULL
