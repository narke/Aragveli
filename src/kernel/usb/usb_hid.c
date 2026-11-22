#include <lib/c/stdio.h>
#include <drivers/ps2_keyboard.h>
#include "tusb.h"

static const uint8_t keycode_to_ascii[128][2] = { HID_KEYCODE_TO_ASCII };
static hid_keyboard_report_t prev_report;

static bool
key_in_report(const hid_keyboard_report_t *report, uint8_t keycode)
{
	for (int i = 0; i < 6; i++)
	{
		if (report->keycode[i] == keycode)
		{
			return true;
		}
	}

	return false;
}

static void
keyboard_report(const hid_keyboard_report_t *report)
{
	bool shift = (report->modifier &
		(KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0;

	for (int i = 0; i < 6; i++)
	{
		uint8_t keycode = report->keycode[i];

		// Reports list every held key: only act on newly pressed ones
		if (keycode == 0 || keycode >= 128 ||
		    key_in_report(&prev_report, keycode))
		{
			continue;
		}

		char c = (char)keycode_to_ascii[keycode][shift ? 1 : 0];

		if (c == '\r')
		{
			c = '\n';	// PS/2 keymap uses '\n' for Enter
		}

		if (c)
		{
			keyboard_push(c);
		}
	}

	prev_report = *report;
}

void
tuh_hid_mount_cb(uint8_t dev_addr, uint8_t idx,
		uint8_t const *report_desc, uint16_t desc_len)
{
	(void)report_desc;
	(void)desc_len;

	if (tuh_hid_interface_protocol(dev_addr, idx) != HID_ITF_PROTOCOL_KEYBOARD)
	{
		return;
	}

	kprintf("USB keyboard connected\n");
	tuh_hid_receive_report(dev_addr, idx);
}

void
tuh_hid_umount_cb(uint8_t dev_addr, uint8_t idx)
{
	(void)dev_addr;
	(void)idx;

	memset(&prev_report, 0, sizeof(prev_report));
}

void
tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t idx,
		uint8_t const *report, uint16_t len)
{
	if (tuh_hid_interface_protocol(dev_addr, idx) == HID_ITF_PROTOCOL_KEYBOARD &&
	    len >= sizeof(hid_keyboard_report_t))
	{
		keyboard_report((const hid_keyboard_report_t *)report);
	}

	// Re-arm, or no further reports are polled
	tuh_hid_receive_report(dev_addr, idx);
}
