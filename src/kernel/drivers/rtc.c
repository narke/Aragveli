/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <arch/x86/acpi.h>
#include <arch/x86/io-ports.h>
#include <arch/x86/spinlock.h>
#include <lib/c/string.h>
#include "rtc.h"

#define CMOS_ADDRESS		0x70
#define CMOS_DATA		0x71
#define CMOS_NMI_DISABLE	0x80

#define RTC_SECONDS		0x00
#define RTC_MINUTES		0x02
#define RTC_HOURS		0x04
#define RTC_DAY			0x07
#define RTC_MONTH		0x08
#define RTC_YEAR		0x09
#define RTC_STATUS_A		0x0A
#define RTC_STATUS_B		0x0B
#define RTC_STATUS_D		0x0D

#define RTC_A_UPDATING		0x80
#define RTC_B_24H		0x02
#define RTC_B_BINARY		0x04
#define RTC_HOUR_PM		0x80

#define RTC_MAX_SPINS		1000000
#define RTC_MAX_TRIES		5

struct rtc_raw
{
	uint8_t second, minute, hour, day, month, year, century;
};

/* Ports 0x70/0x71 are an index/data pair shared by all CPUs. */
static spinlock_t rtc_lock = SPINLOCK_INIT;

static uint8_t
cmos_read(uint8_t reg)
{
	out8(CMOS_ADDRESS, (uint8_t)(CMOS_NMI_DISABLE | reg));
	return in8(CMOS_DATA);
}

static void
rtc_read_raw(struct rtc_raw *r, uint8_t century_reg)
{
	/* An update takes under 2 ms; registers are inconsistent meanwhile. */
	for (unsigned int spins = 0;
		(cmos_read(RTC_STATUS_A) & RTC_A_UPDATING) && spins < RTC_MAX_SPINS;
		spins++)
		;

	r->second  = cmos_read(RTC_SECONDS);
	r->minute  = cmos_read(RTC_MINUTES);
	r->hour    = cmos_read(RTC_HOURS);
	r->day     = cmos_read(RTC_DAY);
	r->month   = cmos_read(RTC_MONTH);
	r->year    = cmos_read(RTC_YEAR);
	r->century = century_reg ? cmos_read(century_reg) : 0;
}

static uint8_t
bcd_to_bin(uint8_t v)
{
	return (uint8_t)((v & 0x0F) + (v >> 4) * 10);
}

status_t
rtc_read(struct datetime *dt)
{
	uint8_t century_reg = AcpiCenturyRegister();
	struct rtc_raw prev, cur;
	unsigned int tries = 0;
	uint8_t status_b;
	uint8_t hour;
	uint32_t flags;

	if (!dt)
	{
		return -KERNEL_INVALID_VALUE;
	}

	flags = spinlock_lock_irqsave(&rtc_lock);

	/* Read until two consecutive snapshots agree, so a tick can't split them. */
	rtc_read_raw(&cur, century_reg);
	do
	{
		prev = cur;
		rtc_read_raw(&cur, century_reg);
	} while (memcmp(&prev, &cur, sizeof(cur)) != 0 && ++tries < RTC_MAX_TRIES);

	status_b = cmos_read(RTC_STATUS_B);
	/* Leave the index on register D with NMI re-enabled. */
	out8(CMOS_ADDRESS, RTC_STATUS_D);

	spinlock_unlock_irqrestore(&rtc_lock, flags);

	hour = cur.hour & (uint8_t)~RTC_HOUR_PM;

	if (!(status_b & RTC_B_BINARY))
	{
		cur.second  = bcd_to_bin(cur.second);
		cur.minute  = bcd_to_bin(cur.minute);
		hour        = bcd_to_bin(hour);
		cur.day     = bcd_to_bin(cur.day);
		cur.month   = bcd_to_bin(cur.month);
		cur.year    = bcd_to_bin(cur.year);
		cur.century = bcd_to_bin(cur.century);
	}

	/* 12-hour mode: 12 AM is 0h, 12 PM is 12h. */
	if (!(status_b & RTC_B_24H))
	{
		hour %= 12;
		if (cur.hour & RTC_HOUR_PM)
			hour = (uint8_t)(hour + 12);
	}

	if (cur.second > 59 || cur.minute > 59 || hour > 23
		|| cur.day < 1 || cur.day > 31
		|| cur.month < 1 || cur.month > 12)
		return -KERNEL_INVALID_VALUE;

	dt->year   = (uint16_t)(cur.century ? cur.century * 100 + cur.year
					: 2000 + cur.year);
	dt->month  = cur.month;
	dt->day    = cur.day;
	dt->hour   = hour;
	dt->minute = cur.minute;
	dt->second = cur.second;

	return KERNEL_OK;
}
