/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include "stdlib.h"

#define SYS_EXIT 0
#define SYS_WAIT 2

void
exit(int status)
{
	asm volatile("int $0x80" :: "a"(SYS_EXIT), "b"(status) : "memory");
	__builtin_unreachable();
}

int
wait(int *status)
{
	int pid;

	asm volatile("int $0x80"
		: "=a"(pid)
		: "a"(SYS_WAIT), "b"(status)
		: "memory");
	return pid;
}
