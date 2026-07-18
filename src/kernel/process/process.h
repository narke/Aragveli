#pragma once

#include <lib/types.h>
#include <lib/queue.h>
#include <fs/tarfs.h>

#include "thread.h"

typedef enum { PROC_LIVE, PROC_ZOMBIE } proc_state;

typedef struct process {
	int		pid;
	int		ppid;
	proc_state	state;
	int		exit_status;
	uint32_t	page_directory;	/* physical, matches page_* API */
	uint32_t	entry;		/* user-mode entry point (virtual address) */
	uint32_t	user_stack_top;
	thread_t	*thread;	/* the 1:1 kernel thread */
	struct process	*parent;
	LIST_HEAD(, process) children;
	LIST_ENTRY(process) sibling;	/* on parent's children */
	TAILQ_HEAD(, thread) waiters;	/* parents blocked in wait */
} process_t;

void process_init(void);
process_t *process_get_init(void);
process_t *process_create_from_elf(const char *path, struct node *root);
int process_image_load(uint32_t pd, const char *path, char *const argv[],
		       struct node *root, uint32_t *entry_out, uint32_t *esp_out);
int process_exec_elf(process_t *p, const char *path, char *const argv[],
		     struct node *root);
void process_exit(process_t *p, int status) __attribute__((noreturn));
int process_wait(process_t *parent, int *status);
void process_wake_waiters(process_t *parent);
