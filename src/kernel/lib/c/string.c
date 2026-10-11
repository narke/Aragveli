/*
 * Copyright (c) 2017, 2026 Konstantin Tcholokachvili
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

	return dst;
}

void *
memcpy(void *dst, const void *src, size_t size) /* Flawfinder: ignore */
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
	char *_dst;
	const char *_src;

	if (dst == NULL || src == NULL)
		return NULL;

	if (dst_size < src_size)
		return NULL;

	_dst = (char *)dst;
	_src = (const char *)src;

	while (src_size > 0)
	{
		*_dst = *_src;

		_dst++;
		_src++;
		src_size--;
	}

	return dst;
}

int
memcmp(const void *s1, const void *s2, size_t n)
{
	const unsigned char *a = (const unsigned char *)s1;
	const unsigned char *b = (const unsigned char *)s2;

	for (; n > 0; a++, b++, n--)
	{
		if (*a != *b)
		{
			return *a - *b;
		}
	}

	return 0;
}

void *
memmove(void *dst, const void *src, size_t size)
{
	char *_dst       = (char *)dst;
	const char *_src = (const char *)src;

	if (_dst < _src)
	{
		while (size--)
		{
			*_dst++ = *_src++;
		}
	}
	else if (_dst > _src)
	{
		_dst += size;
		_src += size;

		while (size--)
		{
			*--_dst = *--_src;
		}
	}

	return dst;
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
strlen(const char *s)
{
	size_t length = 0;

	while (s[length])
	{
		length++;
	}

	return length;
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
	for (; n > 0; s1++, s2++, n--)
	{
		if (*s1 != *s2)
		{
			return (unsigned char)*s1 - (unsigned char)*s2;
		}

		if (*s1 == '\0')
		{
			return 0;
		}
	}

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

int
atoi(const char *s)
{
	int n = 0;
	int sign = 1;

	while (*s == ' ' || *s == '\t')
	{
		s++;
	}

	if (*s == '-' || *s == '+')
	{
		if (*s++ == '-')
		{
			sign = -1;
		}
	}

	while (*s >= '0' && *s <= '9')
	{
		n = n * 10 + (*s++ - '0');
	}

	return sign * n;
}
