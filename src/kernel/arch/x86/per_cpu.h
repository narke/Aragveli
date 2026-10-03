/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>
#include <arch/x86/acpi.h>
#include "gdt.h"

struct thread;

struct cpu
{
	struct cpu 	*self;		// offset 0, %gs:0
	struct thread	*current;	// offset 4
	uint32_t	 id;		// index 0..n-1
	uint32_t	 apic_id;
	struct thread	*idle;
	uint32_t	 boot_stack_top;
	struct thread	*prev;
	volatile uint32_t tlb_pending;

	struct x86_gdt_entry	gdt[GDT_ENTRIES] __attribute__((aligned(8)));
	struct x86_tss		tss;
};

extern struct cpu g_cpus[MAX_CPU_COUNT];
extern volatile uint32_t g_online_cpus; // bit i set = g_cpus[i] is running

static inline struct cpu *
this_cpu(void)
{
	struct cpu *cpu;
	asm volatile("movl %%gs:0, %0" : "=r"(cpu));
	return cpu;

}

static inline struct thread *
this_cpu_current(void)
{
	struct thread *current_thread;

	asm volatile("movl %%gs:4, %0" : "=r"(current_thread));
	return current_thread;
}
