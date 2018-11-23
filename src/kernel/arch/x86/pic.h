/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 *
 * Programmable Interruptr Controller.
 * @see Intel 8259A datasheet
 */

#pragma once

#include <lib/types.h>
#include "irq.h"

// Setup PIC and Disable all IRQ lines.
void x86_pic_disable(void);

void x86_pic_enable_irq_line(uint32_t irq_number);

void x86_pic_disable_irq_line(uint32_t irq_number);
