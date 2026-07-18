#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include <lib/c/assert.h>
#include <lib/status.h>
#include <arch/x86/paging.h>
#include <memory/frame.h>

#include "process.h"
#include "elf_loader.h"
#include "thread.h"
#include "scheduler.h"

#define USER_STACK_TOP 0xBFFFF000

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

process_t *
process_create_from_elf(const char *path, struct node *root)
{
	struct node *file = resolve_node(path, root);

	if (!file || file->type != TMPFS_FILE)
		return NULL;

	uint32_t pd = page_directory_create();

	if (!pd)
		return NULL;

	void *entry = elf_load_file(file->u.file.data, file->u.file.size, pd);

	if (!entry)
		goto fail_pd;

	uint32_t stack_frame = frame_alloc();

	if (!stack_frame)
		goto fail_pd;

	if (page_map_user(pd, USER_STACK_TOP, stack_frame, PAGE_RW) != KERNEL_OK)
	{
		frame_free(stack_frame);
		goto fail_pd;
	}

	process_t *p = malloc(sizeof(process_t));

	if (!p)
		goto fail_pd_mapped;

	memset(p, 0, sizeof(*p));

	process_t *parent = NULL;
	thread_t *current = thread_get_current();

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
	p->entry          = (uint32_t)entry;
	p->user_stack_top = USER_STACK_TOP + PAGE_SIZE;
	LIST_INIT(&p->children);
	TAILQ_INIT(&p->waiters);

	LIST_INSERT_HEAD(&parent->children, p, sibling);

	thread_t *t = thread_user_create(path, p);

	if (!t)
		goto fail_proc;

	return p;

fail_proc:
	LIST_REMOVE(p, sibling);
	free(p);
fail_pd_mapped:
	/* stack_frame (and ELF pages) are mapped in pd; destroy frees them. */
fail_pd:
	page_directory_destroy(pd);
	return NULL;
}
