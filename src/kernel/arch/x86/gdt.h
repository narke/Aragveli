/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <lib/types.h>
#include <lib/c/string.h>
#include <lib/c/stdbool.h>
#include "segment.h"

// Describes a GDT entry
struct x86_gdt_entry
{
        /*
	 * Lowest dword
	 */
        // Segment Limit: bits 15:0
        uint16_t segment_limit_15_0;
        // Base Address: bits 15..0
        uint16_t base_paged_address_15_0;

        /*
	 * Highest dword
	 */
        // Base Address: bits 23..16
        uint8_t base_paged_address_23_16;
        // Segment Type (code/data)
        uint8_t segment_type:			4;
        // 0 = system, 1 = Code/Data
        uint8_t descriptor_type:		1;
        // Descriptor Privilege Level
        uint8_t descriptor_privilege_level:	2;
        // Segment Present
        uint8_t segment_present:		1;
        // Segment Limit: bits 19..16
        uint8_t segment_limit_19_16:		4;
        // Available for any use
        uint8_t available:			1;
        uint8_t zero:				1;
        // 0=16 bits instructions, 1=32 bits
        uint8_t operand_size:			1;
        // 0=limit in bytes, 1=limit in pages
        uint8_t granularity:			1;
        // Base address bits 31..24
        uint8_t base_paged_address_31_24;
} __attribute__((packed, aligned(8)));


/** Describes the GDTR register */
struct x86_gdtr
{
	// The limit address represents the maximal offset of the GDTR register
	uint16_t  limit;

	/* The base (linear, in paged memory) address represents
	 * the starting address of the GDTR register */
	uint32_t linear_base_address;
} __attribute__((packed, aligned(8)));


struct x86_tss
{
	uint16_t back_link;

	uint16_t reserved1;

	vaddr_t	 esp0;
	uint16_t ss0;

	uint16_t reserved2;

	vaddr_t  esp1;
	uint16_t ss1;

	uint16_t reserved3;

	vaddr_t  esp2;
	uint16_t ss2;

	uint16_t reserved4;

	vaddr_t cr3;
	vaddr_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;

	// +72
	uint16_t es;
	uint16_t reserved5;

	// +76
	uint16_t cs;
	uint16_t reserved6;

	// +80
	uint16_t ss;
	uint16_t reserved7;

	// +84
	uint16_t ds;
	uint16_t reserved8;

	// +88
	uint16_t fs;
	uint16_t reserved9;

	// +92
	uint16_t gs;
	uint16_t reserved10;

	// +96
	uint16_t ldtr;
	uint16_t reserved11;

	// +100
	uint16_t debug_trap_flag :1;
	uint16_t reserved12      :15;
	uint16_t iomap_base_addr;
} __attribute__((packed, aligned(128)));

struct cpu;

void gdt_setup_cpu(struct cpu *cpu);
void set_kernel_stack(uint32_t stack);
