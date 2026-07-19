/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "stdint.h"

#define NULL ((void *)0)

typedef uint32_t size_t;

#define STRLEN_MAX 65536u

size_t strlen(const char *str);
size_t strnlen(const char *str, size_t maxlen);
