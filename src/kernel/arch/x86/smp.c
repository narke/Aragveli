/*
 * Copyright (c) 2018, 2026 Konstantin Tcholokachvili.
 * Copyright (c) 2012 Patrick Doane and others.  See AUTHORS file for list.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it freely,
 * subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim
 * that you wrote the original software. If you use this software in a product,
 * an acknowledgment in the product documentation would be appreciated but is
 * not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <lib/c/stdio.h>
#include <lib/c/string.h>
#include <lib/c/stdlib.h>
#include <lib/c/assert.h>
#include <memory/frame.h>
#include <arch/x86/paging.h>
#include <arch/x86/per_cpu.h>
#include <arch/x86/atomic.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <process/thread.h>
#include <process/scheduler.h>
#include "acpi.h"
#include "lapic.h"
#include "pit.h"
#include "smp.h"

extern uint8_t ap_trampoline_start[], ap_trampoline_end[];
extern uint32_t tramp_cr3;
extern uint32_t tramp_stacks[];

static uint32_t *
tramp_ptr(void *sym)
{
    return (uint32_t *)PA2VA(AP_TRAMPOLINE_BASE
        + (uint32_t)((uint8_t *)sym - ap_trampoline_start));
}

static void
smp_prepare_trampoline(void)
{
    size_t size = (size_t)(ap_trampoline_end - ap_trampoline_start);
    uint32_t *stacks;

    assert(size <= PAGE_SIZE);

    memcpy_s(PA2VA(AP_TRAMPOLINE_BASE), PAGE_SIZE, ap_trampoline_start, size);

    *tramp_ptr(&tramp_cr3) = page_directory_kernel();

    stacks = tramp_ptr(tramp_stacks);

    for (uint32_t i = 1; i < g_acpiCpuCount && i < MAX_CPU_COUNT; i++)
    {
        uint8_t *stack = malloc(THREAD_KERNEL_STACK_SIZE);

        assert(stack != NULL);

        stacks[i] = (uint32_t)(stack + THREAD_KERNEL_STACK_SIZE);
        g_cpus[i].boot_stack_top = stacks[i];
    }
}

void SmpInit()
{
    kprintf("Waking up all CPUs\n");

    smp_prepare_trampoline();

    g_activeCpuCount = 1;
    uint32_t localId = LocalApicGetId();

    // Send Init to all cpus except self
    for (uint32_t i = 0; i < g_acpiCpuCount; ++i)
    {
        uint32_t apicId = g_acpiCpuIds[i];

        if (apicId != localId)
        {
            LocalApicSendInit(apicId);
        }
    }

    // wait
    PitWait(10);

    // Send Startup twice to all cpus except self
    for (uint32_t round = 0; round < 2; ++round)
    {
	    for (uint32_t i = 0; i < g_acpiCpuCount; ++i)
	    {
		uint32_t apicId = g_acpiCpuIds[i];

		if (apicId != localId)
		{
		    LocalApicSendStartup(apicId, AP_TRAMPOLINE_BASE >> 12);
		}
	    }

	    PitWait(1);
    }

    // Wait for all cpus to be active
    while (g_activeCpuCount != (atomic_count_t)g_acpiCpuCount)
    {
        kprintf("Waiting... %ld\n", g_activeCpuCount);
        PitWait(1);
    }

    kprintf("All CPUs activated\n");
}

void
ap_main(uint32_t index)
{
    struct cpu *cpu = &g_cpus[index];

    gdt_setup_cpu(cpu);
    x86_idt_load();
    LocalApicInit();

    cpu->id      = index;
    cpu->apic_id = LocalApicGetId();

    threading_setup_cpu();
    LocalApicTimerInit(100);

    atomic_set_bit(&g_online_cpus, index);
    atomic_inc(&g_activeCpuCount);

    scheduler_start();
}

void
lapic_timer_handler(void)
{
    LocalApicEoi();
    schedule();
}

void
resched_handler(void)
{
    LocalApicEoi();
    schedule();
}
