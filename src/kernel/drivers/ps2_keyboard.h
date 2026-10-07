/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

void keyboard_setup(void);

/* Blocking read into a kernel buffer. Returns bytes read (0 if len == 0). */
size_t keyboard_read(void *buf, size_t len);
