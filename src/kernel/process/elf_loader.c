/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/stdbool.h>
#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <lib/status.h>
#include <memory/frame.h>


#include "elf_loader.h"

#define PT_LOAD 1

typedef struct {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

extern void jump_to_user_mode(uint32_t entry, uint32_t user_stack_top);

bool
elf_check_file(Elf32_Ehdr *hdr)
{
	if (!hdr)
	{
		return false;
	}

	if (hdr->e_ident[EI_MAG0] != ELFMAG0)
	{
		kprintf("ELF Header EI_MAG0 incorrect.\n");
		return false;
	}

	if (hdr->e_ident[EI_MAG1] != ELFMAG1)
	{
		kprintf("ELF Header EI_MAG1 incorrect.\n");
		return false;
	}

	if (hdr->e_ident[EI_MAG2] != ELFMAG2)
	{
		kprintf("ELF Header EI_MAG2 incorrect.\n");
		return false;
	}

	if (hdr->e_ident[EI_MAG3] != ELFMAG3)
	{
		kprintf("ELF Header EI_MAG3 incorrect.\n");
		return false;
	}

	return true;
}

bool
elf_check_supported(Elf32_Ehdr *hdr)
{
	if (!elf_check_file(hdr))
	{
		kprintf("Invalid ELF File.\n");
		return false;
	}

	if (hdr->e_ident[EI_CLASS] != ELFCLASS32)
	{
		kprintf("Unsupported ELF File Class.\n");
		return false;
	}

	if (hdr->e_ident[EI_DATA] != ELFDATA2LSB)
	{
		kprintf("Unsupported ELF File byte order.\n");
		return false;
	}

	if(hdr->e_machine != EM_386)
	{
		kprintf("Unsupported ELF File target.\n");
		return false;
	}
	if (hdr->e_ident[EI_VERSION] != EV_CURRENT)
	{
		kprintf("Unsupported ELF File version.\n");
		return false;
	}

	if(hdr->e_type != ET_REL && hdr->e_type != ET_EXEC)
	{
		kprintf("Unsupported ELF File type.\n");
		return false;
	}

	return true;
}

static void *
elf_load_exec(Elf32_Ehdr *hdr, size_t size)
{
    Elf32_Phdr *ph = (Elf32_Phdr *)((uint8_t *)hdr + hdr->e_phoff);

    for (uint16_t i = 0; i < hdr->e_phnum; i++)
    {
        if (ph[i].p_type != PT_LOAD)
            continue;

        if (ph[i].p_offset + ph[i].p_filesz > size)
            return NULL;

        uint8_t *dest = (uint8_t *)(uintptr_t)ph[i].p_vaddr;

        if (ph[i].p_filesz)
            memcpy(dest, (uint8_t *)hdr + ph[i].p_offset, ph[i].p_filesz);

        if (ph[i].p_memsz > ph[i].p_filesz)
            memset(dest + ph[i].p_filesz, 0, ph[i].p_memsz - ph[i].p_filesz);
    }

    return (void *)(uintptr_t)hdr->e_entry;
}

void *
elf_load_file(void *file, size_t size)
{
	Elf32_Ehdr *hdr = (Elf32_Ehdr *)file;

	if (!elf_check_supported(hdr))
	{
		kprintf("ELF File cannot be loaded.\n");
		return NULL;
	}

	switch (hdr->e_type)
	{
		case ET_EXEC:
			return elf_load_exec(hdr, size);
		case ET_REL:
			return NULL;
	}

	return NULL;
}

status_t
elf_exec(const char *path, struct node *root)
{
    struct node *file = resolve_node(path, root);

    if (!file || file->type != TMPFS_FILE)
        return -KERNEL_NO_SUCH_FILE_OR_FOLDER;

    void *entry = elf_load_file(file->u.file.data, file->u.file.size);

    if (!entry)
        return -KERNEL_INVALID_VALUE;

    uint32_t user_stack = frame_alloc();
    jump_to_user_mode((uint32_t)entry, user_stack + 0x1000);

    return KERNEL_OK;
}

