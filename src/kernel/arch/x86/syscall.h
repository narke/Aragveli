/*
 * Copyright (c) 2018 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

// System call numbers (passed in eax).
#define SYS_EXIT	0
#define SYS_WRITE	1
#define SYS_WAIT	2
#define SYS_EXEC	3
#define SYS_READ	4

/**
 * Register state as saved by syscall_stub (see syscall-entry.asm).
 *
 * The layout matches the push order of the stub: pusha followed by the
 * data segment registers, then the interrupt frame pushed by the CPU.
 */
struct syscall_frame
{
	uint32_t gs, fs, es, ds;
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
	uint32_t eip, cs, eflags, user_esp, ss;
} __attribute__((packed));

// Install the int 0x80 gate (callable from ring 3).
void system_calls_setup(void);

// C-level system call dispatcher, called by syscall_stub.
void syscall_dispatch(struct syscall_frame *frame);
