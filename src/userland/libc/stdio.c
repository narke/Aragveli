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

/* Must match kernel READ_MAX in syscall.c. */
#define READ_MAX 4096

static int
write(int fd, const void *buffer, size_t len)
{
	int ret;
	asm volatile("int $0x80"
			: "=a"(ret)
			: "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(len)
			: "memory");
	return ret;
}

int
read(int fd, void *buffer, size_t buffer_size)
{
	int ret;

	if (buffer_size == 0)
		return 0;

	if (fd < 0 || !buffer || buffer_size > READ_MAX)
		return -1;

	asm volatile("int $0x80"
			: "=a"(ret)
			: "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(buffer_size)
			: "memory");

	if (ret < 0 || (size_t)ret > buffer_size)
		return -1;

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
	char digits[16];
	int i = 0;

	while (number != 0)
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
					char buffer[32];

					if (integer < 0)
					{
						fmt_putc(out, '-');
					}

					do
					{
						int digit = integer % 10;
						digit = (digit < 0) ? -digit : digit;
						buffer[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buffer[i]);
					}
				}
				else
				{
					int integer = va_arg(args, int);
					int i = 0;
					char buffer[16];

					if (integer < 0)
					{
						fmt_putc(out, '-');
					}

					do
					{
						int digit = integer % 10;
						digit = (digit < 0) ? -digit : digit;
						buffer[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buffer[i]);
					}
				}

				format_modifiers = false;
				break;

			case 'u':
				if (prefix_long)
				{
					unsigned long integer = va_arg(args, unsigned long);
					int i = 0;
					char buffer[32];

					do
					{
						unsigned int digit = integer % 10;
						buffer[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buffer[i]);
					}
				}
				else
				{
					unsigned int integer = va_arg(args, unsigned int);
					int i = 0;
					char buffer[16];

					do
					{
						unsigned int digit = integer % 10;
						buffer[i++] = (char)('0' + digit);
						integer /= 10;
					} while (integer != 0);

					for (i = i - 1; i >= 0; i--)
					{
						fmt_putc(out, buffer[i]);
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
vsnprintf(char *buffer, size_t size, const char *fmt, va_list ap)
{
	fmt_buffer_t out = { .buffer = buffer, .size = size, .len = 0 };

	if (!buffer || !fmt || size == 0)
		return -1;

	vprintf_helper(&out, fmt, ap);

	buffer[(out.len < size - 1) ? out.len : size - 1] = '\0';
	return (int)out.len;
}

int
snprintf(char *buffer, size_t size, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buffer, size, fmt, ap);
	va_end(ap);
	return n;
}

int
printf(const char *fmt, ...)
{
	char buffer[4096];
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (n < 0)
		return -1;

	return write(1, buffer, strnlen(buffer, sizeof(buffer)));
}

