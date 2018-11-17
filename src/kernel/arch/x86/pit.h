/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 *
 * @see [en] i82C54's datasheet
 * @see [en] http://www.osdever.net/bkerndev/Docs/pit.htm
 * @see [en] http://www.jamesmolloy.co.uk/tutorial_html/5.-IRQs and the PIT.html
 *
 * Programmable Interrupt Timer.
 */

#include <lib/types.h>

/**
 * Changes timer interrupt frequency from the default one (18.222 Hz)
 *
 * @param frequency Frequency at which interrupts whill be raised
 * @return status Status indicating the success of operation
 */
status_t x86_pit_set_frequency(uint32_t frequency);

/**
 * Timer's interrupt handler called periodically
 *
 * @param id
 */
void timer_interrupt_handler(int number);

void PitWait(uint32_t ms);
