/*
 * Copyright (c) 2017 Konstantin Tcholokachvili
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <drivers/vbe.h>
#include <lib/types.h>
#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "stdbool.h"

static void
print_hex(unsigned long int number)
{
	char buffer[16];
	int i = 0;

	while (number != 0)
	{
		int n = number % 16;
		buffer[i++] = (char)(n < 10 ? n + '0' : n + 87);
		number /= 16;
	}

	for (i = i - 1; i >= 0; i--)
		vbe_draw_character(buffer[i]);
}

static void
__vprintf(const char *fmt, va_list args)
{
	bool format_modifiers = false;
	bool prefix_long = false;

	while (*fmt)
	{
		if (*fmt != '%' && !format_modifiers)
		{
			vbe_draw_character(*fmt++);
			continue;
		}
		else if (*fmt == '%')
		{
			fmt++; // skip '%'
			prefix_long = false;
		}

		switch(*fmt++)
		{
			case 'c':
				{
					int c = va_arg(args, int);
					vbe_draw_character((char)c);
				}
				break;

			case 'l':
				prefix_long = true;
				format_modifiers = true;
				break;

			case 'd':
				if (prefix_long)
				{
					long int integer = va_arg(args, long int);
					int i = 0;
					char buffer_int[32];

					if (integer < 0)
						vbe_draw_character('-');

					do
					{
						int modulo10 = integer % 10;
						modulo10 = (modulo10 < 0) ? -modulo10 : modulo10;
						buffer_int[i++] = (char)('0' + modulo10);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
						vbe_draw_character(buffer_int[i]);
				}
				else
				{
					int integer = va_arg(args, int);
					int i = 0;
					char buffer_int[16];

					if (integer < 0)
						vbe_draw_character('-');

					do
					{
						int modulo10 = integer % 10;
						modulo10 = (modulo10 < 0) ? -modulo10 : modulo10;
						buffer_int[i++] = (char)('0' + modulo10);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
						vbe_draw_character(buffer_int[i]);
				}
				format_modifiers = false;
				break;

			case 'u':
				if (prefix_long)
				{
					unsigned long int integer =
						va_arg(args, unsigned long int);
					int i = 0;
					char buffer_long[32];

					do
					{
						unsigned int modulo10 = integer % 10;
						buffer_long[i++] = (char)('0' + modulo10);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
						vbe_draw_character(buffer_long[i]);
				}
				else
				{
					unsigned int integer =
						va_arg(args, unsigned int);
					int i = 0;
					char buffer_long[16];

					do
					{
						unsigned int modulo10 = integer % 10;
						buffer_long[i++] = (char)('0' + modulo10);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
						vbe_draw_character(buffer_long[i]);
				}
				format_modifiers = false;
				break;

			case 'p':
				vbe_draw_character('0');
				vbe_draw_character('x');
				print_hex((unsigned long int)(uintptr_t)va_arg(args, void *));
				format_modifiers = false;
				break;

			case 'x':
				{
					unsigned long int number;

					if (prefix_long)
						number = va_arg(args, unsigned long int);
					else
						number = va_arg(args, unsigned int);

					print_hex(number);
				}
				format_modifiers = false;
				break;

			case 's':
				{
					char *s = va_arg(args, char *);

					if (s == NULL)
					{
						vbe_draw_character('(');
						vbe_draw_character('n');
						vbe_draw_character('u');
						vbe_draw_character('l');
						vbe_draw_character('l');
						vbe_draw_character(')');
					}
					else
					{
						while (*s)
							vbe_draw_character(*s++);
					}
				}
				break;

			case 'n':
				vbe_draw_character('%');
				vbe_draw_character('n');
				break;

			case '%':
				vbe_draw_character('%');
				break;

			default:
				vbe_draw_character('%');
				vbe_draw_character(*(fmt - 1));
				format_modifiers = false;
				break;

		}
	}
}

void
vbe_console_format(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	__vprintf(fmt, args);
	va_end(args);
}
