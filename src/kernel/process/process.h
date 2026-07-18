#pragma once

#include <lib/types.h>
#include <fs/tarfs.h>

#include "thread.h"

typedef enum { PROC_LIVE, PROC_ZOMBIE } proc_state;

typedef struct process {
    int          pid;
    proc_state   state;
    uint32_t     page_directory;   // physical, matches page_* API
    uint32_t     entry;            // user-mode entry point (virtual address)
    uint32_t     user_stack_top;
    thread_t    *thread;           // the 1:1 kernel thread
} process_t;

process_t *process_create_from_elf(const char *path, struct node *root);
