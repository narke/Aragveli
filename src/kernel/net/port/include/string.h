#pragma once

#include <lib/c/string.h>

int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dst, const void *src, size_t n);
size_t strlen(const char *s);
