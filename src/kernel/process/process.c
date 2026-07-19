#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <lib/status.h>
#include <arch/x86/paging.h>
#include <arch/x86/syscall.h>
#include <memory/frame.h>

#include "process.h"
#include "elf_loader.h"
#include "thread.h"
#include "scheduler.h"

/* Highest stack page; stack grows down through USER_STACK_PAGES pages. */
#define USER_STACK_TOP		0xBFFFF000
#define USER_STACK_PAGES	2
#define USER_STACK_BASE		(USER_STACK_TOP - (USER_STACK_PAGES - 1) * PAGE_SIZE)
#define USER_STACK_ARG_MAX	32

static int g_next_pid = 1;
static process_t *g_init_process = NULL;

void
process_init(void)
{
	process_t *init = malloc(sizeof(process_t));

	assert(init != NULL);
	memset(init, 0, sizeof(*init));

	init->pid   = g_next_pid++;
	init->ppid  = 0;
	init->state = PROC_LIVE;
	LIST_INIT(&init->children);
	TAILQ_INIT(&init->waiters);

	g_init_process = init;
}

process_t *
process_get_init(void)
{
	return g_init_process;
}

void
process_wake_waiters(process_t *parent)
{
	thread_t *t;

	if (!parent)
		return;

	while ((t = TAILQ_FIRST(&parent->waiters)) != NULL)
	{
		TAILQ_REMOVE(&parent->waiters, t, next);
		scheduler_insert_thread(t);
	}
}

int
process_wait(process_t *parent, int *status)
{
	process_t *child;

	if (!parent)
		return -1;

	for (;;)
	{
		LIST_FOREACH(child, &parent->children, sibling)
		{
			if (child->state == PROC_ZOMBIE)
			{
				int pid = child->pid;

				if (status)
					*status = child->exit_status;

				LIST_REMOVE(child, sibling);
				free(child);
				return pid;
			}
		}

		if (LIST_EMPTY(&parent->children))
			return -1;

		thread_t *current = thread_get_current();

		current->state = THREAD_BLOCKED;
		scheduler_remove_thread(current);
		TAILQ_INSERT_TAIL(&parent->waiters, current, next);
		schedule();
	}
}

/*
 * Map USER_STACK_PAGES pages and write a System V i386 argc/argv/envp image
 * at the high end. Returns user ESP pointing at argc, or 0 on failure.
 *
 * Several pages are required: libc printf() uses a 4KiB on-stack buffer, and
 * argc/argv already occupy the top of the highest page.
 */
static uint32_t
user_stack_build(uint32_t pd, char *const argv[])
{
	int argc = 0;
	size_t str_bytes = 0;
	size_t lens[USER_STACK_ARG_MAX];
	uint32_t top_frame = 0;
	uint8_t *top_page;
	uint32_t str_va;
	uint32_t *words;
	size_t ptr_bytes;
	size_t total;
	uint32_t page_top;
	uint32_t sp;

	if (!argv)
		return 0;

	for (int i = 0; argv[i] != NULL; i++)
	{
		if (argc >= USER_STACK_ARG_MAX)
			return 0;

		lens[argc] = strnlen(argv[i], PAGE_SIZE) + 1;
		str_bytes += lens[argc];
		argc++;
	}

	/* argc + argv ptrs + NULL + envp NULL + strings, 16-byte aligned. */
	ptr_bytes = (size_t)(argc + 3) * sizeof(uint32_t);
	total = ptr_bytes + str_bytes;
	page_top = USER_STACK_TOP + PAGE_SIZE;

	if (total > PAGE_SIZE)
		return 0;

	sp = (page_top - (uint32_t)total) & ~0xfu;
	if (sp < USER_STACK_TOP)
		return 0;

	for (unsigned i = 0; i < USER_STACK_PAGES; i++)
	{
		uint32_t va = USER_STACK_BASE + i * PAGE_SIZE;
		uint32_t frame = frame_alloc();

		if (!frame)
			return 0;

		memset(PA2VA(frame), 0, PAGE_SIZE);

		if (page_map_user(pd, va, frame, PAGE_RW) != KERNEL_OK)
		{
			frame_free(frame);
			return 0;
		}

		if (va == USER_STACK_TOP)
			top_frame = frame;
	}

	if (!top_frame)
		return 0;

	top_page = (uint8_t *)PA2VA(top_frame);
	str_va = sp + (uint32_t)ptr_bytes;
	words = (uint32_t *)(top_page + (sp - USER_STACK_TOP));

	words[0] = (uint32_t)argc;

	for (int i = 0; i < argc; i++)
	{
		memcpy(top_page + (str_va - USER_STACK_TOP), argv[i], lens[i]);
		words[1 + i] = str_va;
		str_va += (uint32_t)lens[i];
	}

	words[1 + argc] = 0;	/* argv terminator */
	words[2 + argc] = 0;	/* empty envp */

	return sp;
}

int
process_image_load(uint32_t pd, const char *path, char *const argv[],
		   struct node *root, uint32_t *entry_out, uint32_t *esp_out)
{
	struct node *file;
	void *entry;
	uint32_t esp;

	if (!pd || !path || !argv || !root || !entry_out || !esp_out)
		return -1;

	file = resolve_node(path, root);
	if (!file || file->type != TMPFS_FILE)
		return -1;

	/* Initrd file data is a physical address; use the higher-half map so
	 * this works under a process CR3 (no low identity mapping). */
	entry = elf_load_file(PA2VA((uint32_t)(uintptr_t)file->u.file.data),
			      file->u.file.size, pd);
	if (!entry)
		return -1;

	esp = user_stack_build(pd, argv);
	if (!esp)
		return -1;

	*entry_out = (uint32_t)entry;
	*esp_out = esp;
	return 0;
}

void
process_exit(process_t *p, int status)
{
	p->exit_status = status;
	p->state = PROC_ZOMBIE;

	page_directory_switch(page_directory_kernel());
	if (p->page_directory)
	{
		page_directory_destroy(p->page_directory);
		p->page_directory = 0;
	}

	process_wake_waiters(p->parent);
	thread_exit();
}

int
process_exec_elf(process_t *p, const char *path, char *const argv[],
		 struct node *root)
{
	uint32_t entry;
	uint32_t esp;

	if (!p || !p->page_directory || !path || !argv || !root)
		return -1;

	page_directory_clear_user(p->page_directory);

	if (process_image_load(p->page_directory, path, argv, root,
			       &entry, &esp) != 0)
		process_exit(p, -1);

	p->entry = entry;
	p->user_stack_top = esp;
	/* Reload CR3 to flush stale user TLB entries after remap. */
	page_directory_switch(p->page_directory);
	return 0;
}

int
process_fork(process_t *parent, struct syscall_frame *frame)
{
	uint32_t pd;
	process_t *child;
	thread_t *t;
	int pid;

	if (!parent || !frame || !parent->page_directory)
		return -1;

	pd = page_directory_clone(parent->page_directory);
	if (!pd)
		return -1;

	child = malloc(sizeof(process_t));
	if (!child)
	{
		page_directory_destroy(pd);
		return -1;
	}

	memset(child, 0, sizeof(*child));

	pid = g_next_pid++;
	child->pid = pid;
	child->ppid = parent->pid;
	child->parent = parent;
	child->state = PROC_LIVE;
	child->exit_status = 0;
	child->page_directory = pd;
	memcpy(child->fds, parent->fds, sizeof(child->fds));
	LIST_INIT(&child->children);
	TAILQ_INIT(&child->waiters);

	LIST_INSERT_HEAD(&parent->children, child, sibling);

	t = thread_fork_create("fork", child, frame);
	if (!t)
	{
		LIST_REMOVE(child, sibling);
		page_directory_destroy(pd);
		free(child);
		return -1;
	}

	return pid;
}

process_t *
process_create_from_elf(const char *path, struct node *root)
{
	uint32_t pd;
	uint32_t entry;
	uint32_t esp;
	char *argv[2];
	process_t *p;
	process_t *parent;
	thread_t *current;
	thread_t *t;

	if (!path || !root)
		return NULL;

	pd = page_directory_create();
	if (!pd)
		return NULL;

	argv[0] = (char *)path;
	argv[1] = NULL;

	if (process_image_load(pd, path, argv, root, &entry, &esp) != 0)
		goto fail_pd;

	p = malloc(sizeof(process_t));
	if (!p)
		goto fail_pd;

	memset(p, 0, sizeof(*p));

	parent = NULL;
	current = thread_get_current();

	if (current && current->process)
		parent = current->process;
	else
		parent = process_get_init();

	assert(parent != NULL);

	p->pid            = g_next_pid++;
	p->ppid           = parent->pid;
	p->parent         = parent;
	p->state          = PROC_LIVE;
	p->exit_status    = 0;
	p->page_directory = pd;
	p->entry          = entry;
	p->user_stack_top = esp;
	p->fds[0]         = FD_CONSOLE;
	p->fds[1]         = FD_CONSOLE;
	p->fds[2]         = FD_CONSOLE;
	LIST_INIT(&p->children);
	TAILQ_INIT(&p->waiters);

	LIST_INSERT_HEAD(&parent->children, p, sibling);

	t = thread_user_create(path, p);
	if (!t)
		goto fail_proc;

	return p;

fail_proc:
	LIST_REMOVE(p, sibling);
	free(p);
fail_pd:
	page_directory_destroy(pd);
	return NULL;
}
