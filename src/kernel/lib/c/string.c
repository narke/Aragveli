/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include "string.h"

void *
memset(void *dst, int c, size_t length)
{
	char *p;

	for (p = (char *)dst; length > 0; p++, length--)
		*p = (char)c;

	return p;
}

void *
memcpy(void *dst, const void *src, size_t size)
{
	char *_dst       = (char *)dst;
	const char *_src = (const char *)src;

	while (size > 0)
	{
		*_dst = *_src;

		_dst++;
		_src++;
		size--;
	}

	return dst;
}

void *
memcpy_s(void *dst, size_t dst_size, const void *src, size_t src_size)
{
	if (dst == NULL || src == NULL)
		return NULL;

	if (dst_size < src_size)
		return NULL;

	return memcpy(dst, src, src_size);
}

char *
strzcpy(char *dst, const char *src, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
	{
		dst[i] = src[i];

		if(src[i] == '\0')
			return dst;
	}

	dst[len-1] = '\0';
	return dst;
}

size_t
strnlen(const char *s, size_t max_length)
{
	size_t length = 0;

	while (*s++ && length < max_length)
		length++;

	return length;
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
	uint16_t i = 0;

	for (i = 0; i < n && *s1++ && *s2++; i++)
		if (*s1 != *s2)
			return *s1 - *s2;
	return 0;
}

char *
strchr(const char *s, int c)
{
	while (*s++)
	{
		if (*s == (char)c)
			return (char *)s;
	}

	return NULL;
}

char *
strrchr(const char *s, int c)
{
	char *result = NULL;

	do
	{
		if (*s == c)
			result = (char *)s;
	} while (*s++);

	return result;
}
