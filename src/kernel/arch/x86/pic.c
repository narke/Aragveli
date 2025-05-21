/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86/io-ports.h>
#include <lib/status.h>

#include "pic.h"

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA    0x21
#define PIC_SLAVE_COMMAND  0xa0
#define PIC_SLAVE_DATA     0xa1

#define IRQ_BASE 0x20

void
x86_pic_disable(void)
{
	// Send ICW1: 8086 mode + NOT Single ctrl + call address interval=8.
	out8(PIC_MASTER_COMMAND, 0x11);
	out8(PIC_SLAVE_COMMAND, 0x11);

	// Send ICW2: ctrl base address/
	out8(PIC_MASTER_DATA, IRQ_BASE);
	out8(PIC_SLAVE_DATA, IRQ_BASE + 8);

	// Send ICW3 master: mask where slaves are connected.
	out8(PIC_MASTER_DATA, 0x4);
	// Send ICW3 slave: index where the slave is connected on master.
	out8(PIC_SLAVE_DATA, 0x2);

	// Send ICW4: 8086 mode, fully nested, not buffered, no implicit EOI.
	out8(PIC_MASTER_DATA, 0x1);
	out8(PIC_SLAVE_DATA, 0x1);

	// Send OCW1: Disable all IRQs
	out8(PIC_MASTER_DATA, 0xFF);
	out8(PIC_SLAVE_DATA, 0xFF);
}

void
x86_pic_enable_irq_line(uint32_t irq_number)
{
	if( irq_number < 8 )
		// IRQ on master PIC.
		out8(PIC_MASTER_DATA,
			(uint8_t)(in8(PIC_MASTER_DATA) & ~(1 << irq_number)));
	else
		//  IRQ on slave PIC.
		out8(PIC_SLAVE_DATA,
			(uint8_t)(in8(PIC_SLAVE_DATA) & ~(1 << (irq_number-8))));
}

void
x86_pic_disable_irq_line(uint32_t irq_number)
{
	if( irq_number < 8 )
		// IRQ on master PIC.
		out8((in8(PIC_MASTER_DATA) | (1 << irq_number)),
			PIC_MASTER_DATA);
	else
		// IRS on slave PIC.
		out8((in8(PIC_SLAVE_DATA) | (1 << (irq_number-8))),
			PIC_SLAVE_DATA);
}
