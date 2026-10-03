/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>

#define TLB_SHOOTDOWN_VECTOR	0xFD
#define TLB_FLUSH_ALL		0xFFFFFFFFu

static inline void
tlb_flush_page(vaddr_t va)
{
	asm volatile("invlpg (%0)" :: "r"(va) : "memory");
}

static inline void
tlb_flush_all(void)
{
	uint32_t cr3;

	asm volatile("movl %%cr3, %0\n\tmovl %0, %%cr3"
		: "=r"(cr3) :: "memory");
}

/* Flush [start, end) on every CPU in the cpus bitmask; end == TLB_FLUSH_ALL flushes everything. */
void tlb_shootdown(uint32_t cpus, vaddr_t start, vaddr_t end);
void tlb_shootdown_poll(void);
void tlb_shootdown_irq(void);
