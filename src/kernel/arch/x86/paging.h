/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/types.h>
#include <arch/x86-pc/vbe.h>
#include <arch/x86/registers.h>

#define VM_MAP_PROT_NONE	0
#define VM_MAP_PROT_READ	(1 << 0)
#define VM_MAP_PROT_WRITE	(1 << 1)

#define VM_MAP_ATOMIC		(1 << 31)
#define PAGING_MIRROR_VADDR	0x3fc00000
#define PAGING_MIRROR_SIZE	(1 << 22)

status_t paging_setup(paddr_t identity_mapping_start,
	paddr_t identity_mapping_end,
	struct vbe_mode_info *vbe_mode_info);

status_t
paging_map(paddr_t paddr, vaddr_t vaddr, bool is_user_page, uint32_t flags);

status_t paging_unmap(vaddr_t vaddr);

paddr_t paging_get_frame(vaddr_t vaddr);

void page_fault(struct registers *r);
