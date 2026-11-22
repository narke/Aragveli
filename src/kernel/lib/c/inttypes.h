/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/c/stdint.h>

/* int8_t/int16_t promote to int; int32_t/uint32_t/uintptr_t are 'long' */
#define PRId8   "d"
#define PRId16  "d"
#define PRId32  "ld"
#define PRId64  "lld"

#define PRIi8   "i"
#define PRIi16  "i"
#define PRIi32  "li"
#define PRIi64  "lli"

#define PRIu8   "u"
#define PRIu16  "u"
#define PRIu32  "lu"
#define PRIu64  "llu"

#define PRIx8   "x"
#define PRIx16  "x"
#define PRIx32  "lx"
#define PRIx64  "llx"

#define PRIX8   "X"
#define PRIX16  "X"
#define PRIX32  "lX"
#define PRIX64  "llX"

#define PRIdPTR "ld"
#define PRIuPTR "lu"
#define PRIxPTR "lx"
