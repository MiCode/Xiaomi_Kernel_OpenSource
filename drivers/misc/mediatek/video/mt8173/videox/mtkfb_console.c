/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/font.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

#include <linux/types.h>
#include "mtkfb_console.h"
#include "ddp_hal.h"
#include "ddp_log.h"
#include "disp_drv_log.h"
/* --------------------------------------------------------------------------- */

#define MFC_WIDTH           (ctxt->fb_width)
#define MFC_HEIGHT          (ctxt->fb_height)
#define MFC_BPP             (ctxt->fb_bpp)
#define MFC_PITCH           (MFC_WIDTH * MFC_BPP)

#define MFC_FG_COLOR        (ctxt->fg_color)
#define MFC_BG_COLOR        (ctxt->bg_color)

#define MFC_FONT            font_vga_8x16
#define MFC_FONT_WIDTH      (MFC_FONT.width)
#define MFC_FONT_HEIGHT     (MFC_FONT.height)
#define MFC_FONT_DATA       (MFC_FONT.data)

#define MFC_ROW_SIZE        (MFC_FONT_HEIGHT * MFC_PITCH)
#define MFC_ROW_FIRST       ((uint8_t *)(ctxt->fb_addr))
#define MFC_ROW_SECOND      (MFC_ROW_FIRST + MFC_ROW_SIZE)
#define MFC_ROW_LAST        (MFC_ROW_FIRST + MFC_SIZE - MFC_ROW_SIZE)
#define MFC_SIZE            (MFC_ROW_SIZE * ctxt->rows)
#define MFC_SCROLL_SIZE     (MFC_SIZE - MFC_ROW_SIZE)

#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))

#define MFC_LOCK()                                                          \
	do { \
		if (down_interruptible(&ctxt->sem)) { \
			DISPMSG("[MFC] ERROR: Can't get semaphore in %s()\n",            \
			__func__);                                           \
			ASSERT(0);                                                      \
		}                                                                   \
	} while (0)

#define MFC_UNLOCK() up(&ctxt->sem)



/* --------------------------------------------------------------------------- */
uint32_t MFC_Get_Cursor_Offset(MFC_HANDLE handle)
{
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *) handle;

	uint32_t offset =
	    ctxt->cursor_col * MFC_FONT_WIDTH * MFC_BPP +
	    ctxt->cursor_row * MFC_FONT_HEIGHT * MFC_PITCH;

	return offset;
}

static void _MFC_DrawChar(MFC_CONTEXT *ctxt, uint32_t x, uint32_t y, char c)
{
	uint8_t ch = *((uint8_t *) &c);
	const uint8_t *cdat;
	uint8_t *dest;
	int32_t rows, offset;

	int font_draw_table16[4];

	ASSERT(x <= (MFC_WIDTH - MFC_FONT_WIDTH));
	ASSERT(y <= (MFC_HEIGHT - MFC_FONT_HEIGHT));

	offset = y * MFC_PITCH + x * MFC_BPP;
	dest = (MFC_ROW_FIRST + offset);

	switch (MFC_BPP) {
	case 2:
		font_draw_table16[0] = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR, MFC_BG_COLOR);
		font_draw_table16[1] = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR, MFC_FG_COLOR);
		font_draw_table16[2] = MAKE_TWO_RGB565_COLOR(MFC_FG_COLOR, MFC_BG_COLOR);
		font_draw_table16[3] = MAKE_TWO_RGB565_COLOR(MFC_FG_COLOR, MFC_FG_COLOR);

		cdat = (const uint8_t *)MFC_FONT_DATA + ch * MFC_FONT_HEIGHT;

		for (rows = MFC_FONT_HEIGHT; rows--; dest += MFC_PITCH) {
			uint8_t bits = *cdat++;

			((uint32_t *) dest)[0] = font_draw_table16[bits >> 6];
			((uint32_t *) dest)[1] = font_draw_table16[bits >> 4 & 3];
			((uint32_t *) dest)[2] = font_draw_table16[bits >> 2 & 3];
			((uint32_t *) dest)[3] = font_draw_table16[bits & 3];
		}
		break;

	default:
		ASSERT(0);
	}
}


static void _MFC_ScrollUp(MFC_CONTEXT *ctxt)
{
	const uint32_t BG_COLOR = MAKE_TWO_RGB565_COLOR(MFC_BG_COLOR, MFC_BG_COLOR);

	uint32_t *ptr = (uint32_t *) MFC_ROW_LAST;
	int i = MFC_ROW_SIZE / sizeof(uint32_t);

	memcpy(MFC_ROW_FIRST, MFC_ROW_SECOND, MFC_SCROLL_SIZE);

	while (--i >= 0)
		*ptr++ = BG_COLOR;

}


static void _MFC_Newline(MFC_CONTEXT *ctxt)
{
	/* /Bin:add for filling the color for the blank of this column */
	while (ctxt->cursor_col < ctxt->cols) {
		_MFC_DrawChar(ctxt,
			      ctxt->cursor_col * MFC_FONT_WIDTH,
			      ctxt->cursor_row * MFC_FONT_HEIGHT, ' ');

		++ctxt->cursor_col;
	}
	++ctxt->cursor_row;
	ctxt->cursor_col = 0;

	/* Check if we need to scroll the terminal */
	if (ctxt->cursor_row >= ctxt->rows) {
		/* Scroll everything up */
		_MFC_ScrollUp(ctxt);

		/* Decrement row number */
		--ctxt->cursor_row;
	}
}


#define CHECK_NEWLINE()                     \
	do { \
		if (ctxt->cursor_col >= ctxt->cols) \
			_MFC_Newline(ctxt);             \
	} while (0)

static void _MFC_Putc(MFC_CONTEXT *ctxt, const char c)
{
	CHECK_NEWLINE();

	switch (c) {
	case '\n':		/* next line */
		_MFC_Newline(ctxt);
		break;

	case '\r':		/* carriage return */
		ctxt->cursor_col = 0;
		break;

	case '\t':		/* tab 8 */
		ctxt->cursor_col += 8;
		ctxt->cursor_col &= ~0x0007;
		CHECK_NEWLINE();
		break;

	default:		/* draw the char */
		_MFC_DrawChar(ctxt,
			      ctxt->cursor_col * MFC_FONT_WIDTH,
			      ctxt->cursor_row * MFC_FONT_HEIGHT, c);
		++ctxt->cursor_col;
		CHECK_NEWLINE();
	}
}

/* --------------------------------------------------------------------------- */

MFC_STATUS MFC_Open(MFC_HANDLE *handle,
		    void *fb_addr,
		    unsigned int fb_width,
		    unsigned int fb_height,
		    unsigned int fb_bpp, unsigned int fg_color, unsigned int bg_color)
{
	MFC_CONTEXT *ctxt = NULL;

	if (NULL == handle || NULL == fb_addr)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (fb_bpp != 2)
		return MFC_STATUS_NOT_IMPLEMENTED;	/* only support RGB565 */

	ctxt = kzalloc(sizeof(MFC_CONTEXT), GFP_KERNEL);
	if (!ctxt)
		return MFC_STATUS_OUT_OF_MEMORY;

/* init_MUTEX(&ctxt->sem); */
	sema_init(&ctxt->sem, 1);
	ctxt->fb_addr = fb_addr;
	ctxt->fb_width = fb_width;
	ctxt->fb_height = fb_height;
	ctxt->fb_bpp = fb_bpp;
	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;
	ctxt->rows = fb_height / MFC_FONT_HEIGHT;
	ctxt->cols = fb_width / MFC_FONT_WIDTH;
	ctxt->font_width = MFC_FONT_WIDTH;
	ctxt->font_height = MFC_FONT_HEIGHT;

	*handle = ctxt;

	return MFC_STATUS_OK;
}

MFC_STATUS MFC_Open_Ex(MFC_HANDLE *handle,
		       void *fb_addr,
		       unsigned int fb_width,
		       unsigned int fb_height,
		       unsigned int fb_pitch,
		       unsigned int fb_bpp, unsigned int fg_color, unsigned int bg_color)
{

	MFC_CONTEXT *ctxt = NULL;

	if (NULL == handle || NULL == fb_addr)
		return MFC_STATUS_INVALID_ARGUMENT;

	if (fb_bpp != 2)
		return MFC_STATUS_NOT_IMPLEMENTED;	/* only support RGB565 */

	ctxt = kzalloc(sizeof(MFC_CONTEXT), GFP_KERNEL);
	if (!ctxt)
		return MFC_STATUS_OUT_OF_MEMORY;

/* init_MUTEX(&ctxt->sem); */
	sema_init(&ctxt->sem, 1);
	ctxt->fb_addr = fb_addr;
	ctxt->fb_width = fb_pitch;
	ctxt->fb_height = fb_height;
	ctxt->fb_bpp = fb_bpp;
	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;
	ctxt->rows = fb_height / MFC_FONT_HEIGHT;
	ctxt->cols = fb_width / MFC_FONT_WIDTH;
	ctxt->font_width = MFC_FONT_WIDTH;
	ctxt->font_height = MFC_FONT_HEIGHT;

	*handle = ctxt;

	return MFC_STATUS_OK;

}


MFC_STATUS MFC_Close(MFC_HANDLE handle)
{
	if (!handle)
		return MFC_STATUS_INVALID_ARGUMENT;

	kfree(handle);

	return MFC_STATUS_OK;
}


MFC_STATUS MFC_SetColor(MFC_HANDLE handle, unsigned int fg_color, unsigned int bg_color)
{
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *) handle;

	if (!ctxt)
		return MFC_STATUS_INVALID_ARGUMENT;

	MFC_LOCK();
	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;
	MFC_UNLOCK();

	return MFC_STATUS_OK;
}


MFC_STATUS MFC_ResetCursor(MFC_HANDLE handle)
{
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *) handle;

	if (!ctxt)
		return MFC_STATUS_INVALID_ARGUMENT;

	MFC_LOCK();
	ctxt->cursor_row = ctxt->cursor_col = 0;
	MFC_UNLOCK();

	return MFC_STATUS_OK;
}


MFC_STATUS MFC_Print(MFC_HANDLE handle, const char *str)
{
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *) handle;
	int count = 0;

	if (!ctxt || !str)
		return MFC_STATUS_INVALID_ARGUMENT;

	MFC_LOCK();

	count = strlen(str);

	while (count--)
		_MFC_Putc(ctxt, *str++);

	MFC_UNLOCK();

	return MFC_STATUS_OK;
}

MFC_STATUS MFC_SetMem(MFC_HANDLE handle, const char *str, uint32_t color)
{
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *) handle;
	int count = 0;
	int i, j;
	uint32_t *ptr;

	if (!ctxt || !str)
		return MFC_STATUS_INVALID_ARGUMENT;

	MFC_LOCK();

	count = strlen(str);
	count = count * MFC_FONT_WIDTH;

	for (j = 0; j < MFC_FONT_HEIGHT; j++) {
		ptr = (uint32_t *) (ctxt->fb_addr + (j + 1) * MFC_PITCH - count * ctxt->fb_bpp);
		for (i = 0; i < count * ctxt->fb_bpp / sizeof(uint32_t); i++)
			*ptr++ = color;

	}

	MFC_UNLOCK();

	return MFC_STATUS_OK;
}

MFC_STATUS MFC_LowMemory_Printf(MFC_HANDLE handle, const char *str, uint32_t fg_color,
				uint32_t bg_color)
{
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *) handle;
	int count = 0;
	unsigned int col, row, fg_color_mfc, bg_color_mfc;

	if (!ctxt || !str)
		return MFC_STATUS_INVALID_ARGUMENT;

	MFC_LOCK();

	count = strlen(str);
/* //store cursor_col and row for printf low memory char temply */
	row = ctxt->cursor_row;
	col = ctxt->cursor_col;
	ctxt->cursor_row = 0;
	ctxt->cursor_col = ctxt->cols - count;
	fg_color_mfc = ctxt->fg_color;
	bg_color_mfc = ctxt->bg_color;

/* ///////// */
	ctxt->fg_color = fg_color;
	ctxt->bg_color = bg_color;
	while (count--)
		_MFC_Putc(ctxt, *str++);

/* //restore cursor_col and row for printf low memory char temply */
	ctxt->cursor_row = row;
	ctxt->cursor_col = col;
	ctxt->fg_color = fg_color_mfc;
	ctxt->bg_color = bg_color_mfc;
/* ///////// */


	MFC_UNLOCK();

	return MFC_STATUS_OK;
}
