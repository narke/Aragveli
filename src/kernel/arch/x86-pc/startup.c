/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86-pc/bootstrap/multiboot.h>
#include <lib/types.h>
#include <lib/status.h>
#include <lib/libc.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <arch/x86/isr.h>
#include <arch/x86/irq.h>
#include <arch/x86/pit.h>

// The kernel entry point. All starts from here!
void
aragveli_main(uint32_t magic, uint32_t address)
{
    multiboot_info_t *mbi;
    mbi = (multiboot_info_t *)address;

    (void)magic;
    (void)mbi;

    // GDT
    x86_gdt_setup();

    // IDT
    x86_idt_setup();

    // ISRs
    x86_isr_setup();

    // IRQs
    x86_irq_setup();

    // Timer: Raise IRQ0 at 100 HZ rate.
    status_t status = x86_pit_set_frequency(100);

    assert(status == KERNEL_OK);

    // Timer interrupt
    x86_irq_set_routine(IRQ_TIMER, timer_interrupt_handler);

    *((unsigned char *)0xB8000) = 'A';
    *((unsigned char *)0xB8001) = 0x1A;

    // Enable interrupts
    asm volatile("sti");
}
