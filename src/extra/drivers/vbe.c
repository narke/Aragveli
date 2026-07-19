/*
 * Copyright (c) 2017 Konstantin Tcholokachvili.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#include <lib/c/string.h>

#include "vbe.h"

#define FONT_HEIGHT 16
#define LINE_HEIGHT 20

uint32_t *vram;
uint16_t pitch;
uint8_t bpp;
int screen_width;
int screen_height;
uint32_t color = NORMAL_WHITE;
uint8_t scale = 1;
uint16_t x = 0;
uint16_t y = 0;

void
vbe_set_color(uint32_t new_color)
{
	color = new_color;
}

void
vbe_set_position(uint16_t new_y, uint16_t new_x)
{
	x = new_x;
	y = new_y;
}

void
vbe_set_scale(uint8_t new_scale)
{
	scale = new_scale;
}

static void
box_fill(int x0, int y0, int x1, int y1)
{
	int y, x;

	for (y = y0; y <= y1; y++)
		for (x = x0; x <= x1; x++)
			vram[y * screen_width + x] = color;
}

static void
vbe_scroll_up(void)
{
	int row, col;
	uint32_t *dst = vram;
	uint32_t *src = vram + LINE_HEIGHT * screen_width;
	size_t row_bytes = (size_t)screen_width * sizeof(uint32_t);

	for (row = 0; row < screen_height - LINE_HEIGHT; row++)
	{
		if (memcpy_s(dst + row * screen_width, row_bytes,
				src + row * screen_width, row_bytes) == NULL)
			return;
	}

	for (row = screen_height - LINE_HEIGHT; row < screen_height; row++)
		for (col = 0; col < screen_width; col++)
			vram[row * screen_width + col] = NORMAL_BLACK;
}

void
vbe_draw_character(const char c)
{
	int i, j;

	if (c == '\n')
	{
		int next_y = (int)y + LINE_HEIGHT;

		/* Stay on this row after scroll so spacing stays LINE_HEIGHT. */
		if (next_y + FONT_HEIGHT * (int)scale <= screen_height)
			vbe_set_position((uint16_t)next_y, 0);
		else
		{
			vbe_scroll_up();
			x = 0;
		}
		return;
	}

	if (c == '\b')
	{
		if (x >= 8 * scale)
			vbe_set_position(y, (uint16_t)(x - 8 * scale));
		return;
	}

	vbe_set_position(y, (uint16_t)(x + 8 * scale));

	{
		uint32_t saved_color = color;

		color = NORMAL_BLACK;
		box_fill(x, y, x + 8 * scale - 1, y + FONT_HEIGHT * scale - 1);
		color = saved_color;
	}

	for (i = 0; i < 16; i++)
	{
		uint8_t bit = 0x80;

		for (j = 0; j < 8; j++)
		{
			uint8_t b = font[c * 16 + i];

			if ((b & bit) != 0)
				box_fill(x + j * scale,
					y + i * scale,
					x + (j + 1) * scale - 1,
					y + (i + 1) * scale - 1);
			bit >>= 1;
		}
	}
}

void
vbe_draw_string(const char *str)
{
	while (*str != '\0')
	{
		vbe_draw_character(*str);
		str++;
	}
}

void
vbe_setup(struct vbe_mode_info *vbe_mode_info)
{
	vram		= (uint32_t *)vbe_mode_info->framebuffer_addr;
	screen_width	= vbe_mode_info->x_res;
	screen_height	= vbe_mode_info->y_res;
	pitch		= vbe_mode_info->pitch / sizeof(uint32_t);
	bpp		= vbe_mode_info->bpp;

	vbe_set_color(NORMAL_WHITE);
	vbe_set_position(0, 0);
	vbe_set_scale(1);
}
