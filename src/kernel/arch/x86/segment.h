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

/*
 * Builds a value for a segment register
 */
#define X86_BUILD_SEGMENT_REGISTER_VALUE(segment_index) \
	( (0) /* Descriptor Priviledge Level 0 */       \
	| (0) /* Not in LDT */                          \
| ((segment_index) << 3) )
