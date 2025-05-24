/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#define NULL_SEGMENT        0
#define KERNEL_CODE_SEGMENT 1
#define KERNEL_DATA_SEGMENT 2
#define USER_CODE_SEGMENT   3
#define USER_DATA_SEGMENT   4
#define TSS_SEGMENT         5

/*
 * Builds a value for a segment register
 */
#define X86_BUILD_SEGMENT_REGISTER_VALUE(descriptor_privilege_level, into_ldt, segment_index)	\
	( (((descriptor_privilege_level & 0x3)) << 0)	/* Descriptor Priviledge */		\
	| ((into_ldt & 1)                       << 2)	/* Into LDT? */				\
	| ((segment_index)                      << 3) )
