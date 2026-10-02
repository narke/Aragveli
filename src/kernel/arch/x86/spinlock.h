#pragma once

#include <lib/types.h>
#include <arch/x86/irq.h>


typedef struct
{
	volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void
spinlock_init(spinlock_t *l)
{
	l->locked = 0;
}

static inline void
spinlock_lock(spinlock_t *l)
{
	uint32_t v;

	for (;;)
	{
		v = 1;
		asm volatile("xchgl %0, %1"
			: "+r"(v), "+m"(l->locked) :: "memory");
		
		if (v == 0)
		{
			return;
		}

		while (l->locked)
		{
			asm volatile("pause" ::: "memory");
		}
	}
}

static inline void
spinlock_unlock(spinlock_t *l)
{
	asm volatile("movl $0, %0" : "=m"(l->locked) :: "memory");
}

#define spinlock_lock_irqsave(l, flags)		\
	do {					\
		X86_IRQs_DISABLE(flags);	\
		spinlock_lock(l);		\
	} while (0)

#define spinlock_unlock_irqsave(l, flags)	\
	do {					\
		spinlock_unlock(l);		\
		X86_IRQs_ENABLE(flags);	\
	} while (0)

