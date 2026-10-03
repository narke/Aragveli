#pragma once

#include <lib/types.h>
#include <lib/c/stdbool.h>
#include <arch/x86/irq.h>


typedef struct
{
	volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

void tlb_shootdown_poll(void);

static inline void
spinlock_init(spinlock_t *l)
{
	l->locked = 0;
}

static inline bool
spinlock_trylock(spinlock_t *l)
{
	uint32_t v = 1;

	asm volatile("xchgl %0, %1"
		: "+r"(v), "+m"(l->locked) :: "memory");

	return v == 0;
}

static inline void
spinlock_lock(spinlock_t *l)
{
	while (!spinlock_trylock(l))
	{
		while (l->locked)
		{
			/* A CPU spinning with IRQs off must still ack TLB shootdowns. */
			tlb_shootdown_poll();
			asm volatile("pause" ::: "memory");
		}
	}
}

static inline void
spinlock_unlock(spinlock_t *l)
{
	asm volatile("movl $0, %0" : "=m"(l->locked) :: "memory");
}

static inline uint32_t
spinlock_lock_irqsave(spinlock_t *l)
{
	uint32_t flags;

	X86_IRQs_DISABLE(flags);
	spinlock_lock(l);

	return flags;
}

static inline void
spinlock_unlock_irqrestore(spinlock_t *l, uint32_t flags)
{
	spinlock_unlock(l);
	X86_IRQs_ENABLE(flags);
}
