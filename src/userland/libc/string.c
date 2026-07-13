/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include "string.h"

unsigned int
strlen(register const char *str)
{
	unsigned int retval = 0;

	while (*str++)
		retval++;

	return retval;
}
