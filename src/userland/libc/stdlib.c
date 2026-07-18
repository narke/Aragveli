/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include "stdlib.h"

#define SYS_EXIT 0
#define SYS_WAIT 2
#define SYS_EXEC 3
#define SYS_FORK 5

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

int
exec(const char *path, char *const argv[])
{
	int ret;

	asm volatile("int $0x80"
		: "=a"(ret)
		: "a"(SYS_EXEC), "b"(path), "c"(argv)
		: "memory");
	return ret;
}

int
fork(void)
{
	int pid;

	asm volatile("int $0x80"
		: "=a"(pid)
		: "a"(SYS_FORK)
		: "memory");
	return pid;
}
