/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 *
 * See IA-32 Intel Architecture Software Developer's Manual, Volume 3 [Chapter 3]
 *
 * Note The chosen segmentation mechanism model is Basic Flat Model as Paging
 * allows closer control
 */

#pragma once

#include <lib/types.h>
#include <lib/status.h>

// Mapping of the IRQ lines in the IDT.
#define X86_IRQ_BASE	32
#define X86_IRQ_NUMBER	16
#define X86_IRQ_MAX	X86_IRQ_BASE + X86_IRQ_NUMBER - 1

// Setup the interrupt descriptor table.
void x86_idt_setup(void);

/**
 * Set an interrupt handler with a callback.
 *
 * @param index 		Index in IDT
 * @param handler_address	The callback which will be called
 * @return KERNEL_OK on success or KERNEL_INVALID_VALUE on failure
 */
status_t x86_idt_set_handler(uint32_t index, uint32_t handler_address);
