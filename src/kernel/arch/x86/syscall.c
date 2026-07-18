/*
 * Copyright (c) 2018 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/status.h>
#include <fs/vfs.h>
#include <memory/frame.h>
#include <process/process.h>
#include <process/thread.h>
#include <drivers/vbe.h>
#include <drivers/ps2_keyboard.h>
#include "idt.h"
#include "paging.h"
#include "syscall.h"

#define SYSCALL_INTERRUPT 0x80

#define EXEC_PATH_MAX	256
#define EXEC_ARG_MAX	32
#define EXEC_ARG_LEN	256
#define WRITE_MAX	4096
#define READ_MAX	4096

extern struct superblock *root_fs;

// A system call handler receives the saved user frame and returns a value
// that is written back into the user's eax.
typedef uint32_t (*syscall_t)(struct syscall_frame *frame);

static int
copy_user_byte(uint32_t pd, uint32_t uaddr, uint8_t *out)
{
	uint32_t frame;

	if (uaddr >= KERNEL_VIRTUAL_BASE)
		return -1;

	frame = page_lookup(pd, uaddr);
	if (!frame)
		return -1;

	*out = *((uint8_t *)PA2VA(frame) + (uaddr & PAGE_MASK));
	return 0;
}

static int
write_user_byte(uint32_t pd, uint32_t uaddr, uint8_t value)
{
	uint32_t frame;

	if (uaddr >= KERNEL_VIRTUAL_BASE)
		return -1;

	frame = page_lookup(pd, uaddr);
	if (!frame)
		return -1;

	*((uint8_t *)PA2VA(frame) + (uaddr & PAGE_MASK)) = value;
	return 0;
}

static int
copy_user_string(uint32_t pd, uint32_t uaddr, char *dst, size_t maxlen)
{
	size_t i;

	if (!uaddr || !dst || maxlen == 0)
		return -1;

	for (i = 0; i < maxlen; i++)
	{
		uint8_t b;

		if (copy_user_byte(pd, uaddr + i, &b) != 0)
			return -1;

		dst[i] = (char)b;
		if (b == '\0')
			return 0;
	}

	return -1;	/* not NUL-terminated within maxlen */
}

static int
copy_user_u32(uint32_t pd, uint32_t uaddr, uint32_t *out)
{
	uint32_t value = 0;

	for (size_t i = 0; i < sizeof(uint32_t); i++)
	{
		uint8_t b;

		if (copy_user_byte(pd, uaddr + i, &b) != 0)
			return -1;

		value |= (uint32_t)b << (i * 8);
	}

	*out = value;
	return 0;
}

static uint32_t
sys_exit(struct syscall_frame *frame)
{
	process_t *p = thread_get_current()->process;

	process_exit(p, (int)frame->ebx);
	return 0;	/* not reached */
}

static uint32_t
sys_write(struct syscall_frame *frame)
{
	process_t *p = thread_get_current()->process;
	int fd = (int)frame->ebx;
	uint32_t buf = frame->ecx;
	uint32_t len = frame->edx;
	uint32_t i;

	if (!p)
		return (uint32_t)-1;

	if (fd < 0 || fd >= PROC_NFDS || p->fds[fd] != FD_CONSOLE)
		return (uint32_t)-1;

	if (len == 0)
		return 0;

	if (!buf || len > WRITE_MAX)
		return (uint32_t)-1;

	for (i = 0; i < len; i++)
	{
		uint8_t c;

		if (copy_user_byte(p->page_directory, buf + i, &c) != 0)
			return (uint32_t)-1;

		vbe_draw_character((char)c);
	}

	return len;
}

static uint32_t
sys_read(struct syscall_frame *frame)
{
	process_t *p = thread_get_current()->process;
	int fd = (int)frame->ebx;
	uint32_t buf = frame->ecx;
	uint32_t len = frame->edx;
	uint32_t i;

	if (!p)
		return (uint32_t)-1;

	if (fd < 0 || fd >= PROC_NFDS || p->fds[fd] != FD_CONSOLE)
		return (uint32_t)-1;

	if (len == 0)
		return 0;

	if (!buf || len > READ_MAX)
		return (uint32_t)-1;

	for (i = 0; i < len; i++)
	{
		uint8_t c;

		keyboard_read(&c, 1);
		if (write_user_byte(p->page_directory, buf + i, c) != 0)
			return (uint32_t)-1;
	}

	return len;
}

static uint32_t
sys_wait(struct syscall_frame *frame)
{
	process_t *p = thread_get_current()->process;
	int status = 0;
	int pid;

	if (!p)
		return (uint32_t)-1;

	pid = process_wait(p, &status);

	if (pid >= 0 && frame->ebx != 0)
		*(int *)frame->ebx = status;

	return (uint32_t)pid;
}

static uint32_t
sys_exec(struct syscall_frame *frame)
{
	process_t *p = thread_get_current()->process;
	uint32_t pd;
	char path[EXEC_PATH_MAX];
	char arg_storage[EXEC_ARG_MAX][EXEC_ARG_LEN];
	char *argv[EXEC_ARG_MAX + 1];
	int argc = 0;
	uint32_t uargv;
	uint32_t uarg;
	int ret;

	if (!p || !p->page_directory || !root_fs || !root_fs->root)
		return (uint32_t)-1;

	pd = p->page_directory;

	if (copy_user_string(pd, frame->ebx, path, sizeof(path)) != 0)
		return (uint32_t)-1;

	uargv = frame->ecx;
	if (!uargv)
		return (uint32_t)-1;

	for (argc = 0; argc < EXEC_ARG_MAX; argc++)
	{
		if (copy_user_u32(pd, uargv + (uint32_t)argc * sizeof(uint32_t),
				  &uarg) != 0)
			return (uint32_t)-1;

		if (uarg == 0)
			break;

		if (copy_user_string(pd, uarg, arg_storage[argc], EXEC_ARG_LEN) != 0)
			return (uint32_t)-1;

		argv[argc] = arg_storage[argc];
	}

	if (argc == EXEC_ARG_MAX)
	{
		/* Require a NULL terminator within the limit. */
		if (copy_user_u32(pd, uargv + (uint32_t)argc * sizeof(uint32_t),
				  &uarg) != 0 || uarg != 0)
			return (uint32_t)-1;
	}

	argv[argc] = NULL;

	ret = process_exec_elf(p, path, argv, root_fs->root);
	if (ret != 0)
		return (uint32_t)-1;	/* not reached on post-clear failure */

	frame->eip = p->entry;
	frame->user_esp = p->user_stack_top;
	return 0;
}

static syscall_t syscall_table[] = {
	[SYS_EXIT]  = sys_exit,
	[SYS_WRITE] = sys_write,
	[SYS_WAIT]  = sys_wait,
	[SYS_EXEC]  = sys_exec,
	[SYS_READ]  = sys_read,
};

#define SYSCALL_COUNT (sizeof(syscall_table) / sizeof(syscall_table[0]))

void
syscall_dispatch(struct syscall_frame *frame)
{
	uint32_t number = frame->eax;

	if (number < SYSCALL_COUNT && syscall_table[number] != NULL)
	{
		frame->eax = syscall_table[number](frame);
	}
	else
	{
		kprintf("Unknown syscall: %d\n", (int)number);
		frame->eax = (uint32_t)-1;
	}
}

void
system_calls_setup(void)
{
	extern void syscall_stub(void);

	// DPL 3 so ring 3 code is allowed to trigger the gate.
	status_t ret = x86_idt_set_handler(SYSCALL_INTERRUPT,
			(uint32_t)syscall_stub, 3);

	if (ret != KERNEL_OK)
	{
		kprintf("Syscall idt error\n");
	}
}
