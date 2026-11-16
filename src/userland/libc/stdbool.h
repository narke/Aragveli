/*
 * Copyright (c) 2018-2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/* bool/true/false are keywords in C23 */
#define __bool_true_false_are_defined 1
#else
typedef enum { false = 0, true = 1 } bool;
#define __bool_true_false_are_defined 1
#endif
