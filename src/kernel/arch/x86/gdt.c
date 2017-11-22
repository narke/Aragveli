/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/types.h>
#include "gdt.h"
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


static struct x86_gdt_entry gdt[] = {
	[NULL_SEGMENT]  = (struct x86_gdt_entry){ 0, },
	[KERNEL_CODE_SEGMENT] = (struct x86_gdt_entry){
		.segment_limit_15_0		= 0xffff,
		.base_paged_address_15_0	= 0,
		.base_paged_address_23_16	= 0,
		.segment_type			= 0xb,	// Code segment
		.descriptor_type		= 1,	// Code/data
		.descriptor_privilege_level	= 0,
		.segment_present		= 1,
		.segment_limit_19_16		= 0xf,
		.available			= 0,
		.zero				= 0,
		.operand_size			= 1,	// 32-bit
		.granularity			= 1,	// 4KB pages
		.base_paged_address_31_24	= 0
	},
	[KERNEL_DATA_SEGMENT] = (struct x86_gdt_entry){
		.segment_limit_15_0		= 0xffff,
		.base_paged_address_15_0	= 0,
		.base_paged_address_23_16	= 0,
		.segment_type			= 0x3,	// Data segment
		.descriptor_type		= 1,	// Code/data
		.descriptor_privilege_level	= 0,
		.segment_present		= 1,
		.segment_limit_19_16		= 0xf,
		.available			= 0,
		.zero				= 0,
		.operand_size			= 1,	// 32-bit
		.granularity			= 1,	// 4KB pages
		.base_paged_address_31_24	= 0
	}

};


void x86_gdt_setup(void)
{
	struct x86_gdtr gdtr;

	// GDT's starting address
	gdtr.linear_base_address = (uint32_t)gdt;
	// GDT's upper limit address
	gdtr.limit = sizeof(gdt) - 1;

	/*
	 * Load GDT into GDTR register and update segment register.
	 * The CS register may only be updated with a long jump
	 * to an absolute address in the given segment
	 * (Intel x86 manual, volume 3, section 4.8.1)
	 */
	asm volatile (
		"lgdt %0	  \n\
                 ljmp %1, $1f     \n\
                 1:               \n\
                 movw %2,    %%ax \n\
                 movw %%ax,  %%ss \n\
                 movw %%ax,  %%ds \n\
                 movw %%ax,  %%es \n\
                 movw %%ax,  %%fs \n\
                 movw %%ax,  %%gs"
		:
		:"m"(gdtr),
		 "i"(X86_BUILD_SEGMENT_REGISTER_VALUE(KERNEL_CODE_SEGMENT)),
		 "i"(X86_BUILD_SEGMENT_REGISTER_VALUE(KERNEL_DATA_SEGMENT))
		 :"memory","eax");
}
