#include <lib/c/stdlib.h>
#include <lib/c/string.h>
#include <lib/status.h>
#include <arch/x86/paging.h>
#include <memory/frame.h>

#include "process.h"
#include "elf_loader.h"
#include "thread.h"

#define USER_STACK_TOP 0xBFFFF000

static int g_next_pid = 1;

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

	p->pid            = g_next_pid++;
	p->state          = PROC_LIVE;
	p->page_directory = pd;
	p->entry          = (uint32_t)entry;
	p->user_stack_top = USER_STACK_TOP + PAGE_SIZE;

	thread_t *t = thread_user_create(path, p);

	if (!t)
		goto fail_proc;

	return p;

fail_proc:
	free(p);
fail_pd_mapped:
	/* stack_frame (and ELF pages) are mapped in pd; destroy frees them. */
fail_pd:
	page_directory_destroy(pd);
	return NULL;
}
