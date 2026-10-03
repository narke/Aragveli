/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <memory/frame.h>
#include <arch/x86/per_cpu.h>
#include <arch/x86/spinlock.h>
#include <arch/x86/atomic.h>
#include <arch/x86/lapic.h>
#include <arch/x86/irq.h>
#include "tlb.h"

#define TLB_FLUSH_ALL_THRESHOLD	32	// pages

static struct
{
	spinlock_t		lock;
	volatile vaddr_t	start;
	volatile vaddr_t	end;
	volatile atomic_count_t	pending;
} shootdown = { SPINLOCK_INIT, 0, 0, 0 };

static void
tlb_flush_range(vaddr_t start, vaddr_t end)
{
	if (end == TLB_FLUSH_ALL
		|| (end - start) / PAGE_SIZE > TLB_FLUSH_ALL_THRESHOLD)
	{
		tlb_flush_all();
		return;
	}

	for (vaddr_t va = start; va < end; va += PAGE_SIZE)
	{
		tlb_flush_page(va);
	}
}

void
tlb_shootdown_poll(void)
{
	uint32_t pending = 0;

	asm volatile("xchgl %0, %1"
		: "+r"(pending), "+m"(this_cpu()->tlb_pending) :: "memory");

	if (!pending)
	{
		return;
	}

	tlb_flush_range(shootdown.start, shootdown.end);
	atomic_dec(&shootdown.pending);
}

void
tlb_shootdown_irq(void)
{
	tlb_shootdown_poll();
	LocalApicEoi();
}

void
tlb_shootdown(uint32_t cpus, vaddr_t start, vaddr_t end)
{
	uint32_t flags;
	uint32_t self;
	uint32_t targets;
	atomic_count_t count = 0;

	X86_IRQs_DISABLE(flags);

	self = this_cpu()->id;

	if (cpus & (1u << self))
	{
		tlb_flush_range(start, end);
	}

	targets = cpus & g_online_cpus & ~(1u << self);

	if (targets)
	{
		spinlock_lock(&shootdown.lock);

		for (uint32_t i = 0; i < MAX_CPU_COUNT; i++)
		{
			if (targets & (1u << i))
			{
				count++;
			}
		}

		shootdown.start   = start;
		shootdown.end     = end;
		shootdown.pending = count;

		for (uint32_t i = 0; i < MAX_CPU_COUNT; i++)
		{
			if (!(targets & (1u << i)))
			{
				continue;
			}

			g_cpus[i].tlb_pending = 1;
			LocalApicSendIpi(g_cpus[i].apic_id, TLB_SHOOTDOWN_VECTOR);
		}

		while (atomic_read(&shootdown.pending) != 0)
		{
			tlb_shootdown_poll();
			asm volatile("pause" ::: "memory");
		}

		spinlock_unlock(&shootdown.lock);
	}

	X86_IRQs_ENABLE(flags);
}
