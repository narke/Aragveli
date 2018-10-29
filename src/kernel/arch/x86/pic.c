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
#define PIC_SLAVE_COMMAND  0xa0

void
x86_pic_setup(void)
{
	// Send ICW1: 8086 mode + NOT Single ctrl + call address interval=8.
	out8(PIC_MASTER_COMMAND, 0x11);
	out8(PIC_SLAVE_COMMAND, 0x11);

	// Send ICW2: ctrl base address/
	out8(PIC_MASTER_COMMAND+1, 0x20);
	out8(PIC_SLAVE_COMMAND+1, 0x28);

	// Send ICW3 master: mask where slaves are connected.
	out8(PIC_MASTER_COMMAND+1, 0x4);
	// Send ICW3 slave: index where the slave is connected on master.
	out8(PIC_SLAVE_COMMAND+1, 0x2);

	// Send ICW4: 8086 mode, fully nested, not buffered, no implicit EOI.
	out8(PIC_MASTER_COMMAND+1, 0x1);
	out8(PIC_SLAVE_COMMAND+1, 0x1);

	/* Send OCW1:
	 * Closing all IRQs : waiting for a correct handler The only IRQ
	 * enabled is the cascade (that's why we use 0xFB for the master) */
	out8(PIC_MASTER_COMMAND+1, 0xFB);
	out8(PIC_SLAVE_COMMAND+1, 0xFF);
}

void
x86_pic_enable_irq_line(uint32_t param_irq_number)
{
	if( param_irq_number < 8 )
		// IRQ on master PIC.
		out8(PIC_MASTER_COMMAND+1,
			(in8(PIC_MASTER_COMMAND+1) & ~(1 << param_irq_number)));
	else
		//  IRQ on slave PIC.
		out8(PIC_SLAVE_COMMAND+1,
			(in8(PIC_SLAVE_COMMAND+1) & ~(1 << (param_irq_number-8))));
}

void
x86_pic_disable_irq_line(uint32_t param_irq_number)
{
	if( param_irq_number < 8 )
		// IRQ on master PIC.
		out8((in8(PIC_MASTER_COMMAND+1) | (1 << param_irq_number)),
			PIC_MASTER_COMMAND+1);
	else
		// IRS on slave PIC.
		out8((in8(PIC_SLAVE_COMMAND+1) | (1 << (param_irq_number-8))),
			PIC_SLAVE_COMMAND+1);
}
