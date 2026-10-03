/*
 * Copyright (c) 2017, 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/types.h>
#include <lib/c/string.h>
#include <lib/c/stdbool.h>
#include "per_cpu.h"
#include "segment.h"
#include "gdt.h"

#define SEGMENT_TYPE_CODE	0xb	// Execute/read, accessed
#define SEGMENT_TYPE_DATA	0x3	// Read/write, accessed
#define SEGMENT_TYPE_TSS	0x9	// 32-bit available TSS

struct cpu g_cpus[MAX_CPU_COUNT];
volatile uint32_t g_online_cpus;

static const struct x86_gdt_entry gdt[GDT_ENTRIES] = {
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
	},
	[USER_CODE_SEGMENT] = (struct x86_gdt_entry){
		.segment_limit_15_0		= 0xffff,
		.base_paged_address_15_0	= 0,
		.base_paged_address_23_16	= 0,
		.segment_type			= 0xb,	// Code segment
		.descriptor_type		= 1,	// Code/data
		.descriptor_privilege_level	= 3,	// User privilege
		.segment_present		= 1,
		.segment_limit_19_16		= 0xf,
		.available			= 0,
		.zero				= 0,
		.operand_size			= 1,	// 32-bit
		.granularity			= 1,	// 4KB pages
		.base_paged_address_31_24	= 0
	},
	[USER_DATA_SEGMENT] = (struct x86_gdt_entry){
		.segment_limit_15_0		= 0xffff,
		.base_paged_address_15_0	= 0,
		.base_paged_address_23_16	= 0,
		.segment_type			= 0x3,	// Data segment
		.descriptor_type		= 1,	// Code/data
		.descriptor_privilege_level	= 3,	// User privilege
		.segment_present		= 1,
		.segment_limit_19_16		= 0xf,
		.available			= 0,
		.zero				= 0,
		.operand_size			= 1,	// 32-bit
		.granularity			= 1,	// 4KB pages
		.base_paged_address_31_24	= 0
	},
	[TSS_SEGMENT] = (struct x86_gdt_entry){
		0,
	}
};

static void
gdt_set_entry(
	struct x86_gdt_entry *entry,
	uint32_t base,
	uint32_t limit,
	uint8_t type,
	uint8_t privilege_level,
	bool code_or_data)
{
	bool in_pages = (limit > 0xfffff);

	if (in_pages)
	{
		limit >>= 12;
	}

	*entry = (struct x86_gdt_entry){
		.segment_limit_15_0		= limit & 0xffff,
		.base_paged_address_15_0	= base & 0xffff,
		.base_paged_address_23_16	= (base >> 16) & 0xff,
		.segment_type			= (uint32_t)type & 0xf,
		.descriptor_type		= code_or_data,
		.descriptor_privilege_level	= (uint32_t)privilege_level & 0x3,
		.segment_present		= 1,
		.segment_limit_19_16		= (limit >> 16) & 0xf,
		.available			= 0,
		.zero				= 0,
		// Code/data: 32-bit. TSS: this bit must be 0.
		.operand_size			= code_or_data,
		.granularity			= in_pages,
		.base_paged_address_31_24	= (uint8_t)(base >> 24)
	};
}

static void
gdt_load(const struct x86_gdt_entry *table, uint16_t size)
{
	struct x86_gdtr gdtr;
	gdtr.linear_base_address = (uint32_t)table;
	gdtr.limit = (uint16_t)(size - 1);

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
                 movw %3,    %%ax \n\
                 movw %%ax,  %%gs"
		:
		:"m"(gdtr),
		 "i"(X86_BUILD_SEGMENT_REGISTER_VALUE(0, false, KERNEL_CODE_SEGMENT)),
		 "i"(X86_BUILD_SEGMENT_REGISTER_VALUE(0, false, KERNEL_DATA_SEGMENT)),
		 "i"(X86_BUILD_SEGMENT_REGISTER_VALUE(0, false, PER_CPU_SEGMENT))
		 :"memory","eax");
}

void
gdt_setup_cpu(struct cpu *cpu)
{
	cpu->self = cpu;

	memcpy_s(cpu->gdt, sizeof(cpu->gdt), gdt, sizeof(gdt));

	memset(&cpu->tss, 0x0, sizeof(cpu->tss));

	cpu->tss.ss0 = X86_BUILD_SEGMENT_REGISTER_VALUE(0, false, KERNEL_DATA_SEGMENT);

	// I/O bitmap beyond the TSS limit: every port is denied to ring 3
	cpu->tss.iomap_base_addr = sizeof(struct x86_tss);

	gdt_set_entry(&cpu->gdt[TSS_SEGMENT], (uint32_t)&cpu->tss,
		0x67, SEGMENT_TYPE_TSS, 0, false);

	gdt_set_entry(&cpu->gdt[PER_CPU_SEGMENT], (uint32_t)cpu,
		sizeof(struct cpu) - 1, SEGMENT_TYPE_DATA, 0, true);

	gdt_load(cpu->gdt, sizeof(cpu->gdt));

	uint16_t tss_register_value = X86_BUILD_SEGMENT_REGISTER_VALUE(0, false, TSS_SEGMENT);

	asm volatile ("ltr %0"::"r"(tss_register_value));
}

void
set_kernel_stack(uint32_t stack)
{
	this_cpu()->tss.esp0 = stack;
}
