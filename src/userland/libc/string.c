/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include "string.h"

size_t
strnlen(const char *str, size_t maxlen)
{
	size_t n = 0;

	if (!str)
		return 0;

	while (n < maxlen && str[n] != '\0')
		n++;

	return n;
}

size_t
strlen(const char *str)
{
	return strnlen(str, STRLEN_MAX);
}
