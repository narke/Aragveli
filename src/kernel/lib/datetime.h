#pragma once

#include <lib/types.h>

struct datetime
{
	uint16_t year;		/* 0 if unknown */
	uint8_t  month;		/* 1..12 */
	uint8_t  day;		/* 1..31 */
	uint8_t  hour;		/* 0..23 */
	uint8_t  minute;
	uint8_t  second;
};
