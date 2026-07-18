/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86/acpi.h>
#include <arch/x86/idt.h>
#include <arch/x86/io-ports.h>
#include <arch/x86/ioapic.h>
#include <arch/x86/irq.h>
#include <lib/c/string.h>
#include <lib/queue.h>
#include <lib/types.h>
#include <process/scheduler.h>
#include <process/thread.h>

#include "ps2_keyboard.h"

#define KBD_DATA_PORT	0x60
#define KBD_STATUS_PORT	0x64
#define KBD_STATUS_OBF	0x01

#define RING_SIZE	256

#define SC_LSHIFT	0x2a
#define SC_RSHIFT	0x36

static const char keymap_unshifted[128] = {
	0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
	'\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
	'\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
	0,    ' ',
};

static const char keymap_shifted[128] = {
	0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
	'\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
	'\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
	0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
	0,    ' ',
};

static uint8_t ring[RING_SIZE];
static size_t ring_head;
static size_t ring_tail;
static size_t ring_count;

static int shift_down;
static int e0_prefix;
static TAILQ_HEAD(, thread) waiters;

static int
ring_push(uint8_t c)
{
	if (ring_count >= RING_SIZE)
		return -1;

	ring[ring_head] = c;
	ring_head = (ring_head + 1) % RING_SIZE;
	ring_count++;
	return 0;
}

static int
ring_pop(uint8_t *out)
{
	if (ring_count == 0)
		return -1;

	*out = ring[ring_tail];
	ring_tail = (ring_tail + 1) % RING_SIZE;
	ring_count--;
	return 0;
}

static void
wake_one_waiter(void)
{
	thread_t *t = TAILQ_FIRST(&waiters);

	if (!t)
		return;

	TAILQ_REMOVE(&waiters, t, next);
	scheduler_insert_thread(t);
}

static char
scancode_to_ascii(uint8_t sc)
{
	if (sc >= 128)
		return 0;

	if (shift_down)
		return keymap_shifted[sc];
	return keymap_unshifted[sc];
}

static void
keyboard_irq(int irq_level)
{
	uint8_t sc;
	char c;

	(void)irq_level;

	if (!(in8(KBD_STATUS_PORT) & KBD_STATUS_OBF))
		return;

	sc = in8(KBD_DATA_PORT);

	if (sc == 0xe0)
	{
		e0_prefix = 1;
		return;
	}

	/* Drop the scancode that follows an E0 prefix (extended keys). */
	if (e0_prefix)
	{
		e0_prefix = 0;
		return;
	}

	if (sc & 0x80)
	{
		uint8_t make = (uint8_t)(sc & 0x7f);

		if (make == SC_LSHIFT || make == SC_RSHIFT)
			shift_down = 0;
		return;
	}

	if (sc == SC_LSHIFT || sc == SC_RSHIFT)
	{
		shift_down = 1;
		return;
	}

	c = scancode_to_ascii(sc);
	if (!c)
		return;

	if (ring_push((uint8_t)c) == 0)
		wake_one_waiter();
}

static void
drain_buffer(void)
{
	while (in8(KBD_STATUS_PORT) & KBD_STATUS_OBF)
		(void)in8(KBD_DATA_PORT);
}

void
keyboard_setup(void)
{
	TAILQ_INIT(&waiters);

	drain_buffer();
	x86_irq_set_routine(IRQ_KEYBOARD, keyboard_irq);
	/* Unmask only after the handler is installed. */
	IoApicSetEntry(g_ioApicAddr, AcpiRemapIrq(IRQ_KEYBOARD),
			X86_IRQ_BASE + IRQ_KEYBOARD);
}

size_t
keyboard_read(void *buf, size_t len)
{
	uint8_t *dst = buf;
	size_t n = 0;

	if (!buf || len == 0)
		return 0;

	while (n < len)
	{
		uint8_t c;
		uint32_t flags;

		X86_IRQs_DISABLE(flags);
		while (ring_pop(&c) != 0)
		{
			thread_t *current = thread_get_current();

			current->state = THREAD_BLOCKED;
			scheduler_remove_thread(current);
			TAILQ_INSERT_TAIL(&waiters, current, next);
			X86_IRQs_ENABLE(flags);
			schedule();
			X86_IRQs_DISABLE(flags);
		}
		X86_IRQs_ENABLE(flags);

		dst[n++] = c;
	}

	return n;
}
