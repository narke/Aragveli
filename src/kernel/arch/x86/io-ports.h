/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 *
 * x86 I/O space access functions.
 */

#pragma once

/**
 * Write a value to an I/O port.
 *
 * @param value
 * @param port
 * @return None
 */
#define outb(port, value)				\
	asm volatile(					\
			"outb %b0,%w1"			\
			::"a" (value),"Nd" (port)	\
			)				\

/**
 * Read one byte from I/O port.
 *
 * @param port
 * @return value
 */
#define inb(port)				\
({						\
	unsigned char _value;			\
	asm volatile(				\
			"inb %w1,%0"		\
			:"=a" (_value)		\
			:"Nd" (port)		\
			);			\
 	_value;					\
})
