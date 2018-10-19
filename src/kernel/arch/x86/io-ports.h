/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 *
 * x86 I/O space access functions.
 */

#pragma once

#include <lib/types.h>

// Write a byte
static inline void outb(uint16_t port, uint8_t data)
{
	asm volatile("outb %0, %1" :: "a"(data), "dN"(port));
}

// Write a word
static inline void outw(uint16_t port, uint16_t data)
{
	asm volatile("outw %0, %1" :: "a"(data), "dN"(port));
}

// Write a double word
static inline void outdw(uint16_t port, uint32_t data)
{
	asm volatile("outl %%eax, %%dx" :: "dN"(port), "a"(data));
}

// Read a byte
static inline uint8_t inb(uint16_t port)
{
	uint8_t data;
	asm volatile("inb %1, %0"
			: "=a"(data)
			: "dN"(port));
	return data;
}

// Read a word
static inline uint16_t inw(uint16_t port)
{
	uint16_t data;
	asm volatile("inw %1, %0"
			: "=a"(data)
			: "dN"(port));
	return data;
}

// Read a double word
static inline uint32_t indw(uint16_t port)
{
	uint32_t data;
	asm volatile("inl %%dx, %%eax"
			: "=a"(data)
			: "dN"(port));
	return data;
}
