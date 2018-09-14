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

/**
 * Write a value to an I/O port.
 *
 * @param value
 * @param port
 * @return None
 */
static inline void outb(uint16_t port, uint8_t value)
{
	asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value)
{
	asm volatile("outw %0, %1" :: "a"(value), "Nd"(port));
}

static inline void outd(uint16_t port, uint32_t value)
{
	asm volatile("outd %0, %1" :: "a"(value), "Nd"(port));
}

static inline void outl(uint32_t port, uint32_t value)
{
	asm volatile("outl %0, %w1" :: "a"(value), "Nd"(port));
}


/**
 * Read one byte from I/O port.
 *
 * @param port
 * @return value
 */
static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("inb %1, %0"
			: "=a"(ret)
			: "Nd"(port));
	return ret;
}

static inline uint16_t inw(uint16_t port)
{
	uint16_t ret;
	asm volatile("inw %1, %0"
			: "=a"(ret)
			: "Nd"(port));
	return ret;
}

static inline uint32_t ind(uint16_t port)
{
	uint32_t ret;
	asm volatile("ind %1, %0"
			: "=a"(ret)
			: "Nd"(port));
	return ret;
}

static inline uint32_t inl(uint32_t port)
{
	uint32_t data;
	asm volatile("inl %w1, %0"
			: "=a"(data)
			: "Nd"(port));
	return data;
}
