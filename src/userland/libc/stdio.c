/*
 * Copyright (c) 2026 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include "stdio.h"
#include "string.h"

#define SYS_WRITE 1
#define SYS_READ  4

static int
write(int fd, const void *buf, size_t len)
{
	int ret;
	asm volatile("int $0x80"
			: "=a"(ret)
			: "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len)
			: "memory");
	return ret;
}

int
read(int fd, void *buf, size_t len) /* Flawfinder: ignore */
{
	int ret;
	asm volatile("int $0x80"
			: "=a"(ret)
			: "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
			: "memory");
	return ret;
}

typedef struct {
	char   *buffer;
	size_t  size;
	size_t  len;
} fmt_buffer_t;

static void
fmt_putc(fmt_buffer_t *out, char c)
{
	if (out->len < out->size)
	{
		out->buffer[out->len] = c;
	}

	out->len++;
}

static void
fmt_puts(fmt_buffer_t *out, const char *s)
{
	if (s == NULL)
	{
		fmt_puts(out, "(null)");
		return;
	}

	while (*s)
	{
		fmt_putc(out, *s++);
	}
}

static void
print_hex_to_buf(fmt_buffer_t *out, unsigned long number)
{
	char digits[16]; /* Flawfinder: ignore */
	int i = 0;

	while (number != 0 && i < (int)sizeof(digits))
	{
		int n = number % 16;
		digits[i++] = (char)(n < 10 ? n + '0' : n + 87);
		number /= 16;
	}

	for (i = i - 1; i >= 0; i--)
	{
		fmt_putc(out, digits[i]);
	}
}

static void
vprintf_helper(fmt_buffer_t *out, const char *fmt, va_list args)
{
	bool format_modifiers = false;
	bool prefix_long = false;

	while (*fmt)
	{
		if (*fmt != '%' && !format_modifiers)
		{
			fmt_putc(out, *fmt++);
			continue;
		}
		else if (*fmt == '%')
		{
			fmt++; /* skip '%' */
			prefix_long = false;
		}

		switch (*fmt++)
		{
			case 'c':
			{
				int c = va_arg(args, int);
				fmt_putc(out, (char)c);
				break;
			}

			case 'l':
				prefix_long = true;
				format_modifiers = true;
				break;

			case 'd':
				if (prefix_long)
				{
					long integer = va_arg(args, long);
					int i = 0;
					char buf[32]; /* Flawfinder: ignore */

					if (integer < 0)
					{
						fmt_putc(out, '-');
					}

					do
					{
						int digit;

						if (i >= (int)sizeof(buf))
							break;

						digit = integer % 10;
						digit = (digit < 0) ? -digit : digit;
						buf[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buf[i]);
					}
				}
				else
				{
					int integer = va_arg(args, int);
					int i = 0;
					char buf[16]; /* Flawfinder: ignore */

					if (integer < 0)
					{
						fmt_putc(out, '-');
					}

					do
					{
						int digit;

						if (i >= (int)sizeof(buf))
							break;

						digit = integer % 10;
						digit = (digit < 0) ? -digit : digit;
						buf[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buf[i]);
					}
				}

				format_modifiers = false;
				break;

			case 'u':
				if (prefix_long)
				{
					unsigned long integer = va_arg(args, unsigned long);
					int i = 0;
					char buf[32]; /* Flawfinder: ignore */

					do
					{
						unsigned int digit;

						if (i >= (int)sizeof(buf))
							break;

						digit = integer % 10;
						buf[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buf[i]);
					}
				}
				else
				{
					unsigned int integer = va_arg(args, unsigned int);
					int i = 0;
					char buf[16]; /* Flawfinder: ignore */

					do
					{
						unsigned int digit;

						if (i >= (int)sizeof(buf))
							break;

						digit = integer % 10;
						buf[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buf[i]);
					}
				}
				format_modifiers = false;
				break;

			case 'p':
				fmt_putc(out, '0');
				fmt_putc(out, 'x');
				print_hex_to_buf(out, (unsigned long)(uintptr_t)va_arg(args, void *));
				format_modifiers = false;
				break;

			case 'x':
			{
				unsigned long number;

				if (prefix_long)
				{
					number = va_arg(args, unsigned long);
				}
				else
				{
					number = va_arg(args, unsigned int);
				}

				print_hex_to_buf(out, number);
				format_modifiers = false;
				break;
			}

			case 's':
			{
				char *s = va_arg(args, char *);
				fmt_puts(out, s);
				break;
			}

			case 'n':
				fmt_putc(out, '%');
				fmt_putc(out, 'n');
				break;

			case '%':
				fmt_putc(out, '%');
				break;

			default:
				fmt_putc(out, '%');
				fmt_putc(out, *(fmt - 1));
				format_modifiers = false;
				break;
		}
	}
}

int
vsnprintf(char *buffer, size_t size, const char *fmt, va_list ap) /* Flawfinder: ignore */
{
	fmt_buffer_t out = { .buffer = buffer, .size = size, .len = 0 };

	if (size > 0)
	{
		buffer[0] = '\0';
	}

	vprintf_helper(&out, fmt, ap);

	if (size > 0)
	{
		size_t term = out.len < size - 1 ? out.len : size - 1;
		buffer[term] = '\0';
	}

	return (int)out.len;
}

int
printf(const char *fmt, ...) /* Flawfinder: ignore */
{
	/* Filled only via vsnprintf(..., sizeof(buffer), ...); truncates, no overrun. */
	char buffer[4096]; /* Flawfinder: ignore */
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buffer, sizeof(buffer), fmt, args); /* Flawfinder: ignore */
	va_end(args);

	if (len < 0)
		return len;

	/* vsnprintf returns the untruncated length. */
	if ((size_t)len > sizeof(buffer) - 1)
		len = (int)(sizeof(buffer) - 1);

	return write(1, buffer, (size_t)len);
}

